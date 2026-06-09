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
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/task_id.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

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
class GlicSharingManagerInternal;
class GlicInstanceMetrics;
class GlicActorClientSession;

class GlicActorClientSessionInterface : public mojom::ActorHandler,
                                        public actor::ActorTaskDelegate {
 public:
  ~GlicActorClientSessionInterface() override;
  virtual mojom::ActorClient* GetClient() = 0;
  virtual void CancelActiveTask() = 0;
};

// Manages actor-related tasks for GlicKeyedService.
class GlicActorTaskManager {
 public:
  class Delegate {
   public:
    virtual std::optional<std::string> conversation_id() const = 0;

    virtual void OnTabAddedToTask(
        actor::TaskId task_id,
        const tabs::TabInterface::Handle& tab_handle) = 0;
  };
  explicit GlicActorTaskManager(
      Profile* profile,
      actor::ActorKeyedService* actor_keyed_service,
      GlicActorPolicyChecker& actor_policy_checker,
      GlicInstanceMetrics* instance_metrics,
      glic::GlicSharingManagerInternal* sharing_manager,
      Delegate* delegate);
  GlicActorTaskManager(const GlicActorTaskManager&) = delete;
  GlicActorTaskManager& operator=(const GlicActorTaskManager&) = delete;
  ~GlicActorTaskManager();

  void MaybeShowDeactivationToastUi();

  void CancelTask();
  bool IsActuating() const;

  // Returns the last acted tabs for the current task.
  std::vector<tabs::TabInterface*> GetLastActedTabs() const;

  // Adds a callback that is run when the actuating state changes.
  base::CallbackListSubscription AddActuatingChangedCallback(
      base::RepeatingCallback<void(bool)> callback);

  void Bind(mojo::PendingReceiver<mojom::ActorHandler> receiver,
            mojo::PendingRemote<mojom::ActorClient> client);
  void UnbindSession();

  base::WeakPtr<GlicActorTaskManager> GetWeakPtr();
  Profile* profile() const { return profile_; }

  GlicActorClientSessionInterface* GetClientSessionForTesting();

 private:
  void SetActuating(bool actuating);
  friend class GlicActorClientSession;

  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_keyed_service_;
  const raw_ref<GlicActorPolicyChecker> actor_policy_checker_;
  raw_ptr<GlicInstanceMetrics> instance_metrics_;
  raw_ptr<GlicSharingManagerInternal> sharing_manager_;
  bool actuating_ = false;
  base::RepeatingCallbackList<void(bool)> actuating_changed_callbacks_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<GlicActorClientSession> session_;
  base::WeakPtrFactory<GlicActorTaskManager> weak_ptr_factory_{this};
};

class GlicActorClientSession : public GlicActorClientSessionInterface {
 public:
  GlicActorClientSession(GlicActorTaskManager* manager,
                         mojo::PendingReceiver<mojom::ActorHandler> receiver,
                         mojo::PendingRemote<mojom::ActorClient> client);
  ~GlicActorClientSession() override;

  // Unbinds this session from GlicActorTaskManager. Deletes this.
  void Unbind();
  void CancelActiveTask() override;

  // GlicActorClientSessionInterface:
  mojom::ActorClient* GetClient() override;

  actor::TaskId current_task_id() const { return current_task_id_; }

  // mojom::ActorHandler:
  void GetContextForActorFromTab(
      int32_t tab_id,
      mojom::GetTabContextOptionsPtr options,
      GetContextForActorFromTabCallback callback) override;

  // actor::mojom::ActorHandler:
  bool IsActuating() const;
  void CanActOnWebChanged(bool can_act_on_web);
  void CreateTask(actor::webui::mojom::TaskOptionsPtr options,
                  CreateTaskCallback callback) override;
  void PerformActions(const std::vector<uint8_t>& actions_proto,
                      PerformActionsCallback callback) override;
  void CancelActions(int32_t task_id, CancelActionsCallback callback) override;
  void StopActorTask(int32_t task_id,
                     mojom::ActorTaskStopReason stop_reason) override;
  void PauseActorTask(int32_t task_id,
                      mojom::ActorTaskPauseReason pause_reason,
                      std::optional<int32_t> tab_handle) override;
  void ResumeActorTask(int32_t task_id,
                       mojom::GetTabContextOptionsPtr context_options,
                       ResumeActorTaskCallback callback) override;
  void InterruptActorTask(
      int32_t task_id,
      std::optional<mojom::ActorTaskInterruptReason> interrupt_reason) override;
  void UninterruptActorTask(int32_t task_id) override;
  void CreateActorTab(int32_t task_id,
                      bool open_in_background,
                      std::optional<int32_t> initiator_tab_id,
                      std::optional<int32_t> initiator_window_id,
                      CreateActorTabCallback callback) override;

  void LogBeginAsyncEvent(uint64_t event_async_id,
                          int32_t task_id,
                          const std::string& event,
                          const std::string& details) override;
  void LogEndAsyncEvent(uint64_t event_async_id,
                        const std::string& details) override;
  void LogInstantEvent(int32_t task_id,
                       const std::string& event,
                       const std::string& details) override;
  void JournalClear() override;
  void JournalSnapshot(bool clear_journal,
                       JournalSnapshotCallback callback) override;
  void JournalStart(uint64_t max_bytes, bool capture_screenshots) override;
  void JournalStop() override;
  void JournalRecordFeedback(bool positive, const std::string& reason) override;

  base::WeakPtr<GlicActorClientSession> GetWeakPtr();

  // ActorTaskDelegate:
  void OnTabAddedToTask(actor::TaskId task_id,
                        const tabs::TabInterface::Handle& tab_handle) override;
  void RequestToShowCredentialSelectionDialog(
      actor::TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      actor::ActorTaskDelegate::CredentialSelectedCallback callback) override;
  void RequestToShowUserConfirmationDialog(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      actor::ActorTaskDelegate::UserConfirmationDialogCallback callback)
      override;
  void RequestToConfirmNavigation(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      actor::ActorTaskDelegate::NavigationConfirmationCallback callback)
      override;
  void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
      AutofillSuggestionSelectedCallback callback) override;
  void AutofillSuggestionDialogOnFormPresented(
      int32_t task_id,
      actor::webui::mojom::AutofillSuggestionDialogOnFormPresentedParamsPtr
          params) override;
  void AutofillSuggestionDialogOnFormPreviewChanged(
      int32_t task_id,
      actor::webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParamsPtr
          params) override;
  void AutofillSuggestionDialogOnFormConfirmed(
      int32_t task_id,
      actor::webui::mojom::AutofillSuggestionDialogOnFormConfirmedParamsPtr
          params) override;

 private:
  void PerformActionsFinished(
      PerformActionsCallback callback,
      actor::TaskId task_id,
      base::TimeTicks start_time,
      bool skip_async_observation_information,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>
          screenshot_collection_options,
      std::vector<actor::ActionResultWithLatencyInfo> action_results,
      actor::TabObservationStrategy observation_strategy);
  void DidFinishBuildObservation(
      PerformActionsCallback callback,
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
      PerformActionsCallback callback,
      base::TimeTicks start_time,
      std::vector<actor::ActionResultWithLatencyInfo> action_results,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
          journal_entry,
      actor::TabObservationController* controller_ptr,
      std::unique_ptr<actor::ObservationResult> result);
  void ReloadCrashedTab(tabs::TabInterface& crashed_tab,
                        actor::TaskId task_id,
                        base::OnceClosure callback);
  void CreateActorTabFinished(CreateActorTabCallback callback,
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

  mojo::Remote<mojom::ActorClient> actor_client_;
  mojo::Receiver<mojom::ActorHandler> receiver_{this};
  std::unique_ptr<actor::ObservationDelayController> reload_observer_;
  std::vector<std::unique_ptr<actor::TabObservationController>>
      observation_controllers_;

  base::WeakPtr<actor::AutofillSelectionDialogEventHandler>
      autofill_selection_event_handler_;

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
