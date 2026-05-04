// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TAB_OBSERVATION_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TAB_OBSERVATION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/task_id.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

class Profile;

namespace actor {

// Collects the results of tab and window observations after ActorKeyedService
// has performed actions.
struct ObservationResult {
  ObservationResult();
  ~ObservationResult();

  std::vector<optimization_guide::proto::TabObservation> tab_observations;
  std::vector<optimization_guide::proto::WindowObservation> window_observations;
  // Latency steps specifically for the observation phase (APC and screenshots).
  std::vector<
      optimization_guide::proto::ActionsResult_LatencyInformation_LatencyStep>
      latency_steps;

  bool attempted_observation_retry = false;
};

// TabObservationController coordinates the observation of tabs and windows
// associated with an ActorTask after actions have been performed.
//
// It handles:
// 1. Detecting and reloading crashed tabs before observation.
// 2. Collecting structural data about open windows and tabs.
// 3. Orchestrating asynchronous capture of page content and screenshots.
// 4. Retrying failed observations when appropriate.
class TabObservationController {
 public:
  // This callback is guaranteed to be called exactly once if the observation
  // process completes. It will NOT be called if the controller is destroyed
  // before completion.
  using DoneCallback =
      base::OnceCallback<void(TabObservationController*,
                              std::unique_ptr<ObservationResult>)>;

  TabObservationController(
      Profile* profile,
      TaskId task_id,
      base::TimeTicks start_time,
      bool skip_async_observation_information,
      std::vector<actor::ActionResultWithLatencyInfo> action_results,
      DoneCallback done_callback);

  TabObservationController(const TabObservationController&) = delete;
  TabObservationController& operator=(const TabObservationController&) = delete;

  ~TabObservationController();

  // Configures options for screenshot collection.
  void set_screenshot_collection_options(
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions> options) {
    screenshot_collection_options_ = std::move(options);
  }

  // Starts the observation process. This must be called exactly once per
  // object. The ActorTask must exist when this is called.
  void Start();

 private:
  void StartImpl();
  // Internal phases of the observation flow.
  void PerformObservation();
  void FetchObservation(tabs::TabInterface* tab,
                        optimization_guide::proto::TabObservation* observation,
                        base::RepeatingClosure barrier);
  void OnTabObservationFetched(
      tabs::TabHandle tab_handle,
      optimization_guide::proto::TabObservation* observation,
      base::TimeTicks fetch_start_time,
      base::RepeatingClosure barrier,
      ActorKeyedService::TabObservationResult result);
  void OnAllObservationsDone();

  // Crash and retry handling.
  bool ReloadCrashedTab(tabs::TabInterface& crashed_tab);
  void OnReloadDone(tabs::TabHandle tab_handle,
                    ObservationDelayController::Result reload_result);
  void ScheduleRetry();

  ActorTask* GetActorTask() const;

  raw_ptr<Profile> profile_;
  TaskId task_id_;
  base::TimeTicks start_time_;
  bool skip_async_observation_information_;
  std::vector<actor::ActionResultWithLatencyInfo> action_results_;
  DoneCallback done_callback_;

  std::optional<
      page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions>
      screenshot_collection_options_;

  std::unique_ptr<ObservationResult> result_;
  std::unique_ptr<ObservationDelayController> reload_observer_;

  bool attempted_reload_after_crash_ = false;
  bool attempted_observation_retry_ = false;

  base::WeakPtrFactory<TabObservationController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TAB_OBSERVATION_CONTROLLER_H_
