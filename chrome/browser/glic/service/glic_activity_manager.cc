// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/service/glic_activity_manager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/notifications/glic_actor_task_notification_handler.h"
#endif

namespace glic {

using actor::ActorTask;
using TaskState = actor::ActorTask::State;
using TaskDuration = actor::ActorTask::TaskDuration;

namespace {

enum class ActivityPriority {
  kUnseenNeedsAttention,
  kSeenNeedsAttention,
  kInProgress,
  kUnseen,
  kDefault,
};

ActivityPriority GetActivityPriority(const GlicActivityItem& item) {
  if (item.type() == GlicActivityType::kConversationResponse) {
    if (!item.has_been_seen && item.state != GlicActivityState::kInProgress) {
      return ActivityPriority::kUnseenNeedsAttention;
    }
    if (item.state == GlicActivityState::kInProgress) {
      return ActivityPriority::kInProgress;
    }
    return ActivityPriority::kDefault;
  }

  if (item.state == GlicActivityState::kNeedsAttention) {
    return !item.has_been_seen ? ActivityPriority::kUnseenNeedsAttention
                               : ActivityPriority::kSeenNeedsAttention;
  }
  if (item.state == GlicActivityState::kInProgress) {
    return ActivityPriority::kInProgress;
  }
  return !item.has_been_seen ? ActivityPriority::kUnseen
                             : ActivityPriority::kDefault;
}

}  // namespace

// =============================================================================
// GlicActivityManager
// =============================================================================

// static
GlicActivityManager* GlicActivityManager::Get(Profile* profile) {
  if (!base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    return nullptr;
  }
  auto* glic_service = GlicKeyedService::Get(profile);
  return glic_service ? &glic_service->activity_manager() : nullptr;
}

GlicActivityManager::GlicActivityManager(
    Profile* profile,
    GlicInstanceCoordinatorImpl& instance_coordinator)
    : profile_(profile) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActivityHub));
  for (GlicInstanceImpl* instance : instance_coordinator.GetInstances()) {
    OnInstanceCreated(instance);
  }
  instance_created_subscription_ =
      instance_coordinator.AddInstanceCreatedCallback(base::BindRepeating(
          &GlicActivityManager::OnInstanceCreated, base::Unretained(this)));
  if (auto* ui_state_manager = actor::ui::ActorUiStateManager::Get(profile_)) {
    actor_subscriptions_.push_back(
        ui_state_manager->RegisterActorTaskStateChange(
            base::BindRepeating(&GlicActivityManager::OnActorTaskStateUpdate,
                                base::Unretained(this))));
    actor_subscriptions_.push_back(ui_state_manager->RegisterActorTaskStopped(
        base::BindRepeating(&GlicActivityManager::OnActorTaskStateUpdate,
                            base::Unretained(this))));
    actor_subscriptions_.push_back(
        ui_state_manager->RegisterActorTaskRemoved(base::BindRepeating(
            &GlicActivityManager::OnActorTaskRemoved, base::Unretained(this))));
  }
}

GlicActivityManager::~GlicActivityManager() = default;

void GlicActivityManager::OnInstanceActivityStateChanged(
    const GlicInstance& instance,
    GlicActivityState state,
    std::optional<std::string> preview_snippet) {
  auto& instance_state = instance_activity_states_[instance.id()];
  instance_state.state = state;
  instance_state.preview_snippet = std::move(preview_snippet);

  std::optional<std::string> conv_id = instance.conversation_id();
  if (!conv_id || conv_id->empty()) {
    return;
  }
  OnConversationActivityStateChanged(
      *conv_id, {.state = state,
                 .title = instance.conversation_title(),
                 .preview_snippet = instance_state.preview_snippet,
                 .is_currently_visible = instance.IsShowing()});
  if (instance.IsShowing() && state != GlicActivityState::kInProgress) {
    instance_activity_states_.erase(instance.id());
  }
}

void GlicActivityManager::OnInstanceVisibilityChanged(
    const GlicInstance& instance) {
  auto it = instance_activity_states_.find(instance.id());
  if (instance.IsShowing()) {
    if (it != instance_activity_states_.end() &&
        it->second.state != GlicActivityState::kInProgress) {
      instance_activity_states_.erase(it);
    }
    if (auto conv_id = instance.conversation_id()) {
      MarkActivitySeen(
          GlicActivityKey{GlicActivityType::kConversationResponse, *conv_id});
    }
    return;
  }

  if (it == instance_activity_states_.end() ||
      it->second.state != GlicActivityState::kInProgress) {
    return;
  }
  std::optional<std::string> conv_id = instance.conversation_id();
  if (!conv_id || conv_id->empty()) {
    return;
  }
  OnConversationActivityStateChanged(
      *conv_id, {.state = it->second.state,
                 .title = instance.conversation_title(),
                 .preview_snippet = it->second.preview_snippet});
}

void GlicActivityManager::OnInstanceConversationRegistered(
    const GlicInstance& instance) {
  std::optional<std::string> conv_id = instance.conversation_id();
  if (!conv_id || conv_id->empty()) {
    return;
  }
  std::string title = instance.conversation_title();
  auto it = instance_activity_states_.find(instance.id());
  if (it != instance_activity_states_.end()) {
    OnConversationActivityStateChanged(
        *conv_id, {.state = it->second.state,
                   .title = title,
                   .preview_snippet = it->second.preview_snippet,
                   .is_currently_visible = instance.IsShowing()});
    if (instance.IsShowing() &&
        it->second.state != GlicActivityState::kInProgress) {
      instance_activity_states_.erase(it);
    }
    return;
  }
  if (!title.empty()) {
    OnConversationTitleUpdated(*conv_id, title);
  }
}

void GlicActivityManager::OnInstanceResponseStarted(
    const GlicInstance& instance) {
  OnInstanceActivityStateChanged(instance, GlicActivityState::kInProgress,
                                 std::nullopt);
}

void GlicActivityManager::OnInstanceResponseStopped(
    const GlicInstance& instance,
    mojom::ResponseStopCause cause) {
  switch (cause) {
    case mojom::ResponseStopCause::kUnknown:
      OnInstanceActivityStateChanged(instance, GlicActivityState::kCompleted,
                                     std::nullopt);
      break;
    case mojom::ResponseStopCause::kUser:
      ClearInstanceActivity(instance);
      break;
    case mojom::ResponseStopCause::kOther:
      OnInstanceActivityStateChanged(instance, GlicActivityState::kFailed,
                                     std::nullopt);
      break;
  }
}

void GlicActivityManager::OnInstanceWebClientCleared(
    const GlicInstance& instance) {
  ClearInstanceActivity(instance);
}

void GlicActivityManager::OnInstanceTabAddedToTask(
    const GlicInstance& /*instance*/,
    actor::TaskId task_id) {
  OnTabAddedToTask(task_id);
}

void GlicActivityManager::ClearInstanceActivity(const GlicInstance& instance) {
  instance_activity_states_.erase(instance.id());
  if (auto conv_id = instance.conversation_id()) {
    OnConversationDestroyed(*conv_id);
  }
}

void GlicActivityManager::OnInstanceDestroyed(GlicInstanceImpl* instance) {
  if (instance_observations_.IsObservingSource(instance)) {
    instance_observations_.RemoveObservation(instance);
  }
  ClearInstanceActivity(*instance);
}

void GlicActivityManager::OnInstanceCreated(GlicInstanceImpl* instance) {
  if (!instance_observations_.IsObservingSource(instance)) {
    instance_observations_.AddObservation(instance);
  }
}

void GlicActivityManager::OnConversationActivityStateChanged(
    const std::string& conversation_id,
    GlicConversationActivityUpdate update) {
  if (conversation_id.empty()) {
    return;
  }
  GlicActivityKey key{GlicActivityType::kConversationResponse, conversation_id};

  auto [it, inserted] = activities_.try_emplace(key, key, update.state);
  auto& item = it->second;
  item.state = update.state;
  item.timestamp = base::Time::Now();
  if (!update.title.empty()) {
    item.title = base::UTF8ToUTF16(update.title);
  }
  if (update.preview_snippet.has_value()) {
    item.subtitle = base::UTF8ToUTF16(*update.preview_snippet);
  } else if (update.state == GlicActivityState::kInProgress ||
             update.state == GlicActivityState::kCompleted ||
             update.state == GlicActivityState::kFailed) {
    item.subtitle.clear();
  }

  item.has_been_seen = update.is_currently_visible;
  if (!update.is_currently_visible &&
      (update.state == GlicActivityState::kNeedsAttention ||
       update.state == GlicActivityState::kCompleted ||
       update.state == GlicActivityState::kFailed)) {
    activity_alert_callback_list_.Notify(
        ActivityInterruptionLevel::kPassiveNudge);
  }

  PopulateActivityItemDetails(item);

  UpdateTaskNudge();
}

void GlicActivityManager::OnConversationTitleUpdated(
    const std::string& conversation_id,
    std::string_view title) {
  GlicActivityKey key{GlicActivityType::kConversationResponse, conversation_id};
  auto it = activities_.find(key);
  if (it != activities_.end()) {
    it->second.title = base::UTF8ToUTF16(title);
    activity_alert_callback_list_.Notify(ActivityInterruptionLevel::kSilent);
  }
}

void GlicActivityManager::OnConversationDestroyed(
    const std::string& conversation_id) {
  GlicActivityKey key{GlicActivityType::kConversationResponse, conversation_id};
  activities_.erase(key);
  UpdateTaskNudge();
}

void GlicActivityManager::RefreshActivities() {
  for (auto& [key, item] : activities_) {
    PopulateActivityItemDetails(item);
  }
}

std::vector<const GlicActivityItem*> GlicActivityManager::GetSortedActivities(
    const std::optional<std::string>& active_tab_conv_id,
    bool is_active_tab_showing) const {
  std::vector<const GlicActivityItem*> sorted_items;
  sorted_items.reserve(activities_.size());
  for (const auto& [key, item] : activities_) {
    if (item.type() == GlicActivityType::kConversationResponse &&
        active_tab_conv_id.has_value() && key.id == *active_tab_conv_id &&
        (is_active_tab_showing ||
         (item.has_been_seen &&
          item.state != GlicActivityState::kInProgress))) {
      continue;
    }
    sorted_items.push_back(&item);
  }

  std::sort(sorted_items.begin(), sorted_items.end(),
            [](const GlicActivityItem* a, const GlicActivityItem* b) {
              ActivityPriority prio_a = GetActivityPriority(*a);
              ActivityPriority prio_b = GetActivityPriority(*b);
              return std::tie(prio_a, a->key.type, a->task_id, a->has_been_seen,
                              b->timestamp, a->key.id) <
                     std::tie(prio_b, b->key.type, b->task_id, b->has_been_seen,
                              a->timestamp, b->key.id);
            });

  return sorted_items;
}

GlicActivityNudgeState GlicActivityManager::GetCurrentActivityNudgeState()
    const {
  GlicActivityNudgeState state;
  size_t needs_attention_count = 0;
  size_t complete_tasks_count = 0;
  size_t response_ready_count = 0;

  for (const auto& [key, item] : activities_) {
    if (!item.has_been_seen) {
      if (item.state == GlicActivityState::kNeedsAttention) {
        needs_attention_count++;
      } else if (item.state == GlicActivityState::kCompleted ||
                 item.state == GlicActivityState::kFailed) {
        if (item.type() == GlicActivityType::kActorTask) {
          complete_tasks_count++;
        } else {
          response_ready_count++;
        }
      }
    }
  }

  state.has_attention_badge = needs_attention_count > 0;

  if (needs_attention_count > 0) {
    state.text = GlicActivityNudgeState::Text::kNeedsAttention;
    state.count = needs_attention_count;
  } else if (complete_tasks_count > 0) {
    state.text = GlicActivityNudgeState::Text::kCompleteTasks;
    state.count = complete_tasks_count;
  } else if (response_ready_count > 0) {
    state.text = GlicActivityNudgeState::Text::kResponseReady;
    state.count = response_ready_count;
  } else {
    state.text = GlicActivityNudgeState::Text::kDefault;
    state.count = 0;
  }

  return state;
}

size_t GlicActivityManager::GetNumUnseenActivities() const {
  size_t count = 0;
  for (const auto& [key, item] : activities_) {
    if (!item.has_been_seen &&
        (item.state == GlicActivityState::kCompleted ||
         item.state == GlicActivityState::kNeedsAttention ||
         item.state == GlicActivityState::kFailed)) {
      count++;
    }
  }
  return count;
}

bool GlicActivityManager::HasUnseenOrInProgressActivities() const {
  for (const auto& [key, item] : activities_) {
    if (!item.has_been_seen || item.state == GlicActivityState::kInProgress) {
      return true;
    }
  }
  return false;
}

base::CallbackListSubscription
GlicActivityManager::RegisterActivityNudgeStateChange(
    ActivityNudgeChangeCallback callback) {
  return activity_nudge_state_change_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicActivityManager::RegisterActivityAlertCallback(
    ActivityAlertCallback callback) {
  return activity_alert_callback_list_.Add(std::move(callback));
}

void GlicActivityManager::MarkActivitySeen(const GlicActivityKey& key) {
  auto it = activities_.find(key);
  if (it == activities_.end()) {
    return;
  }
  it->second.has_been_seen = true;
  PopulateActivityItemDetails(it->second);
  UpdateTaskNudge();
}

void GlicActivityManager::MarkActivitySeen(actor::TaskId task_id) {
  MarkActivitySeen(GlicActivityKey{GlicActivityType::kActorTask,
                                   base::NumberToString(task_id.value())});
}

void GlicActivityManager::OnTabAddedToTask(actor::TaskId task_id) {
  OnActorTaskStateUpdate(task_id);
}

void GlicActivityManager::UpdateTaskNudge(bool show_bubble) {
  GlicActivityNudgeState old_state = current_activity_nudge_state_;
  current_activity_nudge_state_ = GetCurrentActivityNudgeState();

  bool state_changed = (old_state != current_activity_nudge_state_);
  bool size_changed = (last_notified_activity_count_ != activities_.size());

  if (state_changed || show_bubble || size_changed) {
    last_notified_activity_count_ = activities_.size();
    activity_nudge_state_change_callback_list_.Notify(
        show_bubble, current_activity_nudge_state_);
  }
}

void GlicActivityManager::Shutdown() {
  instance_created_subscription_ = {};
  instance_observations_.RemoveAllObservations();
  actor_subscriptions_.clear();
  instance_activity_states_.clear();
  tasks_notified_of_start_.clear();
  last_actor_task_states_.clear();
  activities_.clear();
}

void GlicActivityManager::OnActorTaskStateUpdate(actor::TaskId task_id) {
  auto* ui_state_manager = actor::ui::ActorUiStateManager::Get(profile_);
  if (!ui_state_manager) {
    return;
  }

  const auto state = ui_state_manager->GetActorTaskState(task_id);
  if (!state.has_value() ||
      state.value() == actor::ActorTask::State::kCancelled) {
    GlicActivityKey key{GlicActivityType::kActorTask,
                        base::NumberToString(task_id.value())};
    activities_.erase(key);
    tasks_notified_of_start_.erase(task_id);
    last_actor_task_states_.erase(task_id);
#if !BUILDFLAG(IS_ANDROID)
    GlicActorTaskNotificationHandler::Close(profile_, task_id);
#endif
    UpdateTaskNudge();
    return;
  }

  glic::mojom::FeatureMode feature_mode =
      ui_state_manager->GetFeatureMode(task_id);
  bool is_active_universal_cart_task =
      IsActiveUniversalCartTask(state.value(), feature_mode);
  bool is_active_task = IsActiveExperimentalTask(state.value(), feature_mode) ||
                        is_active_universal_cart_task ||
                        IsActivePasswordChangeTask(state.value(), feature_mode);

  if (is_active_universal_cart_task) {
    auto task_tab = ui_state_manager->GetLastActedOnTab(task_id);
    bool has_tab = task_tab && *task_tab;
    if (!has_tab) {
      return;
    }
  }

  const auto duration = ui_state_manager->GetDuration(task_id);
  bool requires_processing =
      RequiresTaskProcessing(state.value(), feature_mode);

  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  std::optional<GlicActivityState> activity_state;
  if (RequiresAttention(state.value())) {
    activity_state = GlicActivityState::kNeedsAttention;
  } else if (state.value() == actor::ActorTask::State::kFinished) {
    activity_state = GlicActivityState::kCompleted;
  } else if (state.value() == actor::ActorTask::State::kFailed) {
    activity_state = GlicActivityState::kFailed;
  } else if (state.value() == actor::ActorTask::State::kPausedByUser) {
    activity_state = GlicActivityState::kPaused;
  } else if (state.value() == actor::ActorTask::State::kCreated ||
             state.value() == actor::ActorTask::State::kActing ||
             state.value() == actor::ActorTask::State::kReflecting) {
    activity_state = GlicActivityState::kInProgress;
  }

  bool actor_state_changed = false;
  if (activity_state.has_value()) {
    auto [it, inserted] = activities_.try_emplace(key, key, *activity_state);
    auto& item = it->second;
    auto prev_state_it = last_actor_task_states_.find(task_id);
    actor_state_changed = prev_state_it == last_actor_task_states_.end() ||
                          prev_state_it->second != state.value();
    last_actor_task_states_[task_id] = state.value();
    item.task_id = task_id;
    item.state = *activity_state;
    item.timestamp = base::Time::Now();
    if (inserted || actor_state_changed || !requires_processing) {
      item.has_been_seen = !requires_processing;
    }
    PopulateActivityItemDetails(item);
  } else {
    activities_.erase(key);
    last_actor_task_states_.erase(task_id);
  }

  if (is_active_task) {
    if (tasks_notified_of_start_.insert(task_id).second) {
      activity_alert_callback_list_.Notify(
          ActivityInterruptionLevel::kDelayedTaskStart);
#if !BUILDFLAG(IS_ANDROID)
      if (IsActiveExperimentalTask(state.value(), feature_mode)) {
        GlicActorTaskNotificationHandler::MaybeShow(profile_, task_id);
      }
#endif
    }
  } else {
#if !BUILDFLAG(IS_ANDROID)
    if (feature_mode == glic::mojom::FeatureMode::kExperimentalTriggering) {
      GlicActorTaskNotificationHandler::Close(profile_, task_id);
    }
#endif
    if (RequiresAttention(state.value()) ||
        ShouldShowBubble(state.value(), duration, feature_mode)) {
      activity_alert_callback_list_.Notify(
          ActivityInterruptionLevel::kPassiveNudge);
    }
  }

  const bool show_bubble =
      actor_state_changed && requires_processing &&
      ShouldShowBubble(state.value(), duration, feature_mode);
  UpdateTaskNudge(show_bubble);
}

void GlicActivityManager::PopulateActivityItemDetails(GlicActivityItem& item) {
  if (item.type() == GlicActivityType::kActorTask) {
    if (item.task_id.has_value()) {
      PopulateActorTaskItemDetails(*item.task_id, item);
    }
  } else if (item.type() == GlicActivityType::kConversationResponse) {
    if (item.has_been_seen && item.state != GlicActivityState::kInProgress) {
      item.subtitle.clear();
    }
    if (!item.has_been_seen && item.state == GlicActivityState::kCompleted) {
      item.icon = &(features::IsRoundedIconsEnabled() ? kTaskSparkIcon
                                                      : kTaskSparkOldIcon);
    } else {
      item.icon = &vector_icons::kChatSparkIcon;
    }
  }
}

void GlicActivityManager::PopulateActorTaskItemDetails(actor::TaskId task_id,
                                                       GlicActivityItem& item) {
  auto* ui_state_manager = actor::ui::ActorUiStateManager::Get(profile_);
  if (!ui_state_manager) {
    return;
  }
  auto task_state = ui_state_manager->GetActorTaskState(task_id);
  if (!task_state.has_value()) {
    return;
  }
  auto feature_mode = ui_state_manager->GetFeatureMode(task_id);
  auto task_title = ui_state_manager->GetActorTaskTitle(task_id);
  auto task_tab = ui_state_manager->GetLastActedOnTab(task_id);
  auto task_interrupt_reason =
      ui_state_manager->GetActorTaskInterruptReason(task_id);

  bool has_tab = task_tab.has_value() && task_tab.value() != nullptr;
  if (!has_tab && IsActiveExperimentalTask(*task_state, feature_mode)) {
    has_tab = true;
  }
  item.has_tab = has_tab;

  const bool is_acting_or_reflecting = (*task_state == TaskState::kActing ||
                                        *task_state == TaskState::kReflecting);
  const bool is_processed_tab_closed =
      !is_acting_or_reflecting && !has_tab && item.has_been_seen;
  item.is_enabled = !is_processed_tab_closed;

  const bool show_consent_override =
      (has_tab || is_acting_or_reflecting) && RequiresAttention(*task_state) &&
      task_interrupt_reason ==
          ActorTask::InterruptReason::kWaitingForExperimentalTriggeringConsent;

  if (show_consent_override) {
    item.title =
        l10n_util::GetStringUTF16(IDS_GLIC_TASK_WAITING_FOR_CONSENT_TITLE);
  } else if (task_title.has_value()) {
    item.title = base::UTF8ToUTF16(*task_title);
  }

  if (show_consent_override) {
    item.subtitle =
        l10n_util::GetStringUTF16(IDS_GLIC_TASK_WAITING_FOR_CONSENT_SUBTITLE);
  } else if (!has_tab && !is_acting_or_reflecting) {
    item.subtitle = l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_TAB_CLOSED_SUBTITLE);
  } else if (RequiresAttention(*task_state)) {
    item.subtitle = l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE);
  } else if (*task_state == TaskState::kFinished) {
    item.subtitle = l10n_util::GetStringUTF16(
        feature_mode == glic::mojom::FeatureMode::kExperimentalTriggering
            ? IDS_EXPERIMENTAL_TRIGGERING_TASK_LIST_BUBBLE_ROW_COMPLETED_TASK_SUBTITLE
            : IDS_ACTOR_TASK_LIST_BUBBLE_ROW_COMPLETED_TASK_SUBTITLE);
  } else if (*task_state == TaskState::kFailed) {
    item.subtitle = l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_FAILED_TASK_SUBTITLE);
  } else if (*task_state == TaskState::kPausedByUser) {
    item.subtitle = l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_PAUSED_TASK_SUBTITLE);
  } else {
    item.subtitle = l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_ACTING_TASK_SUBTITLE);
  }

  if (RequiresAttention(*task_state)) {
    item.icon = &(features::IsRoundedIconsEnabled() ? kHourglassIcon
                                                    : kHourglassOldIcon);
  } else if (*task_state == TaskState::kFinished) {
    item.icon = &(features::IsRoundedIconsEnabled() ? kTaskSparkIcon
                                                    : kTaskSparkOldIcon);
  } else {
#if !BUILDFLAG(IS_ANDROID)
    item.icon =
        &GlicVectorIconManager::GetVectorIcon(IDR_ACTOR_AUTO_BROWSE_ICON);
#else
    item.icon = &gfx::VectorIcon::EmptyIcon();
#endif
  }
}

void GlicActivityManager::OnActorTaskRemoved(actor::TaskId task_id) {
  GlicActivityKey key{GlicActivityType::kActorTask,
                      base::NumberToString(task_id.value())};
  activities_.erase(key);
  tasks_notified_of_start_.erase(task_id);
  last_actor_task_states_.erase(task_id);
#if !BUILDFLAG(IS_ANDROID)
  GlicActorTaskNotificationHandler::Close(profile_, task_id);
#endif
  UpdateTaskNudge();
}

// static
bool GlicActivityManager::RequiresAttention(TaskState state) {
  return state == TaskState::kPausedByActor ||
         state == TaskState::kWaitingOnUser;
}

// static
bool GlicActivityManager::RequiresTaskProcessing(
    TaskState state,
    glic::mojom::FeatureMode feature_mode) {
  if (ShouldSuppressNotification(feature_mode, state)) {
    return false;
  }
  return GlicActivityManager::RequiresAttention(state) ||
         state == TaskState::kFinished || state == TaskState::kFailed ||
         IsActiveExperimentalTask(state, feature_mode) ||
         IsActiveUniversalCartTask(state, feature_mode) ||
         IsActivePasswordChangeTask(state, feature_mode);
}

// static
bool GlicActivityManager::ShouldSuppressNotification(
    glic::mojom::FeatureMode feature_mode,
    TaskState state) {
  return base::FeatureList::IsEnabled(
             features::kGlicExperimentalTriggeringSuppressDoneNotification) &&
         feature_mode == glic::mojom::FeatureMode::kExperimentalTriggering &&
         (state == TaskState::kFinished || state == TaskState::kFailed);
}

// static
bool GlicActivityManager::IsActiveExperimentalTask(
    TaskState state,
    glic::mojom::FeatureMode feature_mode) {
  return feature_mode == glic::mojom::FeatureMode::kExperimentalTriggering &&
         (state == TaskState::kActing || state == TaskState::kReflecting);
}

// static
bool GlicActivityManager::IsActiveUniversalCartTask(
    TaskState state,
    glic::mojom::FeatureMode feature_mode) {
  return feature_mode == glic::mojom::FeatureMode::kUniversalCart &&
         (state == TaskState::kActing || state == TaskState::kReflecting);
}

// static
bool GlicActivityManager::IsActivePasswordChangeTask(
    TaskState state,
    glic::mojom::FeatureMode feature_mode) {
  return feature_mode == glic::mojom::FeatureMode::kPasswordChange &&
         (state == TaskState::kActing || state == TaskState::kReflecting);
}

// static
bool GlicActivityManager::ShouldShowBubble(
    TaskState state,
    TaskDuration duration,
    glic::mojom::FeatureMode feature_mode) {
  if (GlicActivityManager::RequiresAttention(state)) {
    return true;
  }
  if (ShouldSuppressNotification(feature_mode, state)) {
    return false;
  }
  return (state == TaskState::kFinished || state == TaskState::kFailed) &&
         duration != ActorTask::TaskDuration::kTransient;
}

}  // namespace glic
