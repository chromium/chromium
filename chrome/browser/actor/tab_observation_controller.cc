// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_observation_controller.h"

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#else
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif  // !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)

namespace actor {

namespace {

// TODO(bokan): These should probably be shared constants.
BASE_FEATURE(kGlicReloadAfterPerformActionsCrash,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kGlicRetryFailedObservations, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kObservationRetryDelay{
    &kGlicRetryFailedObservations, "delay", base::Seconds(5)};

tabs::TabInterface* GetCrashedTab(actor::ActorTask& task) {
  for (tabs::TabHandle tab_handle : task.GetLastActedTabs()) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab) {
      continue;
    }

    content::WebContents* contents = tab->GetContents();
    if (contents && contents->IsCrashed()) {
      return tab;
    }
  }

  return nullptr;
}

optimization_guide::proto::TabObservation::TabObservationResult
ToTabObservationResult(page_content_annotations::FetchPageContextError error) {
  using optimization_guide::proto::TabObservation;
  switch (error) {
    case page_content_annotations::FetchPageContextError::kUnknown:
      return TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR;
    case page_content_annotations::FetchPageContextError::kWebContentsChanged:
      return TabObservation::TAB_OBSERVATION_WEB_CONTENTS_CHANGED;
    case page_content_annotations::FetchPageContextError::
        kPageContextNotEligible:
      return TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE;
    case page_content_annotations::FetchPageContextError::kWebContentsWentAway:
      return TabObservation::TAB_OBSERVATION_TAB_WENT_AWAY;
  }
}

}  // namespace

ObservationResult::ObservationResult() = default;
ObservationResult::~ObservationResult() = default;

TabObservationController::TabObservationController(
    Profile* profile,
    TaskId task_id,
    base::TimeTicks start_time,
    bool skip_async_observation_information,
    std::vector<actor::ActionResultWithLatencyInfo> action_results,
    DoneCallback done_callback)
    : profile_(profile),
      task_id_(task_id),
      start_time_(start_time),
      skip_async_observation_information_(skip_async_observation_information),
      action_results_(std::move(action_results)),
      done_callback_(std::move(done_callback)) {}

TabObservationController::~TabObservationController() = default;

void TabObservationController::Start() {
  CHECK(!result_);
  CHECK(GetActorTask());
  result_ = std::make_unique<ObservationResult>();
  StartImpl();
}

void TabObservationController::StartImpl() {
  ActorTask* task = GetActorTask();
  if (!task) {
    OnAllObservationsDone();
    return;
  }

  if (base::FeatureList::IsEnabled(kGlicReloadAfterPerformActionsCrash) &&
      !attempted_reload_after_crash_) {
    if (tabs::TabInterface* crashed_tab = GetCrashedTab(*task)) {
      attempted_reload_after_crash_ = true;
      if (ReloadCrashedTab(*crashed_tab)) {
        return;
      }
    }
  }

  PerformObservation();
}

void TabObservationController::PerformObservation() {
  ActorTask* task = GetActorTask();
  if (!task) {
    OnAllObservationsDone();
    return;
  }

  // Reset the result object if this is a retry.
  result_ = std::make_unique<ObservationResult>();

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  ProfileBrowserCollection::GetForProfile(profile_)->ForEach(
      [this](BrowserWindowInterface* browser) {
        optimization_guide::proto::WindowObservation window_observation;
        window_observation.set_id(browser->GetSessionID().id());
        window_observation.set_active(browser->IsActive());

        if (tabs::TabInterface* tab = browser->GetActiveTabInterface()) {
          window_observation.set_activated_tab_id(tab->GetHandle().raw_value());
        }

        for (const tabs::TabInterface* tab : *browser->GetTabStripModel()) {
          window_observation.add_tab_ids(tab->GetHandle().raw_value());
        }
        result_->window_observations.push_back(std::move(window_observation));
        return true;
      });
#else
  GlobalBrowserCollection* browser_collection =
      GlobalBrowserCollection::GetInstance();
  browser_collection->ForEach(
      [this](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile_) {
          return true;
        }

        optimization_guide::proto::WindowObservation window_observation;
        window_observation.set_id(browser->GetSessionID().id());
        window_observation.set_active(result_->window_observations.empty());

        if (TabModel* tab_model =
                static_cast<TabModel*>(TabListInterface::From(browser))) {
          if (tabs::TabInterface* active_tab = tab_model->GetActiveTab()) {
            window_observation.set_activated_tab_id(
                active_tab->GetHandle().raw_value());
          }

          for (const tabs::TabInterface* tab : tab_model->GetAllTabs()) {
            window_observation.add_tab_ids(tab->GetHandle().raw_value());
          }
        }

        result_->window_observations.push_back(std::move(window_observation));
        return true;
      },
      BrowserCollection::Order::kActivation);
#endif

  std::vector<tabs::TabInterface*> tabs_to_fetch;
  ActorTask::TabHandleSet last_acted_tabs = task->GetLastActedTabs();
  for (const tabs::TabHandle& handle : last_acted_tabs) {
    tabs::TabInterface* tab = handle.Get();
    optimization_guide::proto::TabObservation tab_observation;
    tab_observation.set_id(handle.raw_value());

    if (!tab) {
      tab_observation.set_result(optimization_guide::proto::TabObservation::
                                     TAB_OBSERVATION_TAB_WENT_AWAY);
    } else if (!tab->GetContents()
                    ->GetPrimaryMainFrame()
                    ->IsRenderFrameLive()) {
      tab_observation.set_result(optimization_guide::proto::TabObservation::
                                     TAB_OBSERVATION_PAGE_CRASHED);
    } else {
      if (skip_async_observation_information_) {
        tab_observation.set_result(
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
      } else {
        tabs_to_fetch.push_back(tab);
      }
    }

    result_->tab_observations.push_back(std::move(tab_observation));
  }

  // Add additional observations (e.g. from LoadAndExtractTool).
  for (const auto& additional_obs : task->GetAdditionalTabObservations()) {
    tabs::TabHandle handle(additional_obs.id());
    if (last_acted_tabs.contains(handle)) {
      continue;
    }
    result_->tab_observations.push_back(additional_obs);
  }

  actor::RecordPageContextTabCount(tabs_to_fetch.size());

  if (tabs_to_fetch.empty()) {
    OnAllObservationsDone();
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(
      tabs_to_fetch.size(),
      base::BindOnce(&TabObservationController::OnAllObservationsDone,
                     weak_ptr_factory_.GetWeakPtr()));

  for (tabs::TabInterface* tab : tabs_to_fetch) {
    // Find the observation we just added.
    optimization_guide::proto::TabObservation* observation = nullptr;
    for (auto& obs : result_->tab_observations) {
      if (obs.id() == tab->GetHandle().raw_value()) {
        observation = &obs;
        break;
      }
    }
    CHECK(observation);

    FetchObservation(tab, observation, barrier);
  }
}

void TabObservationController::FetchObservation(
    tabs::TabInterface* tab,
    optimization_guide::proto::TabObservation* observation,
    base::RepeatingClosure barrier) {
  auto* actor_service = actor::ActorKeyedService::Get(profile_);

  actor_service->RequestTabObservation(
      *tab, task_id_, screenshot_collection_options_,
      base::BindOnce(&TabObservationController::OnTabObservationFetched,
                     weak_ptr_factory_.GetWeakPtr(), tab->GetHandle(),
                     observation, base::TimeTicks::Now(), barrier));
}

void TabObservationController::OnTabObservationFetched(
    tabs::TabHandle tab_handle,
    optimization_guide::proto::TabObservation* observation,
    base::TimeTicks fetch_start_time,
    base::RepeatingClosure barrier,
    ActorKeyedService::TabObservationResult result) {
  base::ScopedClosureRunner run_barrier_at_return(barrier);

  tabs::TabInterface* const tab = tab_handle.Get();

  if (!tab || !tab->GetContents()) {
    observation->set_result(optimization_guide::proto::TabObservation::
                                TAB_OBSERVATION_TAB_WENT_AWAY);
  } else if (tab->GetContents()->IsCrashed()) {
    observation->set_result(optimization_guide::proto::TabObservation::
                                TAB_OBSERVATION_PAGE_CRASHED);
  } else if (!result.has_value()) {
    observation->set_result(ToTabObservationResult(result.error().error_code));
  } else {
    page_content_annotations::FetchPageContextResult& fetch_result = **result;
    bool has_apc = fetch_result.annotated_page_content_result.has_value();
    observation->set_annotated_page_content_result(
        has_apc ? optimization_guide::proto::TabObservation::
                      ANNOTATED_PAGE_CONTENT_OK
                : optimization_guide::proto::TabObservation::
                      ANNOTATED_PAGE_CONTENT_ERROR);

    bool has_screenshot = fetch_result.screenshot_result.has_value();
    bool screenshot_required =
        !base::FeatureList::IsEnabled(actor::kGlicActorSkipScreenshot);
    observation->set_screenshot_result(
        has_screenshot || !screenshot_required
            ? optimization_guide::proto::TabObservation::SCREENSHOT_OK
            : optimization_guide::proto::TabObservation::SCREENSHOT_ERROR);

    if (!has_apc || (screenshot_required && !has_screenshot)) {
      observation->set_result(optimization_guide::proto::TabObservation::
                                  TAB_OBSERVATION_FETCH_ERROR);
    }

    observation->set_result(
        optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);

    // Populate latency steps.
    {
      optimization_guide::proto::ActionsResult_LatencyInformation_LatencyStep
          latency_step;
      latency_step.mutable_annotated_page_content()->set_id(observation->id());
      latency_step.set_latency_start_ms(
          (fetch_start_time - start_time_).InMilliseconds());
      latency_step.set_latency_stop_ms(
          (fetch_result.annotated_page_content_result.value().end_time -
           start_time_)
              .InMilliseconds());
      result_->latency_steps.push_back(std::move(latency_step));
      actor::RecordPageContextApcDuration(
          fetch_result.annotated_page_content_result.value().end_time -
          fetch_start_time);
    }

    if (has_screenshot) {
      optimization_guide::proto::ActionsResult_LatencyInformation_LatencyStep
          latency_step;
      latency_step.mutable_screenshot()->set_id(observation->id());
      latency_step.set_latency_start_ms(
          (fetch_start_time - start_time_).InMilliseconds());
      latency_step.set_latency_stop_ms(
          (fetch_result.screenshot_result.value().end_time - start_time_)
              .InMilliseconds());
      result_->latency_steps.push_back(std::move(latency_step));
      actor::RecordPageContextScreenshotDuration(
          fetch_result.screenshot_result.value().end_time - fetch_start_time);
    }

    if (!GetTabObservationResultOverrideForTesting().is_null()) {  // IN-TEST
      GetTabObservationResultOverrideForTesting().Run(             // IN-TEST
          observation, **result);
    }

    // Copy script tool results if any.
    //
    // TODO(b/489841640): Remove this once migration to
    // ActionsResult.script_tool_results is done.
    actor::CopyScriptToolResults(*fetch_result.annotated_page_content_result
                                      ->proto.mutable_main_frame_data(),
                                 action_results_);

    actor::FillInTabObservation(fetch_result, *observation);
  }
}

void TabObservationController::OnAllObservationsDone() {
  if (base::FeatureList::IsEnabled(kGlicRetryFailedObservations) &&
      !attempted_observation_retry_) {
    bool has_failure = false;
    for (const auto& obs : result_->tab_observations) {
      if (obs.result() !=
          optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK) {
        has_failure = true;
        break;
      }
    }

    if (has_failure) {
      attempted_observation_retry_ = true;
      ScheduleRetry();
      return;
    }
  }

  result_->attempted_observation_retry = attempted_observation_retry_;
  std::move(done_callback_).Run(this, std::move(result_));
}

bool TabObservationController::ReloadCrashedTab(
    tabs::TabInterface& crashed_tab) {
  content::WebContents* contents = crashed_tab.GetContents();
  if (!contents) {
    return false;
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile_);
  reload_observer_ = std::make_unique<actor::ObservationDelayController>(
      task_id_, actor_service->GetJournal());

  contents->GetController().Reload(content::ReloadType::NORMAL, true);
  reload_observer_->Wait(
      crashed_tab,
      base::BindOnce(&TabObservationController::OnReloadDone,
                     weak_ptr_factory_.GetWeakPtr(), crashed_tab.GetHandle()));
  return true;
}

void TabObservationController::OnReloadDone(
    tabs::TabHandle tab_handle,
    ObservationDelayController::Result reload_result) {
  if (reload_result ==
      actor::ObservationDelayController::Result::kPageNavigated) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (tab) {
      size_t last_navigation_count = reload_observer_->NavigationCount();
      auto* actor_service = actor::ActorKeyedService::Get(profile_);
      reload_observer_ = std::make_unique<actor::ObservationDelayController>(
          task_id_, actor_service->GetJournal());
      reload_observer_->SetNavigationCount(last_navigation_count + 1);
      reload_observer_->Wait(
          *tab, base::BindOnce(&TabObservationController::OnReloadDone,
                               weak_ptr_factory_.GetWeakPtr(), tab_handle));
      return;
    }
  }

  reload_observer_.reset();
  StartImpl();
}

void TabObservationController::ScheduleRetry() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabObservationController::PerformObservation,
                     weak_ptr_factory_.GetWeakPtr()),
      kObservationRetryDelay.Get());
}

ActorTask* TabObservationController::GetActorTask() const {
  auto* actor_service = actor::ActorKeyedService::Get(profile_);
  return actor_service ? actor_service->GetTask(task_id_) : nullptr;
}

}  // namespace actor
