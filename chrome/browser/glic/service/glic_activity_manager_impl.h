// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTIVITY_MANAGER_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTIVITY_MANAGER_IMPL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/common/glic_instance_observer.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/glic_instance_id.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "components/actor/core/task_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;

namespace glic {

class GlicInstance;
class GlicInstanceCoordinatorImpl;
class GlicInstanceImpl;

// Concrete implementation of GlicActivityManager internal to
// //chrome/browser/glic:impl. Owns producer and lifecycle entry points in
// addition to the public consumer query/subscription API.
class GlicActivityManagerImpl : public GlicActivityManager,
                                private GlicInstanceObserver {
 public:
  GlicActivityManagerImpl(Profile* profile,
                          GlicInstanceCoordinatorImpl& instance_coordinator);
  ~GlicActivityManagerImpl() override;

  // GlicActivityManager:
  const absl::flat_hash_map<GlicActivityKey, GlicActivityItem>& activities()
      const override;
  std::vector<const GlicActivityItem*> GetSortedActivities(
      const tabs::TabInterface* active_tab) const override;
  void RefreshActivities() override;
  GlicActivityNudgeState GetCurrentActivityNudgeState() const override;
  size_t GetNumUnseenActivities() const override;
  bool HasUnseenOrInProgressActivities() const override;
  base::CallbackListSubscription RegisterActivityNudgeStateChange(
      ActivityNudgeChangeCallback callback) override;
  base::CallbackListSubscription RegisterActivityAlertCallback(
      ActivityAlertCallback callback) override;
  void MarkActivitySeen(const GlicActivityKey& key) override;
  void MarkActivitySeen(actor::TaskId task_id) override;

  // Instance-level conversation activity and lifecycle hooks. Buffers pending
  // state per InstanceId when activity events arrive before a conversation ID
  // is registered on the instance.
  void OnInstanceActivityStateChanged(
      const GlicInstance& instance,
      GlicActivityState state,
      std::optional<std::string> preview_snippet);
  void OnInstanceVisibilityChanged(const GlicInstance& instance) override;
  void OnInstanceConversationRegistered(const GlicInstance& instance) override;
  void OnInstanceResponseStarted(const GlicInstance& instance) override;
  void OnInstanceResponseStopped(const GlicInstance& instance,
                                 mojom::ResponseStopCause cause) override;
  void OnInstanceWebClientCleared(const GlicInstance& instance) override;
  void OnInstanceTabAddedToTask(const GlicInstance& instance,
                                actor::TaskId task_id) override;
  void ClearInstanceActivity(const GlicInstance& instance);

  // Updates a conversation's activity state or preview snippet directly by ID.
  void OnConversationActivityStateChanged(
      const std::string& conversation_id,
      GlicConversationActivityUpdate update);

  // Updates a conversation's title if tracked.
  void OnConversationTitleUpdated(const std::string& conversation_id,
                                  std::string_view title);
  // Removes tracked conversation activity for `conversation_id`.
  void OnConversationDestroyed(const std::string& conversation_id);

  // Called when a tab is added to an actor task.
  void OnTabAddedToTask(actor::TaskId task_id);

  void Shutdown();

 private:
  // Instance observation lifecycle handlers.
  void OnInstanceDestroyed(GlicInstanceImpl* instance) override;
  void OnInstanceCreated(GlicInstanceImpl* instance);

  // Actor task and activity item state handlers.
  void OnActorTaskStateUpdate(actor::TaskId task_id);
  void OnActorTaskRemoved(actor::TaskId task_id);
  void PopulateActivityItemDetails(GlicActivityItem& item);
  void PopulateActorTaskItemDetails(actor::TaskId task_id,
                                    GlicActivityItem& item);

  // Helper functions for Actor task policies and UI states.
  static bool RequiresAttention(actor::ActorTask::State state);
  static bool RequiresTaskProcessing(actor::ActorTask::State state,
                                     glic::mojom::FeatureMode feature_mode);
  static bool ShouldShowBubble(actor::ActorTask::State state,
                               actor::ActorTask::TaskDuration duration,
                               glic::mojom::FeatureMode feature_mode);
  static bool IsActiveExperimentalTask(actor::ActorTask::State state,
                                       glic::mojom::FeatureMode feature_mode);
  static bool IsActiveUniversalCartTask(actor::ActorTask::State state,
                                        glic::mojom::FeatureMode feature_mode);
  static bool IsActivePasswordChangeTask(actor::ActorTask::State state,
                                         glic::mojom::FeatureMode feature_mode);

  // Returns true if notifications should be suppressed for the task.
  static bool ShouldSuppressNotification(glic::mojom::FeatureMode feature_mode,
                                         actor::ActorTask::State state);

  struct InstanceActivityState {
    GlicActivityState state = GlicActivityState::kInProgress;
    std::optional<std::string> preview_snippet;
  };

  void ResetInstanceActivityToIdle(const GlicInstance& instance);
  bool OnConversationActivityStateChangedImpl(
      const std::string& conversation_id,
      GlicConversationActivityUpdate update);
  bool MarkActivitySeenImpl(const GlicActivityKey& key);
  bool UpdateTaskNudge(bool show_bubble, bool force_notify);

  raw_ptr<Profile> profile_;

  base::CallbackListSubscription instance_created_subscription_;
  base::ScopedMultiSourceObservation<GlicInstanceImpl, GlicInstanceObserver>
      instance_observations_{this};
  std::vector<base::CallbackListSubscription> actor_subscriptions_;

  // Per-instance conversation activity state (including state buffered before
  // RegisterConversation assigns a conversation_id).
  base::flat_map<InstanceId, InstanceActivityState> instance_activity_states_;

  // Map of all tracked activities.
  absl::flat_hash_map<GlicActivityKey, GlicActivityItem> activities_;

  base::RepeatingCallbackList<void(bool show_bubble,
                                   GlicActivityNudgeState activity_nudge_state)>
      activity_nudge_state_change_callback_list_;

  base::RepeatingCallbackList<void(ActivityInterruptionLevel level)>
      activity_alert_callback_list_;

  GlicActivityNudgeState current_activity_nudge_state_;
  size_t last_notified_activity_count_ = 0;
  absl::flat_hash_set<actor::TaskId> tasks_notified_of_start_;
  absl::flat_hash_map<actor::TaskId, actor::ActorTask::State>
      last_actor_task_states_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_ACTIVITY_MANAGER_IMPL_H_
