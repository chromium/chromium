// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TASK_MANAGER_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TASK_MANAGER_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace actor {
struct ActionResultWithLatencyInfo;
class ActorKeyedService;
class ActorTaskDelegate;
struct ObservationResult;
class TabObservationController;
}  // namespace actor

namespace glic {
class GlicActorClientSession;
class GlicInstanceMetrics;
class GlicActorJournalHandler;

// Manages actor-related tasks for GlicKeyedService.
class GlicActorTaskManager {
 public:
  class Delegate {
   public:
    virtual std::optional<std::string> conversation_id() const = 0;
    virtual base::WeakPtr<actor::ActorTaskDelegate> GetActorTaskDelegate() = 0;
  };
  explicit GlicActorTaskManager(Profile* profile,
                                actor::ActorKeyedService* actor_keyed_service,
                                GlicActorPolicyChecker& actor_policy_checker,
                                GlicInstanceMetrics* instance_metrics,
                                Delegate* delegate);
  GlicActorTaskManager(const GlicActorTaskManager&) = delete;
  GlicActorTaskManager& operator=(const GlicActorTaskManager&) = delete;
  ~GlicActorTaskManager();

  void MaybeShowDeactivationToastUi();

  void CancelTask();
  bool IsActuating() const;

  // Adds a callback that is run when the actuating state changes.
  base::CallbackListSubscription AddActuatingChangedCallback(
      base::RepeatingCallback<void(bool)> callback);

  GlicActorClientSession* BindSession();
  void UnbindSession();

  base::WeakPtr<GlicActorTaskManager> GetWeakPtr();
  Profile* profile() const { return profile_; }

  GlicActorClientSession* GetClientSessionForTesting();

 private:
  void SetActuating(bool actuating);
  friend class GlicActorClientSession;

  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_keyed_service_;
  const raw_ref<GlicActorPolicyChecker> actor_policy_checker_;
  raw_ptr<GlicInstanceMetrics> instance_metrics_;
  bool actuating_ = false;
  base::RepeatingCallbackList<void(bool)> actuating_changed_callbacks_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<GlicActorClientSession> session_;
  base::WeakPtrFactory<GlicActorTaskManager> weak_ptr_factory_{this};
};

class GlicActorClientSession {
 public:
  explicit GlicActorClientSession(GlicActorTaskManager* manager);
  ~GlicActorClientSession();

  bool IsActuating() const;
  void CanActOnWebChanged(bool can_act_on_web);
  void CreateTask(actor::webui::mojom::TaskOptionsPtr options,
                  glic::mojom::WebClientHandler::CreateTaskCallback callback);
  void PerformActions(const std::vector<uint8_t>& actions_proto,
                      mojom::WebClientHandler::PerformActionsCallback callback);
  void CancelActions(actor::TaskId task_id,
                     mojom::WebClientHandler::CancelActionsCallback callback);
  void StopActorTask(actor::TaskId task_id,
                     mojom::ActorTaskStopReason stop_reason);
  void PauseActorTask(actor::TaskId task_id,
                      mojom::ActorTaskPauseReason pause_reason,
                      tabs::TabInterface::Handle tab_handle);
  void ResumeActorTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback);
  void InterruptActorTask(actor::TaskId task_id);
  void UninterruptActorTask(actor::TaskId task_id);
  void CreateActorTab(
      actor::TaskId task_id,
      bool open_in_background,
      const std::optional<int32_t>& initiator_tab_id,
      const std::optional<int32_t>& initiator_window_id,
      glic::mojom::WebClientHandler::CreateActorTabCallback callback);
  void CancelTask();

  void LogBeginAsyncEvent(uint64_t event_async_id,
                          int32_t task_id,
                          const std::string& event,
                          const std::string& details);
  void LogEndAsyncEvent(uint64_t event_async_id, const std::string& details);
  void LogInstantEvent(int32_t task_id,
                       const std::string& event,
                       const std::string& details);
  void JournalClear();
  void JournalSnapshot(
      bool clear_journal,
      glic::mojom::WebClientHandler::JournalSnapshotCallback callback);
  void JournalStart(uint64_t max_bytes, bool capture_screenshots);
  void JournalStop();
  void JournalRecordFeedback(bool positive, const std::string& reason);

  base::WeakPtr<GlicActorClientSession> GetWeakPtr();

 private:
  void PerformActionsFinished(
      mojom::WebClientHandler::PerformActionsCallback callback,
      actor::TaskId task_id,
      base::TimeTicks start_time,
      bool skip_async_observation_information,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>
          screenshot_collection_options,
      std::vector<actor::ActionResultWithLatencyInfo> action_results);
  void DidFinishBuildObservation(
      mojom::WebClientHandler::PerformActionsCallback callback,
      base::TimeTicks start_time,
      std::vector<actor::ActionResultWithLatencyInfo> action_results,
      actor::TaskId task_id,
      bool skip_async_observation_information,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>
          screenshot_collection_options,
      std::unique_ptr<optimization_guide::proto::ActionsResult> result,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
          journal_entry);
  void OnPerformActionsComplete(
      mojom::WebClientHandler::PerformActionsCallback callback,
      base::TimeTicks start_time,
      std::vector<actor::ActionResultWithLatencyInfo> action_results,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
          journal_entry,
      actor::TabObservationController* controller_ptr,
      std::unique_ptr<actor::ObservationResult> result);
  void ReloadCrashedTab(tabs::TabInterface& crashed_tab,
                        actor::TaskId task_id,
                        base::OnceClosure callback);
  void CreateActorTabFinished(
      glic::mojom::WebClientHandler::CreateActorTabCallback callback,
      tabs::TabInterface* new_tab);
  void ReloadObserverDone(tabs::TabHandle tab_handle,
                          base::OnceClosure callback,
                          actor::ObservationDelayController::Result result);
  void NotifyActorTaskStateChanged(actor::ActorTask& task);
  void StopTaskImpl(actor::TaskId task_id,
                    actor::ActorTask::StoppedReason reason);
  actor::ActorKeyedService& actor_keyed_service() const;
  GlicActorPolicyChecker& actor_policy_checker() const;
  GlicInstanceMetrics& instance_metrics() const;
  Profile& profile() const;

  std::unique_ptr<actor::ObservationDelayController> reload_observer_;
  std::vector<std::unique_ptr<actor::TabObservationController>>
      observation_controllers_;

  // Only attempt to reload a crashed tab once *per task*. Crashes should be
  // rare so if we're getting repeated crashes it's likely being triggered by
  // actor code; retrying repeatedly will only trigger more crashes. After the
  // second crash we prefer to proceed to observation code with a crashed tab
  // which will be noticed there and return with a TAB_OBSERVATION_PAGE_CRASHED
  // code.
  bool attempted_reload_after_crash_ = false;
  bool attempted_observation_retry_ = false;
  actor::TaskId current_task_id_;
  std::optional<base::CallbackListSubscription>
      actor_task_state_changed_subscription_;
  base::CallbackListSubscription can_act_on_web_changed_subscription_;
  raw_ref<GlicActorTaskManager> manager_;
  std::unique_ptr<GlicActorJournalHandler> journal_handler_;
  base::WeakPtrFactory<GlicActorClientSession> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TASK_MANAGER_H_
