// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/observation_delay_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/observation_delay_metrics.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/form_predictions_tracker.h"
#include "components/page_content_annotations/content/browser/page_settled_monitor.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

namespace {

using ::content::RenderFrameHost;
using ::content::WebContents;
using ::content::WebContentsObserver;

using PageSettledMonitorState =
    ::page_content_annotations::PageSettledMonitor::State;

// The timeout to wait for Autofill to parse and classify form fields.
// It's autofill's `FormPredictionsTracker`'s responsibility to respect this
// timeout.
base::TimeDelta GetAutofillPredictionsTimeout() {
  return features::kActorObservationDelayAutofillPredictionsTimeout.Get();
}

// This should be similar to the number of redirects.
constexpr size_t kMaxNavigations = 20;

}  // namespace

// Implementation of PageSettledMonitor::Delegate for Actor.
class PageSettledMonitorDelegate
    : public page_content_annotations::PageSettledMonitor::Delegate {
 public:
  PageSettledMonitorDelegate(
      ObservationDelayController& controller,
      TaskId task_id,
      std::optional<ObservationDelayController::PageStabilityConfig>
          page_stability_config)
      : page_content_annotations::PageSettledMonitor::Delegate(
            std::move(page_stability_config)),
        controller_(controller),
        task_id_(task_id) {}

  ~PageSettledMonitorDelegate() override = default;

  // page_content_annotations::PageSettledMonitor::Delegate:
  void WillMoveToState(PageSettledMonitorState state) override {
    controller_->WillMoveToState(state);
  }

  void OnMilestoneReached(
      page_content_annotations::PageSettledMonitor::Milestone milestone,
      base::OnceClosure resume_callback) override {
    controller_->OnMilestoneReached(milestone, std::move(resume_callback));
  }

  void OnEvent(
      page_content_annotations::PageSettledMonitor::Event event) override {
    controller_->OnEvent(event);
  }

  mojo::PendingRemote<page_content_annotations::mojom::PageStabilityMonitor>
  CreatePageStabilityMonitor(content::RenderFrameHost* target_frame) override {
    if (!target_frame || !page_stability_config_.has_value()) {
      return mojo::NullRemote();
    }

    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    target_frame->GetRemoteAssociatedInterfaces()->GetInterface(
        &chrome_render_frame);

    mojo::PendingRemote<page_content_annotations::mojom::PageStabilityMonitor>
        monitor;
    chrome_render_frame->CreatePageStabilityMonitor(
        monitor.InitWithNewPipeAndPassReceiver(), task_id_,
        page_stability_config_->supports_paint_stability);
    return monitor;
  }

  bool ShouldExcludeAdSubframes() const override {
    return base::FeatureList::IsEnabled(
        features::kGlicActorObservationDelayExcludeAdFrameLoading);
  }

  bool ShouldSkipVisualStateUpdateForHiddenTabs() const override {
    return base::FeatureList::IsEnabled(
        actor::kGlicSkipAwaitVisualStateForNewTabs);
  }

  base::TimeDelta GetLcpDelay() const override {
    return features::kActorObservationDelayLcp.Get();
  }

  base::TimeDelta GetCompletionTimeout() const override {
    return features::kActorObservationDelayTimeout.Get();
  }

 private:
  base::raw_ref<ObservationDelayController> controller_;
  TaskId task_id_;
};

ObservationDelayController::ObservationDelayController(
    content::RenderFrameHost& target_frame,
    TaskId task_id,
    AggregatedJournal& journal,
    PageStabilityConfig page_stability_config)
    : content::WebContentsObserver(
          WebContents::FromRenderFrameHost(&target_frame)),
      journal_(journal),
      task_id_(task_id) {
  CHECK(web_contents());
  journal.Log(
      GURL::EmptyGURL(), task_id, "ObservationDelay: Created",
      JournalDetailsBuilder().Add("May Use PageStability", true).Build());

  journal.EnsureJournalBound(target_frame);

  // Note: It's important that the PageStabilityMonitor be created on the same
  // interface as tool invocation since it relies on being created before a
  // tool is invoked.
  page_settled_monitor_ =
      std::make_unique<page_content_annotations::PageSettledMonitor>(
          &target_frame, std::make_unique<PageSettledMonitorDelegate>(
                             *this, task_id, std::move(page_stability_config)));
}

ObservationDelayController::ObservationDelayController(
    TaskId task_id,
    AggregatedJournal& journal)
    : journal_(journal), task_id_(task_id) {
  journal.Log(
      GURL::EmptyGURL(), task_id, "ObservationDelay: Created",
      JournalDetailsBuilder().Add("May Use PageStability", false).Build());

  page_settled_monitor_ =
      std::make_unique<page_content_annotations::PageSettledMonitor>(
          /*target_frame=*/nullptr,
          std::make_unique<PageSettledMonitorDelegate>(
              *this, task_id,
              /*page_stability_config=*/std::nullopt));
}

ObservationDelayController::~ObservationDelayController() = default;

void ObservationDelayController::Wait(tabs::TabInterface& target_tab,
                                      ReadyCallback callback) {
  CHECK_EQ(state_, State::kInitial);
  CHECK(callback);

  CHECK(!ready_callback_);
  ready_callback_ = std::move(callback);

  metrics_ = std::make_unique<ObservationDelayMetrics>();
  metrics_->Start();

  WebContents* web_contents = target_tab.GetContents();
  WebContentsObserver::Observe(web_contents);

  wait_journal_entry_ = journal_->CreatePendingAsyncEntry(
      GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
      "ObservationDelay: Wait", {});

  page_settled_monitor_->Wait(
      web_contents, base::BindOnce(&ObservationDelayController::OnPageSettled,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ObservationDelayController::OnFederatedLoginRequestComplete(
    base::OnceClosure resume_callback) {
  if (state_ != State::kWaitForFederatedLogin) {
    return;
  }

  CHECK(metrics_);
  metrics_->OnFederatedLoginRequestComplete();

  CHECK(resume_callback);
  std::move(resume_callback).Run();
}

void ObservationDelayController::OnAutofillPredictionsFinished(
    base::OnceClosure resume_callback) {
  if (state_ != State::kWaitForAutofillPredictions) {
    return;
  }

  CHECK(metrics_);
  metrics_->OnAutofillPredictionsFinished();

  CHECK(resume_callback);
  std::move(resume_callback).Run();
}

void ObservationDelayController::MoveToState(State new_state) {
  if (state_ == State::kDone) {
    return;
  }

  CHECK(metrics_);
  metrics_->WillMoveToState(new_state);

  DCheckStateTransition(state_, new_state);

  inner_journal_entry_.reset();
  journal_->Log(GURL::EmptyGURL(), task_id_, "ObservationDelay: State Change",
                JournalDetailsBuilder()
                    .Add("old_state", StateToString(state_))
                    .Add("new_state", StateToString(new_state))
                    .Build());

  SetState(new_state);

  switch (state_) {
    case State::kInitial: {
      NOTREACHED();
    }
    case State::kWaitForPageStability:
      break;
    case State::kPageStabilityMonitorDisconnected:
      break;
    case State::kWaitForFederatedLogin: {
      CHECK(resume_callback_);

      if (!base::FeatureList::IsEnabled(
              features::kFedCmEmbedderInitiatedLogin)) {
        std::move(resume_callback_).Run();
        break;
      }
      auto* request =
          content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
      if (!request) {
        std::move(resume_callback_).Run();
        break;
      }
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "WaitForFederatedLogin",
          JournalDetailsBuilder()
              .Add("idp_origin", request->idp_origin())
              .Build());
      federated_login_subscription_ =
          request->RegisterCompletion(base::BindOnce(
              &ObservationDelayController::OnFederatedLoginRequestComplete,
              base::Unretained(this), std::move(resume_callback_)));
      break;
    }
    case State::kWaitForLoadCompletion: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "WaitForLoadCompletion", {});
      break;
    }
    case State::kWaitForVisualStateUpdate: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "WaitForVisualStateUpdate", {});
      break;
    }
    case State::kMaybeDelayForLcp: {
      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "MaybeDelayForLcp", {});
      break;
    }
    case State::kDelayForLcp: {
      break;
    }
    case State::kWaitForAutofillPredictions: {
      CHECK(resume_callback_);

      inner_journal_entry_ = journal_->CreatePendingAsyncEntry(
          GURL::EmptyGURL(), task_id_, MakeBrowserTrackUUID(task_id_),
          "WaitForAutofillPredictions", {});

      autofill::ContentAutofillClient* autofill_client =
          autofill::ContentAutofillClient::FromWebContents(web_contents());
      if (!autofill_client) {
        std::move(resume_callback_).Run();
        break;
      }
      autofill::FormPredictionsTracker* tracker =
          autofill_client->GetFormPredictionsTracker();
      if (!tracker) {
        std::move(resume_callback_).Run();
        break;
      }
      tracker->Wait(
          base::BindOnce(
              &ObservationDelayController::OnAutofillPredictionsFinished,
              weak_ptr_factory_.GetWeakPtr(), std::move(resume_callback_)),
          GetAutofillPredictionsTimeout());
      break;
    }
    case State::kPageNavigated: {
      result_ = Result::kPageNavigated;
      // Stop monitoring.
      page_settled_monitor_.reset();
      MoveToState(State::kDone);
      break;
    }
    case State::kDidTimeout: {
      if (base::FeatureList::IsEnabled(
              features::kFedCmEmbedderInitiatedLogin)) {
        if (auto* request = content::webid::FederatedEmbedderLoginRequest::Get(
                web_contents())) {
          // We are no longer willing to wait for the federated login request.
          // Consider it a failure. Note that this will treat the tool as having
          // failed, since we don't want to confuse an abandoned request as
          // being successful.
          request->OnFederatedResultReceived(
              content::webid::FederatedLoginResult::kTimeoutByEmbedder);
        }
      }
      break;
    }
    case State::kDone: {
      // The state machine is never entered until Wait is called so a callback
      // must be provided.
      CHECK(ready_callback_);
      wait_journal_entry_.reset();
      federated_login_subscription_ = {};
      resume_callback_.Reset();
      PostFinishedTask(
          base::BindOnce([](ReadyCallback callback,
                            Result result) { std::move(callback).Run(result); },
                         std::move(ready_callback_), result_));
      break;
    }
  }
}

void ObservationDelayController::OnPageSettled() {
  MoveToState(State::kDone);
}

void ObservationDelayController::WillMoveToState(
    PageSettledMonitorState state) {
  // Map generic state to Actor state for metrics and logging.
  State actor_state;
  switch (state) {
    case PageSettledMonitorState::kInitial:
      NOTREACHED();
    case PageSettledMonitorState::kWaitForPageStability:
      actor_state = State::kWaitForPageStability;
      break;
    case PageSettledMonitorState::kPageStabilityMonitorDisconnected:
      actor_state = State::kPageStabilityMonitorDisconnected;
      break;
    case PageSettledMonitorState::kWaitForLoadCompletion:
      actor_state = State::kWaitForLoadCompletion;
      break;
    case PageSettledMonitorState::kWaitForVisualStateUpdate:
      actor_state = State::kWaitForVisualStateUpdate;
      break;
    case PageSettledMonitorState::kMaybeDelayForLcp:
      actor_state = State::kMaybeDelayForLcp;
      break;
    case PageSettledMonitorState::kDelayForLcp:
      actor_state = State::kDelayForLcp;
      break;
    case PageSettledMonitorState::kDidTimeout:
      actor_state = State::kDidTimeout;
      break;
    case PageSettledMonitorState::kDone:
      // kDone is handled via OnPageSettled().
      return;
  }

  MoveToState(actor_state);
}

void ObservationDelayController::OnMilestoneReached(
    page_content_annotations::PageSettledMonitor::Milestone milestone,
    base::OnceClosure resume_callback) {
  if (state_ == State::kDone) {
    return;
  }

  CHECK(resume_callback);
  CHECK(!resume_callback_);
  resume_callback_ = std::move(resume_callback);

  switch (milestone) {
    case page_content_annotations::PageSettledMonitor::Milestone::
        kPageStability:
      // Once the page is stable (network/main thread idle), we check if there's
      // a pending Federated Login request to wait for.
      PostMoveToStateClosure(State::kWaitForFederatedLogin).Run();
      break;
    case page_content_annotations::PageSettledMonitor::Milestone::
        kLoadCompletion:
      // Just resume the monitor.
      std::move(resume_callback_).Run();
      break;
    case page_content_annotations::PageSettledMonitor::Milestone::
        kVisualStateUpdate:
      // Just resume the monitor.
      std::move(resume_callback_).Run();
      break;
    case page_content_annotations::PageSettledMonitor::Milestone::kLcpSettled:
      // After LCP has settled, we wait for Autofill to finish its field
      // classification, as tool-use often depends on having accurate form
      // metadata.
      PostMoveToStateClosure(State::kWaitForAutofillPredictions).Run();
      break;
  }
}

void ObservationDelayController::OnEvent(
    page_content_annotations::PageSettledMonitor::Event event) {
  if (state_ == State::kDone) {
    return;
  }

  switch (event) {
    case page_content_annotations::PageSettledMonitor::Event::kPageStabilized:
      if (metrics_) {
        metrics_->OnPageStable();
      }
      break;
    case page_content_annotations::PageSettledMonitor::Event::kLoadCompleted:
      if (metrics_) {
        metrics_->OnLoadCompleted();
      }
      break;
    case page_content_annotations::PageSettledMonitor::Event::
        kVisualStateUpdated:
      if (metrics_) {
        metrics_->OnVisualStateUpdated();
      }
      break;
    case page_content_annotations::PageSettledMonitor::Event::kMojoDisconnected:
      if (state_ == State::kInitial) {
        journal_->Log(GURL::EmptyGURL(), task_id_,
                      "ObservationDelay: Monitor Disconnect Before Wait", {});
      }
      break;
    case page_content_annotations::PageSettledMonitor::Event::
        kVisualStateUpdateSkipped:
      journal_->Log(
          web_contents()->GetLastCommittedURL(), task_id_,
          "ObservationDelay: Skip visual state update of non-captured tab", {});
      break;
  }
}

std::ostream& operator<<(std::ostream& o,
                         const ObservationDelayController::State& state) {
  return o << ObservationDelayController::StateToString(state);
}

void ObservationDelayController::DCheckStateTransition(State old_state,
                                                       State new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          // clang-format off
          {State::kInitial,
              {State::kWaitForPageStability,
               State::kDone}},
          {State::kWaitForPageStability,
              {State::kWaitForFederatedLogin,
               State::kPageStabilityMonitorDisconnected,
               State::kDidTimeout,
               State::kDone,
               State::kPageNavigated}},
          {State::kPageStabilityMonitorDisconnected,
              {State::kWaitForFederatedLogin,
               State::kDidTimeout,
               State::kDone,
               State::kPageNavigated}},
          {State::kWaitForFederatedLogin,
              {State::kWaitForLoadCompletion,
               State::kDidTimeout,
               State::kDone,
               State::kPageNavigated}},
          {State::kWaitForLoadCompletion,
              {State::kDidTimeout,
               State::kDone,
               State::kPageNavigated,
               State::kWaitForVisualStateUpdate}},
          {State::kWaitForVisualStateUpdate,
              {State::kDidTimeout,
               State::kDone,
               State::kPageNavigated,
               State::kMaybeDelayForLcp}},
          {State::kMaybeDelayForLcp,
              {State::kDidTimeout,
               State::kDone,
               State::kPageNavigated,
               State::kDelayForLcp,
               State::kWaitForAutofillPredictions}},
          {State::kDelayForLcp,
              {State::kDidTimeout,
               State::kDone,
               State::kPageNavigated,
               State::kWaitForAutofillPredictions}},
          {State::kWaitForAutofillPredictions,
              {State::kDidTimeout,
               State::kPageNavigated,
               State::kDone}},
          {State::kDidTimeout,
              {State::kDone}},
          {State::kPageNavigated,
              {State::kDone}}
          // clang-format on
      }));
  DCHECK_STATE_TRANSITION(transitions, old_state, new_state);
#endif  // DCHECK_IS_ON()
}

void ObservationDelayController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame() || state_ == State::kInitial) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          kActorRestartObservationDelayControllerOnNavigate)) {
    return;
  }
  // If we exceed the number of navigations just keep waiting for observations.
  if (navigation_count_ >= kMaxNavigations) {
    return;
  }
  MoveToState(State::kPageNavigated);
}

void ObservationDelayController::SetState(State state) {
  state_ = state;
}

std::string_view ObservationDelayController::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kWaitForPageStability:
      return "WaitForPageStability";
    case State::kPageStabilityMonitorDisconnected:
      return "PageStabilityMonitorDisconnected";
    case State::kWaitForFederatedLogin:
      return "WaitForFederatedLogin";
    case State::kWaitForLoadCompletion:
      return "WaitForLoadCompletion";
    case State::kWaitForVisualStateUpdate:
      return "WaitForVisualStateUpdate";
    case State::kMaybeDelayForLcp:
      return "MaybeDelayForLcp";
    case State::kDelayForLcp:
      return "DelayForLcp";
    case State::kWaitForAutofillPredictions:
      return "WaitForAutofillPredictions";
    case State::kDidTimeout:
      return "DidTimeout";
    case State::kPageNavigated:
      return "PageNavigated";
    case State::kDone:
      return "Done";
  }
  NOTREACHED();
}

base::OnceClosure ObservationDelayController::MoveToStateClosure(
    State new_state) {
  return base::BindOnce(&ObservationDelayController::MoveToState,
                        weak_ptr_factory_.GetWeakPtr(), new_state);
}

base::OnceClosure ObservationDelayController::PostMoveToStateClosure(
    State new_state,
    base::TimeDelta delay) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::OnceClosure task, base::TimeDelta delay) {
        task_runner->PostDelayedTask(FROM_HERE, std::move(task), delay);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      MoveToStateClosure(new_state), delay);
}

size_t ObservationDelayController::NavigationCount() const {
  return navigation_count_;
}

void ObservationDelayController::SetNavigationCount(size_t count) {
  navigation_count_ = count;
}

}  // namespace actor
