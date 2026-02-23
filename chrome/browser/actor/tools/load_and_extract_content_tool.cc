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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_features.h"
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
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using TabObservation = ::optimization_guide::proto::TabObservation;

namespace actor {

namespace {

TabObservation::TabObservationResult ToTabObservationResult(
    mojom::ActionResultCode code) {
  switch (code) {
    case mojom::ActionResultCode::kOk:
      return TabObservation::TAB_OBSERVATION_OK;
    case mojom::ActionResultCode::kNavigateCommittedErrorPage:
      return TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR;
    case mojom::ActionResultCode::kTabWentAway:
    case mojom::ActionResultCode::kWindowWentAway:
      return TabObservation::TAB_OBSERVATION_TAB_WENT_AWAY;
    case mojom::ActionResultCode::kToolTimeout:
      return TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR;
    case mojom::ActionResultCode::kLoadAndExtractContentExtractionFailed:
      return TabObservation::TAB_OBSERVATION_FETCH_ERROR;
    case mojom::ActionResultCode::kNewTabCreationFailed:
      // New tab creation failure results in no tab observation.
    default:
      // This switch should be exhaustive.
      NOTREACHED();
  }
}

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

// Manages waiting for a tab to finish navigating and then delegation to the
// `ObservationDelayController` to wait for page stability. Begins waiting on
// construction, invoking the callback once the page is stable (or an error
// occurs).
class TabObservationDelayer : public content::WebContentsObserver {
 public:
  TabObservationDelayer(
      content::WebContents* web_contents,
      TaskId task_id,
      AggregatedJournal& journal,
      ObservationDelayController::PageStabilityConfig page_stability_config,
      ToolCallback callback);
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
  ToolCallback callback_;

  std::unique_ptr<ObservationDelayController> observation_delay_controller_;
  bool has_finished_main_frame_navigation_ = false;
  base::OneShotTimer timeout_timer_;
  base::WeakPtrFactory<TabObservationDelayer> weak_ptr_factory_{this};
};

TabObservationDelayer::TabObservationDelayer(
    content::WebContents* web_contents,
    TaskId task_id,
    AggregatedJournal& journal,
    ObservationDelayController::PageStabilityConfig page_stability_config,
    ToolCallback callback)
    : content::WebContentsObserver(web_contents),
      task_id_(task_id),
      journal_(journal),
      page_stability_config_(page_stability_config),
      callback_(std::move(callback)) {
  observation_delay_controller_ = std::make_unique<ObservationDelayController>(
      *web_contents->GetPrimaryMainFrame(), task_id_, journal_,
      page_stability_config_);
  timeout_timer_.Start(FROM_HERE,
                       kGlicActorLoadAndExtractContentToolTimeout.Get(),
                       base::BindOnce(&TabObservationDelayer::OnTimeout,
                                      base::Unretained(this)));
}

void TabObservationDelayer::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  if (has_finished_main_frame_navigation_) {
    // Only wait for the first main frame navigation to start the
    // ObservationDelayController.
    return;
  }
  has_finished_main_frame_navigation_ = true;

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    if (callback_) {
      PostResponseTask(
          std::move(callback_),
          MakeResult(mojom::ActionResultCode::kNavigateCommittedErrorPage));
    }
    return;
  }

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab) {
    if (callback_) {
      PostResponseTask(std::move(callback_),
                       MakeResult(mojom::ActionResultCode::kTabWentAway));
    }
    return;
  }

  observation_delay_controller_->Wait(
      *tab, base::BindOnce(&TabObservationDelayer::OnObservationDelayComplete,
                           weak_ptr_factory_.GetWeakPtr()));
}

void TabObservationDelayer::OnTimeout() {
  if (callback_) {
    PostResponseTask(std::move(callback_),
                     MakeResult(mojom::ActionResultCode::kToolTimeout));
  }
}

void TabObservationDelayer::OnObservationDelayComplete(
    ObservationDelayController::Result result) {
  switch (result) {
    case ObservationDelayController::Result::kOk:
      if (callback_) {
        PostResponseTask(std::move(callback_), MakeOkResult());
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
          PostResponseTask(std::move(callback_),
                           MakeResult(mojom::ActionResultCode::kTabWentAway));
        }
        return;
      }
    }
  }
}

void OnValidatedAllUrls(ToolCallback overall_callback,
                        std::vector<mojom::ActionResultPtr> results) {
  for (auto& result : results) {
    // In the case of multiple errors, we arbitrarily return the first.
    if (!IsOk(result->code)) {
      if (overall_callback) {
        PostResponseTask(std::move(overall_callback), std::move(result));
      }
      return;
    }
  }
  if (overall_callback) {
    PostResponseTask(std::move(overall_callback), MakeOkResult());
  }
}

}  // namespace

struct LoadAndExtractContentTool::PerTabState {
  tabs::TabHandle tab_handle;
  std::unique_ptr<TabObservationDelayer> tab_observation_delayer;

  // Only empty if the tab was not even able to be opened.
  std::optional<TabObservation> tab_observation;

  // This starts null. After invocation for this tab, it should have a kOk
  // result code if all phases succeeded, otherwise it should be the first error
  // encountered.
  mojom::ActionResultPtr result;
};

LoadAndExtractContentTool::LoadAndExtractContentTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    SessionID window_id,
    std::vector<GURL> urls)
    : Tool(task_id, tool_delegate),
      urls_(std::move(urls)),
      window_id_(window_id) {
  per_url_completion_closure_ = base::BarrierClosure(
      urls_.size(),
      base::BindOnce(&LoadAndExtractContentTool::OnAllUrlsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

LoadAndExtractContentTool::~LoadAndExtractContentTool() {
  // In the case of a tool being destroyed before completion, ensure we close
  // any remaining opened tabs.
  for (auto& [_, per_tab_state] : per_tab_state_) {
    if (tabs::TabInterface* tab = per_tab_state.tab_handle.Get()) {
      tab->Close();
    }
  }
}

void LoadAndExtractContentTool::Validate(ToolCallback overall_callback) {
  // TODO(b/478282022): Consider imposing a limit on the number of URLs.
  if (urls_.empty()) {
    PostResponseTask(std::move(overall_callback),
                     MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  // TODO(b/478282022): If any URL is invalid, we could fail immediately instead
  // of waiting for all URLs to return.
  base::RepeatingCallback<void(mojom::ActionResultPtr)> per_url_callback =
      base::BarrierCallback<mojom::ActionResultPtr>(
          urls_.size(),
          base::BindOnce(&OnValidatedAllUrls, std::move(overall_callback)));

  for (const auto& url : urls_) {
    ValidateUrlIsAcceptableNavigationDestination(url, tool_delegate(),
                                                 per_url_callback);
  }
}

void LoadAndExtractContentTool::Invoke(ToolCallback callback) {
  CHECK(!invoke_callback_);
  invoke_callback_ = std::move(callback);

  BrowserWindowInterface* browser_window_interface =
      BrowserWindowInterface::FromSessionID(window_id_);

  if (!browser_window_interface) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }

  for (size_t i = 0; i < urls_.size(); ++i) {
    const GURL& url = urls_[i];
    UrlIndex url_index = UrlIndex(i);

    constexpr int kIndexAppendToEnd = -1;
    content::WebContents* web_contents = chrome::AddAndReturnTabAt(
        browser_window_interface->GetBrowserForMigrationOnly(), url,
        kIndexAppendToEnd, /*foreground=*/false);
    if (!web_contents) {
      per_tab_state_.emplace(
          url_index,
          PerTabState{.result = MakeResult(
                          mojom::ActionResultCode::kNewTabCreationFailed)});
      per_url_completion_closure_.Run();
      continue;
    }

    tabs::TabHandle tab_handle =
        tabs::TabInterface::GetFromContents(web_contents)->GetHandle();
    CHECK(page_stability_config_.has_value());

    TabObservation tab_observation;
    tab_observation.set_id(tab_handle.raw_value());

    // This tool doesn't support screenshots, so set it to error.
    // TODO(b/478282022): Consider adding a new enum value for this case.
    tab_observation.set_screenshot_result(TabObservation::SCREENSHOT_ERROR);

    per_tab_state_.emplace(
        url_index,
        PerTabState{
            .tab_handle = tab_handle,
            .tab_observation_delayer = std::make_unique<TabObservationDelayer>(
                web_contents, task_id(), journal(),
                page_stability_config_.value(),
                base::BindOnce(
                    &LoadAndExtractContentTool::OnTabObservationDelayComplete,
                    weak_ptr_factory_.GetWeakPtr(), url_index)),
            .tab_observation = std::move(tab_observation)});
  }
}

void LoadAndExtractContentTool::OnTabObservationDelayComplete(
    UrlIndex index,
    mojom::ActionResultPtr result) {
  if (!IsOk(result->code)) {
    PostResponseTask(
        base::BindOnce(&LoadAndExtractContentTool::OnTabReadyToClose,
                       weak_ptr_factory_.GetWeakPtr(), index),
        std::move(result));
    return;
  }

  // TODO(b/484078735): Consider hooking into existing tab observation
  // infrastructure rather than re-implementing it here. This would require
  // extending the existing infra to support multiple tabs and to support a new
  // 'finalization' phase after observation where we can close the tabs.
  tabs::TabInterface* tab = per_tab_state_[index].tab_handle.Get();
  if (!tab) {
    per_tab_state_[index].result =
        MakeResult(mojom::ActionResultCode::kTabWentAway);
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
    UrlIndex index,
    optimization_guide::AIPageContentResultOrError result_or_error) {
  mojom::ActionResultPtr result;

  TabObservation& tab_observation =
      per_tab_state_[index].tab_observation.value();

  if (result_or_error.has_value()) {
    tab_observation.set_annotated_page_content_result(
        TabObservation::ANNOTATED_PAGE_CONTENT_OK);
    *tab_observation.mutable_annotated_page_content() =
        std::move(result_or_error->proto);
    FillInTabObservationMetadata(result_or_error->metadata, tab_observation);

    result = MakeOkResult();
  } else {
    tab_observation.set_annotated_page_content_result(
        TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);
    result = MakeResult(
        mojom::ActionResultCode::kLoadAndExtractContentExtractionFailed);
  }
  PostResponseTask(base::BindOnce(&LoadAndExtractContentTool::OnTabReadyToClose,
                                  weak_ptr_factory_.GetWeakPtr(), index),
                   std::move(result));
}

void LoadAndExtractContentTool::OnTabReadyToClose(
    UrlIndex index,
    mojom::ActionResultPtr result) {
  if (tabs::TabInterface* tab = per_tab_state_[index].tab_handle.Get()) {
    tab->Close();
  } else if (!per_tab_state_[index]
                  .tab_observation->has_annotated_page_content()) {
    // Throw an error if the tab was closed before we could extract the APC.
    result = MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  per_tab_state_[index].result = std::move(result);
  per_url_completion_closure_.Run();
}

void LoadAndExtractContentTool::OnAllUrlsCompleted() {
  if (!invoke_callback_) {
    return;
  }

  // Update all tab observations with the final results, even if some tabs
  // encountered errors.
  for (auto& [_, per_tab_state] : per_tab_state_) {
    if (per_tab_state.tab_observation.has_value()) {
      TabObservation& tab_observation = per_tab_state.tab_observation.value();

      bool has_apc = tab_observation.has_annotated_page_content();
      tab_observation.set_annotated_page_content_result(
          has_apc ? TabObservation::ANNOTATED_PAGE_CONTENT_OK
                  : TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);

      tab_observation.set_result(
          ToTabObservationResult(per_tab_state.result->code));
    }
  }

  for (auto& [_, per_tab_state] : per_tab_state_) {
    CHECK(per_tab_state.result);
    if (!IsOk(per_tab_state.result->code)) {
      PostResponseTask(std::move(invoke_callback_),
                       std::move(per_tab_state.result));

      return;
    }
  }

  // TODO(b/478282022): Plumb the TabObservation results back to the caller.
  PostResponseTask(std::move(invoke_callback_), MakeOkResult());
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
