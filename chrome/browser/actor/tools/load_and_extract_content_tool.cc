// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/load_and_extract_content_tool.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using TabObservation = ::optimization_guide::proto::TabObservation;

namespace actor {

namespace {

// TODO(b/484078735): Remove this once we can hook into existing tab
// observation infra.
void FillInTabObservationMetadata(
    const blink::mojom::PageMetadataPtr& mojom_metadata,
    TabObservation& tab_observation) {
  if (!mojom_metadata) {
    return;
  }

  auto* proto_metadata = tab_observation.mutable_metadata();
  for (const auto& mojom_frame_metadata : mojom_metadata->frame_metadata) {
    auto* proto_frame_metadata = proto_metadata->add_frame_metadata();
    proto_frame_metadata->set_url(mojom_frame_metadata->url.spec());
    for (const auto& mojom_meta_tag : mojom_frame_metadata->meta_tags) {
      auto* proto_meta_tag = proto_frame_metadata->add_meta_tags();
      proto_meta_tag->set_name(mojom_meta_tag->name);
      proto_meta_tag->set_content(mojom_meta_tag->content);
    }
  }
}

void OnValidatedAllUrls(ToolCallback overall_callback,
                        std::vector<mojom::ActionResultPtr> results) {
  for (auto& result : results) {
    // In the case of multiple errors, we arbitrarily return the first.
    if (!IsOk(result->code)) {
      PostResponseTask(std::move(overall_callback), std::move(result));
      return;
    }
  }
  PostResponseTask(std::move(overall_callback), MakeOkResult());
}

mojom::ActionResultPtr LogIfValidationFailed(AggregatedJournal& journal,
                                             TaskId task_id,
                                             const GURL& url,
                                             mojom::ActionResultPtr result) {
  if (!IsOk(result->code)) {
    journal.Log(url, task_id, "LoadAndExtractContentTool::ValidateUrlFailed",
                JournalDetailsBuilder()
                    .Add("action_result_code", static_cast<int>(result->code))
                    .Build());
  }
  return result;
}

}  // namespace

// The internal result for a single tab.
enum class LoadAndExtractContentTool::PerTabResultCode {
  kOk,
  kNewTabCreationFailed,
  kNavigateCommittedErrorPage,
  kLoadAndExtractContentExtractionFailed,
  kTabWentAway,
  kWindowWentAway,
  kToolTimeout,
};

// Manages waiting for a tab to finish navigating and then delegation to the
// `ObservationDelayController` to wait for page stability. Begins waiting after
// initial navigation to about:blank is complete. Invokes the callback once the
// page is stable (or an error occurs).
class LoadAndExtractContentTool::TabObservationDelayer
    : public content::WebContentsObserver {
 public:
  TabObservationDelayer(
      content::WebContents* web_contents,
      TaskId task_id,
      AggregatedJournal& journal,
      ObservationDelayController::PageStabilityConfig page_stability_config,
      bool target_is_about_blank,
      base::OnceCallback<void(LoadAndExtractContentTool::PerTabResultCode)>
          callback);
  ~TabObservationDelayer() override = default;

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnObservationDelayComplete(ObservationDelayController::Result result);
  void OnTimeout();

  TaskId task_id_;
  AggregatedJournal& journal_;
  ObservationDelayController::PageStabilityConfig page_stability_config_;
  const bool target_is_about_blank_;
  base::OnceCallback<void(LoadAndExtractContentTool::PerTabResultCode)>
      callback_;

  std::unique_ptr<ObservationDelayController> observation_delay_controller_;
  bool has_finished_main_frame_navigation_ = false;
  base::OneShotTimer timeout_timer_;
  base::WeakPtrFactory<TabObservationDelayer> weak_ptr_factory_{this};
};

LoadAndExtractContentTool::TabObservationDelayer::TabObservationDelayer(
    content::WebContents* web_contents,
    TaskId task_id,
    AggregatedJournal& journal,
    ObservationDelayController::PageStabilityConfig page_stability_config,
    bool target_is_about_blank,
    base::OnceCallback<void(PerTabResultCode)> callback)
    : content::WebContentsObserver(web_contents),
      task_id_(task_id),
      journal_(journal),
      page_stability_config_(page_stability_config),
      target_is_about_blank_(target_is_about_blank),
      callback_(std::move(callback)) {
  observation_delay_controller_ = std::make_unique<ObservationDelayController>(
      *web_contents->GetPrimaryMainFrame(), task_id_, journal_,
      page_stability_config_);
  timeout_timer_.Start(FROM_HERE,
                       kGlicActorLoadAndExtractContentToolTimeout.Get(),
                       base::BindOnce(&TabObservationDelayer::OnTimeout,
                                      base::Unretained(this)));
}

void LoadAndExtractContentTool::TabObservationDelayer::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  if (has_finished_main_frame_navigation_) {
    // Only wait for the first main frame navigation to start the
    // ObservationDelayController.
    return;
  }

  if (!target_is_about_blank_ &&
      navigation_handle->GetURL() == GURL(url::kAboutBlankURL)) {
    // Don't count the initial about:blank navigation (unless the target is
    // about:blank).
    return;
  }

  has_finished_main_frame_navigation_ = true;

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    journal_.Log(navigation_handle->GetURL(), task_id_,
                 "LoadAndExtractContentTool::TabObservationDelayer::"
                 "DidFinishNavigationFailed",
                 JournalDetailsBuilder()
                     .Add("navigation_id", navigation_handle->GetNavigationId())
                     .Add("is_error_page", navigation_handle->IsErrorPage())
                     .Build());
    if (callback_) {
      std::move(callback_).Run(PerTabResultCode::kNavigateCommittedErrorPage);
    }
    return;
  }

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab) {
    if (callback_) {
      std::move(callback_).Run(PerTabResultCode::kTabWentAway);
    }
    return;
  }

  observation_delay_controller_->Wait(
      *tab, base::BindOnce(&TabObservationDelayer::OnObservationDelayComplete,
                           weak_ptr_factory_.GetWeakPtr()));
}

void LoadAndExtractContentTool::TabObservationDelayer::OnTimeout() {
  if (callback_) {
    std::move(callback_).Run(PerTabResultCode::kToolTimeout);
  }
}

void LoadAndExtractContentTool::TabObservationDelayer::
    OnObservationDelayComplete(ObservationDelayController::Result result) {
  switch (result) {
    case ObservationDelayController::Result::kOk:
      if (callback_) {
        std::move(callback_).Run(PerTabResultCode::kOk);
      }
      return;
    case ObservationDelayController::Result::kPageNavigated: {
      if (tabs::TabInterface* tab =
              tabs::TabInterface::GetFromContents(web_contents())) {
        size_t last_navigation_count =
            observation_delay_controller_->NavigationCount();
        // The page navigated, restart the observation.
        observation_delay_controller_ =
            std::make_unique<ObservationDelayController>(
                *tab->GetContents()->GetPrimaryMainFrame(), task_id_, journal_,
                page_stability_config_);
        observation_delay_controller_->SetNavigationCount(
            last_navigation_count + 1);
        observation_delay_controller_->Wait(
            *tab,
            base::BindOnce(&TabObservationDelayer::OnObservationDelayComplete,
                           weak_ptr_factory_.GetWeakPtr()));
      } else {
        if (callback_) {
          std::move(callback_).Run(PerTabResultCode::kTabWentAway);
        }
        return;
      }
    }
  }
}

TabObservation::TabObservationResult
LoadAndExtractContentTool::ToTabObservationResult(
    LoadAndExtractContentTool::PerTabResultCode result_code) {
  switch (result_code) {
    case PerTabResultCode::kOk:
      return TabObservation::TAB_OBSERVATION_OK;
    case PerTabResultCode::kNewTabCreationFailed:
      // New tab creation failure results in no tab observation.
      NOTREACHED();
    case PerTabResultCode::kNavigateCommittedErrorPage:
      return TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR;
    case PerTabResultCode::kLoadAndExtractContentExtractionFailed:
      return TabObservation::TAB_OBSERVATION_FETCH_ERROR;
    case PerTabResultCode::kTabWentAway:
    case PerTabResultCode::kWindowWentAway:
      return TabObservation::TAB_OBSERVATION_TAB_WENT_AWAY;
    case PerTabResultCode::kToolTimeout:
      return TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR;
    default:
      // This switch should be exhaustive.
      NOTREACHED();
  }
}

mojom::ActionResultCode LoadAndExtractContentTool::ToActionResultCode(
    LoadAndExtractContentTool::PerTabResultCode result_code) {
  switch (result_code) {
    case PerTabResultCode::kNewTabCreationFailed:
      return mojom::ActionResultCode::kNewTabCreationFailed;
    case PerTabResultCode::kOk:
    case PerTabResultCode::kNavigateCommittedErrorPage:
    case PerTabResultCode::kLoadAndExtractContentExtractionFailed:
    case PerTabResultCode::kTabWentAway:
    case PerTabResultCode::kWindowWentAway:
    case PerTabResultCode::kToolTimeout:
      // As long as tab creation succeeds, ignore all subsequent failures in the
      // overall result code so that we don't prevent any further actions or
      // drop successful tab observations. See also b/409333494.
      return mojom::ActionResultCode::kOk;
    default:
      // This switch should be exhaustive.
      NOTREACHED();
  }
}

struct LoadAndExtractContentTool::PerTabState {
  const GURL url;
  tabs::TabHandle tab_handle;
  std::unique_ptr<LoadAndExtractContentTool::TabObservationDelayer>
      tab_observation_delayer;

  // Only empty if the tab was never opened.
  std::optional<TabObservation> tab_observation;

  // After invocation for this tab, it should have a kOk result code if all
  // phases succeeded, otherwise it should be the first error encountered.
  std::optional<PerTabResultCode> result_code = std::nullopt;
};

LoadAndExtractContentTool::LoadAndExtractContentTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    SessionID window_id,
    base::span<const GURL> urls)
    : Tool(task_id, tool_delegate),
      window_id_(window_id),
      per_tab_state_(base::ToVector(urls, [](const GURL& url) {
        return PerTabState{.url = url};
      })) {
  per_url_completion_closure_ = base::BarrierClosure(
      per_tab_state_.size(),
      base::BindOnce(&LoadAndExtractContentTool::OnAllUrlsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

LoadAndExtractContentTool::~LoadAndExtractContentTool() {
  // In the case of a tool being destroyed before completion, ensure we close
  // any remaining opened tabs.
  for (auto& per_tab_state : per_tab_state_) {
    if (tabs::TabInterface* tab = per_tab_state.tab_handle.Get()) {
      tab->Close();
    }
  }
}

void LoadAndExtractContentTool::Validate(ToolCallback overall_callback) {
  // TODO(b/478282022): Consider imposing a limit on the number of URLs.
  if (per_tab_state_.empty()) {
    journal().Log(GURL::EmptyGURL(), task_id(),
                  "LoadAndExtractContentTool::EmptyUrlsFailedValidation", {});
    PostResponseTask(std::move(overall_callback),
                     MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  // TODO(b/478282022): If any URL is invalid, we could fail immediately instead
  // of waiting for all URLs to return.
  base::RepeatingCallback<void(mojom::ActionResultPtr)> per_url_callback =
      base::BarrierCallback<mojom::ActionResultPtr>(
          per_tab_state_.size(),
          base::BindOnce(&OnValidatedAllUrls, std::move(overall_callback)));

  for (const auto& state : per_tab_state_) {
    ValidateUrlIsAcceptableNavigationDestination(
        state.url, tool_delegate(),
        base::BindOnce(&LogIfValidationFailed, std::ref(journal()), task_id(),
                       state.url)
            .Then(per_url_callback));
  }
}

void LoadAndExtractContentTool::Invoke(ToolCallback callback) {
  CHECK(!invoke_callback_);
  invoke_callback_ = std::move(callback);

  journal().Log(
      GURL::EmptyGURL(), task_id(), "LoadAndExtractContentTool::Invoke",
      JournalDetailsBuilder().Add("url_count", per_tab_state_.size()).Build());

  BrowserWindowInterface* browser_window_interface =
      BrowserWindowInterface::FromSessionID(window_id_);

  if (!browser_window_interface) {
    journal().Log(GURL::EmptyGURL(), task_id(),
                  "LoadAndExtractContentTool::WindowWentAway", {});
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }

  const GURL about_blank(url::kAboutBlankURL);
  for (size_t i = 0; i < per_tab_state_.size(); ++i) {
    PerTabState& per_tab_state = per_tab_state_.at(i);
    const GURL& url = per_tab_state.url;

    constexpr int kIndexAppendToEnd = -1;
    // We start the tab on about:blank and ensure the tab is "controlled" before
    // performing the "real" navigation, to avoid racing with the navigation
    // itself.
    content::WebContents* web_contents = chrome::AddAndReturnTabAt(
        browser_window_interface->GetBrowserForMigrationOnly(), about_blank,
        kIndexAppendToEnd, /*foreground=*/false);
    if (!web_contents) {
      journal().Log(url, task_id(),
                    "LoadAndExtractContentTool::NewTabCreationFailed",
                    JournalDetailsBuilder().Add("url_index", i).Build());
      per_tab_state.result_code = PerTabResultCode::kNewTabCreationFailed;
      per_url_completion_closure_.Run();
      continue;
    }

    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
    tabs::TabHandle tab_handle = tab->GetHandle();

    journal().Log(url, task_id(), "LoadAndExtractContentTool::TabCreated",
                  JournalDetailsBuilder().Add("url_index", i).Build());
    CHECK(page_stability_config_.has_value());

    TabObservation tab_observation;
    tab_observation.set_id(tab_handle.raw_value());

    // This tool doesn't support screenshots, so set it to error.
    // TODO(b/478282022): Consider adding a new enum value for this case.
    tab_observation.set_screenshot_result(TabObservation::SCREENSHOT_ERROR);

    const bool target_is_about_blank = url == about_blank;
    per_tab_state.tab_handle = tab_handle;
    per_tab_state.tab_observation_delayer =
        std::make_unique<TabObservationDelayer>(
            web_contents, task_id(), journal(), page_stability_config_.value(),
            target_is_about_blank,
            base::BindOnce(
                &LoadAndExtractContentTool::OnTabObservationDelayComplete,
                weak_ptr_factory_.GetWeakPtr(), i));
    per_tab_state.tab_observation = std::move(tab_observation);
    tool_delegate().AddTab(tab_handle,
                           /*stop_task_on_detach=*/false, base::DoNothing());
    CHECK(tool_delegate().HasTab(tab_handle));

    if (target_is_about_blank) {
      // No need to issue another navigation, since the tab is already
      // navigating to about:blank.
      return;
    }

    content::NavigationController::LoadURLParams params(url);
    params.transition_type = ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
    params.initiator_origin = url::Origin();
    tab->GetContents()->GetController().LoadURLWithParams(params);
  }
}

void LoadAndExtractContentTool::OnTabObservationDelayComplete(
    size_t index,
    PerTabResultCode result_code) {
  PerTabState& per_tab_state = per_tab_state_.at(index);
  if (result_code != PerTabResultCode::kOk) {
    journal().Log(per_tab_state.url, task_id(),
                  "LoadAndExtractContentTool::OnTabObservationDelayComplete",
                  JournalDetailsBuilder()
                      .Add("per_tab_result_code", static_cast<int>(result_code))
                      .Build());
    PostFinishedTask(
        base::BindOnce(&LoadAndExtractContentTool::OnTabReadyToClose,
                       weak_ptr_factory_.GetWeakPtr(), index, result_code));
    return;
  }

  // TODO(b/484078735): Consider hooking into existing tab observation
  // infrastructure rather than re-implementing it here. This would require
  // extending the existing infra to support multiple tabs and to support a new
  // 'finalization' phase after observation where we can close the tabs.
  tabs::TabInterface* tab = per_tab_state.tab_handle.Get();
  if (!tab) {
    per_tab_state.result_code = PerTabResultCode::kTabWentAway;
    per_url_completion_closure_.Run();
    return;
  }

  optimization_guide::GetAIPageContent(
      tab->GetContents(),
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true),
      base::BindOnce(&LoadAndExtractContentTool::OnGotAIPageContent,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void LoadAndExtractContentTool::OnGotAIPageContent(
    size_t index,
    optimization_guide::AIPageContentResultOrError result_or_error) {
  PerTabResultCode result_code = PerTabResultCode::kOk;

  TabObservation& tab_observation =
      per_tab_state_.at(index).tab_observation.value();

  if (result_or_error.has_value()) {
    tab_observation.set_annotated_page_content_result(
        TabObservation::ANNOTATED_PAGE_CONTENT_OK);
    *tab_observation.mutable_annotated_page_content() =
        std::move(result_or_error->proto);
    FillInTabObservationMetadata(result_or_error->metadata, tab_observation);
  } else {
    journal().Log(GURL::EmptyGURL(), task_id(),
                  "LoadAndExtractContentTool::OnGotAIPageContentFailed",
                  JournalDetailsBuilder().Add("url_index", index).Build());
    tab_observation.set_annotated_page_content_result(
        TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
    result_code = PerTabResultCode::kLoadAndExtractContentExtractionFailed;
  }
  PostFinishedTask(base::BindOnce(&LoadAndExtractContentTool::OnTabReadyToClose,
                                  weak_ptr_factory_.GetWeakPtr(), index,
                                  result_code));
}

void LoadAndExtractContentTool::OnTabReadyToClose(
    size_t index,
    PerTabResultCode result_code) {
  if (result_code != PerTabResultCode::kOk) {
    journal().Log(GURL::EmptyGURL(), task_id(),
                  "LoadAndExtractContentTool::OnTabReadyToCloseWithError",
                  JournalDetailsBuilder()
                      .Add("url_index", index)
                      .Add("per_tab_result_code", static_cast<int>(result_code))
                      .Build());
  }

  PerTabState& state = per_tab_state_.at(index);
  if (tabs::TabInterface* tab = state.tab_handle.Get()) {
    // Remove the tab from the set of controlled tabs before closing it, to
    // avoid tests thinking that something went wrong.
    tool_delegate().RemoveTab(state.tab_handle);
    tab->Close();
  } else if (!state.tab_observation->has_annotated_page_content()) {
    // Record an error code if the tab was closed before we could extract the
    // APC.
    result_code = PerTabResultCode::kTabWentAway;
  }

  state.result_code = result_code;
  per_url_completion_closure_.Run();
}

void LoadAndExtractContentTool::OnAllUrlsCompleted() {
  if (!invoke_callback_) {
    return;
  }

  // Update all tab observations with the final results, even if some tabs
  // encountered errors.
  for (auto& per_tab_state : per_tab_state_) {
    if (per_tab_state.tab_observation.has_value()) {
      TabObservation& tab_observation = per_tab_state.tab_observation.value();

      bool has_apc = tab_observation.has_annotated_page_content();
      tab_observation.set_annotated_page_content_result(
          has_apc ? TabObservation::ANNOTATED_PAGE_CONTENT_OK
                  : TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);

      tab_observation.set_result(
          ToTabObservationResult(per_tab_state.result_code.value()));
    }
  }

  for (size_t i = 0; i < per_tab_state_.size(); ++i) {
    const PerTabState& per_tab_state = per_tab_state_.at(i);
    CHECK(per_tab_state.result_code.has_value());
    const GURL& url = per_tab_state.url;

    journal().Log(url, task_id(), "LoadAndExtractContentTool::PerTabResult",
                  JournalDetailsBuilder()
                      .Add("url_index", i)
                      .Add("per_tab_result_code",
                           static_cast<int>(per_tab_state.result_code.value()))
                      .Build());

    mojom::ActionResultCode action_result_code =
        ToActionResultCode(per_tab_state.result_code.value());
    if (!IsOk(action_result_code)) {
      journal().Log(
          url, task_id(), "LoadAndExtractContentTool::OnAllUrlsCompleted",
          JournalDetailsBuilder()
              .Add("action_result_code", static_cast<int>(action_result_code))
              .Build());
      PostResponseTask(std::move(invoke_callback_),
                       MakeResult(action_result_code));

      return;
    }
  }
  journal().Log(GURL::EmptyGURL(), task_id(),
                "LoadAndExtractContentTool::OnAllUrlsCompleted",
                JournalDetailsBuilder()
                    .Add("action_result_code",
                         static_cast<int>(mojom::ActionResultCode::kOk))
                    .Build());

  PostResponseTask(std::move(invoke_callback_), MakeOkResult());
}

void LoadAndExtractContentTool::UpdateTaskAfterInvoke(
    ActorTask& task,
    mojom::ActionResultPtr result,
    ToolCallback callback) const {
  std::vector<TabObservation> tab_observations;
  for (const auto& per_tab_state : per_tab_state_) {
    if (per_tab_state.tab_observation.has_value()) {
      tab_observations.push_back(per_tab_state.tab_observation.value());
    }
  }
  task.AddAdditionalTabObservations(std::move(tab_observations));

  std::move(callback).Run(std::move(result));
}

std::string LoadAndExtractContentTool::DebugString() const {
  return "LoadAndExtractContentTool";
}

std::string LoadAndExtractContentTool::JournalEvent() const {
  return "LoadAndExtractContent";
}

std::unique_ptr<ObservationDelayController>
LoadAndExtractContentTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // Save the config to use for the `TabObservationDelayer`s.
  page_stability_config_ = page_stability_config;

  // Any affected tabs will have been closed once the tool invocation is
  // complete, so there's no need to wait for page stability.
  return nullptr;
}

tabs::TabHandle LoadAndExtractContentTool::GetTargetTab() const {
  // This tool can operate on multiple tabs, so there's no single target.
  return tabs::TabHandle::Null();
}

}  // namespace actor
