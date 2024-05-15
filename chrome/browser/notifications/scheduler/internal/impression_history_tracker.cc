// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

namespace notifications {
namespace {

size_t GetDismissCount(const ThrottleConfig* custom_throttle_config,
                       const SchedulerConfig& config) {
  if (custom_throttle_config &&
      custom_throttle_config->negative_action_count_threshold.has_value()) {
    return custom_throttle_config->negative_action_count_threshold.value();
  }
  return config.dismiss_count;
}

base::TimeDelta GetSuppressionDuration(
    const ThrottleConfig* custom_throttle_config,
    const SchedulerConfig& config) {
  if (custom_throttle_config &&
      custom_throttle_config->suppression_duration.has_value()) {
    return custom_throttle_config->suppression_duration.value();
  }
  return config.suppression_duration;
}

std::string ToDatabaseKey(SchedulerClientType type) {
  switch (type) {
    case SchedulerClientType::kTest1:
      return "Test1";
    case SchedulerClientType::kTest2:
      return "Test2";
    case SchedulerClientType::kTest3:
      return "Test3";
    case SchedulerClientType::kUnknown:
    case SchedulerClientType::kDeprecatedFeatureGuide:
      NOTREACHED_IN_MIGRATION();
      return std::string();
    case SchedulerClientType::kWebUI:
      return "WebUI";
    case SchedulerClientType::kChromeUpdate:
      return "ChromeUpdate";
    case SchedulerClientType::kPrefetch:
      return "Prefetch";
    case SchedulerClientType::kReadingList:
      return "ReadingList";
  }
}

}  // namespace

ImpressionHistoryTrackerImpl::ImpressionHistoryTrackerImpl(
    const SchedulerConfig& config,
    std::vector<SchedulerClientType> registered_clients,
    std::unique_ptr<CollectionStore<ClientState>> store,
    base::Clock* clock)
    : store_(std::move(store)),
      config_(config),
      registered_clients_(std::move(registered_clients)),
      initialized_(false),
      clock_(clock),
      delegate_(nullptr) {}

ImpressionHistoryTrackerImpl::~ImpressionHistoryTrackerImpl() = default;

void ImpressionHistoryTrackerImpl::Init(Delegate* delegate,
                                        InitCallback callback) {
  DCHECK(delegate && !delegate_);
  delegate_ = delegate;
  store_->InitAndLoad(
      base::BindOnce(&ImpressionHistoryTrackerImpl::OnStoreInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImpressionHistoryTrackerImpl::AddImpression(
    SchedulerClientType type,
    const std::string& guid,
    const Impression::ImpressionResultMap& impression_mapping,
    const Impression::CustomData& custom_data,
    std::optional<base::TimeDelta> ignore_timeout_duration) {
  DCHECK(initialized_);
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return;

  Impression impression(type, guid, clock_->Now());
  impression.impression_mapping = impression_mapping;
  impression.custom_data = custom_data;
  impression.ignore_timeout_duration = ignore_timeout_duration;
  it->second->impressions.emplace_back(std::move(impression));
  it->second->last_shown_ts = clock_->Now();
  SetNeedsUpdate(type, true /*needs_update*/);
  MaybeUpdateDb(type);
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory() {
  DCHECK(initialized_);
  for (auto& client_state : client_states_)
    AnalyzeImpressionHistory(client_state.second.get());
  MaybeUpdateAllDb();
}

void ImpressionHistoryTrackerImpl::GetClientStates(
    std::map<SchedulerClientType, const ClientState*>* client_states) const {
  DCHECK(initialized_);
  DCHECK(client_states);
  client_states->clear();
  for (const auto& pair : client_states_) {
    client_states->emplace(pair.first, pair.second.get());
  }
}

const Impression* ImpressionHistoryTrackerImpl::GetImpression(
    SchedulerClientType type,
    const std::string& guid) {
  return GetImpressionInternal(type, guid);
}

void ImpressionHistoryTrackerImpl::GetImpressionDetail(
    SchedulerClientType type,
    ImpressionDetail::ImpressionDetailCallback callback) {
  DCHECK(initialized_);
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return;

  auto* state = it->second.get();
  int num_notification_shown_today =
      notifications::NotificationsShownToday(state);
  ImpressionDetail detail(state->current_max_daily_show,
                          num_notification_shown_today,
                          state->negative_events_count,
                          state->last_negative_event_ts, state->last_shown_ts);
  std::move(callback).Run(std::move(detail));
}

void ImpressionHistoryTrackerImpl::OnUserAction(
    const UserActionData& action_data) {
  auto button_type = action_data.button_click_info.has_value()
                         ? action_data.button_click_info->type
                         : ActionButtonType::kUnknownAction;
  switch (action_data.action_type) {
    case UserActionType::kClick:
      OnClickInternal(action_data.client_type, action_data.guid,
                      true /*update_db*/);
      break;
    case UserActionType::kButtonClick:
      OnButtonClickInternal(action_data.client_type, action_data.guid,
                            button_type, true /*update_db*/);
      break;
    case UserActionType::kDismiss:
      OnDismissInternal(action_data.client_type, action_data.guid,
                        true /*update_db*/);
      break;
  }
}

void ImpressionHistoryTrackerImpl::OnStoreInitialized(
    InitCallback callback,
    bool success,
    CollectionStore<ClientState>::Entries entries) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  initialized_ = true;

  // Load the data to memory, and prune expired impressions.
  auto now = clock_->Now();
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    auto& entry = (*it);
    auto type = entry->type;
    ClientState::Impressions impressions;
    for (auto& impression : entry->impressions) {
      bool expired =
          now - impression.create_time > config_->impression_expiration;
      if (expired) {
        SetNeedsUpdate(type, true);
      } else {
        impressions.emplace_back(impression);
      }
    }
    entry->impressions.swap(impressions);
    client_states_.emplace(type, std::move(*it));
    MaybeUpdateDb(type);
  }

  SyncRegisteredClients();
  std::move(callback).Run(true);
}

void ImpressionHistoryTrackerImpl::SyncRegisteredClients() {
  // Remove deprecated clients.
  for (auto it = client_states_.begin(); it != client_states_.end();) {
    auto client_type = it->first;
    if (!base::Contains(registered_clients_, client_type)) {
      store_->Delete(ToDatabaseKey(client_type),
                     /*callback=*/base::DoNothing());
      client_states_.erase(it++);
      continue;
    } else {
      it++;
    }
  }

  // Add new data for new registered client.
  for (const auto type : registered_clients_) {
    if (client_states_.find(type) == client_states_.end()) {
      auto new_client_data = CreateNewClientState(type, *config_);

      DCHECK(new_client_data);
      store_->Add(ToDatabaseKey(type), *new_client_data.get(),
                  /*callback=*/base::DoNothing());
      client_states_.emplace(type, std::move(new_client_data));
    }
  }
}

void ImpressionHistoryTrackerImpl::HandleIgnoredImpressions(
    ClientState* client_state) {
  for (auto& it : client_state->impressions) {
    auto* impression = &it;
    if (impression->feedback != UserFeedback::kNoFeedback)
      continue;

    if (impression->ignore_timeout_duration.has_value() &&
        impression->create_time + impression->ignore_timeout_duration.value() <=
            clock_->Now())
      impression->feedback = UserFeedback::kIgnore;
  }
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory(
    ClientState* client_state) {
  DCHECK(client_state);
  HandleIgnoredImpressions(client_state);
  base::circular_deque<Impression*> dismisses;
  for (auto it = client_state->impressions.begin();
       it != client_state->impressions.end(); ++it) {
    auto* impression = &*it;
    switch (impression->feedback) {
      case UserFeedback::kDismiss:
      case UserFeedback::kIgnore:
        dismisses.emplace_back(impression);
        PruneImpressionByCreateTime(
            &dismisses, impression->create_time - config_->dismiss_duration);
        CheckConsecutiveDismiss(client_state, &dismisses);
        break;
      case UserFeedback::kClick:
        OnClickInternal(client_state->type, impression->guid,
                        false /*update_db*/);
        break;
      case UserFeedback::kHelpful:
        OnButtonClickInternal(client_state->type, impression->guid,
                              ActionButtonType::kHelpful, false /*update_db*/);
        break;
      case UserFeedback::kNotHelpful:
        OnButtonClickInternal(client_state->type, impression->guid,
                              ActionButtonType::kUnhelpful,
                              false /*update_db*/);
        break;
      case UserFeedback::kNoFeedback:
        [[fallthrough]];
      default:
        // The user didn't interact with the notification yet.
        continue;
    }
  }

  // Check suppression expiration.
  CheckSuppressionExpiration(client_state);
}

// static
void ImpressionHistoryTrackerImpl::PruneImpressionByCreateTime(
    base::circular_deque<Impression*>* impressions,
    const base::Time& start_time) {
  DCHECK(impressions);
  while (!impressions->empty()) {
    if (impressions->front()->create_time > start_time)
      break;
    // Anything created before |start_time| is considered to have no effect
    // and will never be processed again.
    impressions->front()->integrated = true;
    impressions->pop_front();
  }
}

void ImpressionHistoryTrackerImpl::GenerateImpressionResult(
    Impression* impression) {
  DCHECK(impression);
  auto it = impression->impression_mapping.find(impression->feedback);
  if (it != impression->impression_mapping.end()) {
    // Use client defined impression mapping.
    impression->impression = it->second;
  } else {
    // Use default mapping from user feedback to impression result.
    switch (impression->feedback) {
      case UserFeedback::kClick:
      case UserFeedback::kHelpful:
        impression->impression = ImpressionResult::kPositive;
        break;
      case UserFeedback::kDismiss:
      case UserFeedback::kIgnore:
        impression->impression = ImpressionResult::kNeutral;
        break;
      case UserFeedback::kNotHelpful:
        impression->impression = ImpressionResult::kNegative;
        break;
      case UserFeedback::kNoFeedback:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

void ImpressionHistoryTrackerImpl::UpdateThrottling(ClientState* client_state,
                                                    Impression* impression) {
  DCHECK(client_state);
  DCHECK(impression);

  // Affect the notification throttling.
  switch (impression->impression) {
    case ImpressionResult::kPositive:
      ApplyPositiveImpression(client_state, impression);
      break;
    case ImpressionResult::kNegative:
      ApplyNegativeImpression(client_state, impression);
      break;
    case ImpressionResult::kNeutral:
      break;
    case ImpressionResult::kInvalid:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void ImpressionHistoryTrackerImpl::CheckConsecutiveDismiss(
    ClientState* client_state,
    base::circular_deque<Impression*>* impressions) {
  delegate_->GetThrottleConfig(
      client_state->type,
      base::BindOnce(
          &ImpressionHistoryTrackerImpl::OnCustomNegativeActionCountQueried,
          weak_ptr_factory_.GetWeakPtr(), client_state->type, impressions));
}

void ImpressionHistoryTrackerImpl::OnCustomNegativeActionCountQueried(
    SchedulerClientType type,
    base::circular_deque<Impression*>* impressions,
    std::unique_ptr<ThrottleConfig> custom_throttle_config) {
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return;
  ClientState* client_state = it->second.get();
  size_t num_actions = GetDismissCount(custom_throttle_config.get(), *config_);
  if (impressions->size() < num_actions)
    return;

  // Suppress the notification if the user performed consecutive operations
  // that generates negative impressions.
  for (auto* impression : *impressions) {
    DCHECK(impression->feedback == UserFeedback::kDismiss ||
           impression->feedback == UserFeedback::kIgnore);
    if (impression->integrated)
      continue;

    impression->integrated = true;
    SetNeedsUpdate(client_state->type, true);
    GenerateImpressionResult(impression);
  }
}

void ImpressionHistoryTrackerImpl::ApplyPositiveImpression(
    ClientState* client_state,
    Impression* impression) {
  DCHECK(impression);
  if (impression->integrated)
    return;

  DCHECK_EQ(impression->impression, ImpressionResult::kPositive);
  SetNeedsUpdate(client_state->type, true);
  impression->integrated = true;

  // A positive impression directly releases the suppression.
  if (client_state->suppression_info.has_value()) {
    client_state->current_max_daily_show =
        client_state->suppression_info->recover_goal;
    client_state->suppression_info.reset();
    return;
  }

  // Increase |current_max_daily_show| by 1.
  client_state->current_max_daily_show =
      std::clamp(client_state->current_max_daily_show + 1, 0,
                  config_->max_daily_shown_per_type);
}

void ImpressionHistoryTrackerImpl::ApplyNegativeImpression(
    ClientState* client_state,
    Impression* impression) {
  if (impression->integrated)
    return;

  DCHECK_EQ(impression->impression, ImpressionResult::kNegative);
  SetNeedsUpdate(client_state->type, true);
  impression->integrated = true;

  delegate_->GetThrottleConfig(
      client_state->type,
      base::BindOnce(
          &ImpressionHistoryTrackerImpl::OnCustomSuppressionDurationQueried,
          weak_ptr_factory_.GetWeakPtr(), client_state->type));
}

void ImpressionHistoryTrackerImpl::OnCustomSuppressionDurationQueried(
    SchedulerClientType type,
    std::unique_ptr<ThrottleConfig> custom_throttle_config) {
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return;
  ClientState* client_state = it->second.get();
  auto now = clock_->Now();
  // Suppress the notification, the user will not see this type of
  // notification for a while.
  SuppressionInfo supression_info(
      now, GetSuppressionDuration(custom_throttle_config.get(), *config_));
  client_state->suppression_info = std::move(supression_info);
  client_state->current_max_daily_show = 0;
  client_state->last_negative_event_ts = now;
  client_state->negative_events_count++;
}

void ImpressionHistoryTrackerImpl::CheckSuppressionExpiration(
    ClientState* client_state) {
  // No suppression to recover from.
  if (!client_state->suppression_info.has_value())
    return;

  SuppressionInfo& suppression = client_state->suppression_info.value();
  base::Time now = clock_->Now();

  // Still in the suppression time window.
  if (now - suppression.last_trigger_time < suppression.duration)
    return;

  // Recover from suppression and increase |current_max_daily_show|.
  DCHECK_EQ(client_state->current_max_daily_show, 0);
  client_state->current_max_daily_show = suppression.recover_goal;

  // Clear suppression if fully recovered.
  client_state->suppression_info.reset();
  SetNeedsUpdate(client_state->type, true);
}

bool ImpressionHistoryTrackerImpl::MaybeUpdateDb(SchedulerClientType type) {
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return false;

  bool db_updated = false;
  if (NeedsUpdate(type)) {
    store_->Update(ToDatabaseKey(type), *(it->second.get()),
                   /*callback=*/base::DoNothing());
    db_updated = true;
  }
  SetNeedsUpdate(type, false);
  return db_updated;
}

bool ImpressionHistoryTrackerImpl::MaybeUpdateAllDb() {
  bool db_updated = false;
  for (const auto& client_state : client_states_) {
    auto type = client_state.second->type;
    db_updated |= MaybeUpdateDb(type);
  }

  return db_updated;
}

void ImpressionHistoryTrackerImpl::SetNeedsUpdate(SchedulerClientType type,
                                                  bool needs_update) {
  need_update_db_[type] = needs_update;
}

bool ImpressionHistoryTrackerImpl::NeedsUpdate(SchedulerClientType type) const {
  auto it = need_update_db_.find(type);
  return it == need_update_db_.end() ? false : it->second;
}

Impression* ImpressionHistoryTrackerImpl::FindImpressionNeedsUpdate(
    SchedulerClientType type,
    const std::string& notification_guid) {
  Impression* impression = GetImpressionInternal(type, notification_guid);
  if (!impression || impression->integrated)
    return nullptr;

  return impression;
}

Impression* ImpressionHistoryTrackerImpl::GetImpressionInternal(
    SchedulerClientType type,
    const std::string& guid) {
  auto it = client_states_.find(type);
  if (it == client_states_.end())
    return nullptr;

  ClientState* client_state = it->second.get();
  for (auto& impression : client_state->impressions) {
    if (impression.guid == guid)
      return &impression;
  }

  return nullptr;
}

void ImpressionHistoryTrackerImpl::OnClickInternal(
    SchedulerClientType type,
    const std::string& notification_guid,
    bool update_db) {
  auto* impression = FindImpressionNeedsUpdate(type, notification_guid);
  if (!impression)
    return;

  auto it = client_states_.find(impression->type);
  if (it == client_states_.end())
    return;
  ClientState* client_state = it->second.get();
  impression->feedback = UserFeedback::kClick;
  GenerateImpressionResult(impression);
  SetNeedsUpdate(impression->type, true);
  UpdateThrottling(client_state, impression);

  if (update_db)
    MaybeUpdateDb(client_state->type);
}

void ImpressionHistoryTrackerImpl::OnButtonClickInternal(
    SchedulerClientType type,
    const std::string& notification_guid,
    ActionButtonType button_type,
    bool update_db) {
  auto* impression = FindImpressionNeedsUpdate(type, notification_guid);
  if (!impression)
    return;
  auto it = client_states_.find(impression->type);
  if (it == client_states_.end())
    return;

  ClientState* client_state = it->second.get();
  switch (button_type) {
    case ActionButtonType::kHelpful:
      impression->feedback = UserFeedback::kHelpful;
      break;
    case ActionButtonType::kUnhelpful:
      impression->feedback = UserFeedback::kNotHelpful;
      break;
    case ActionButtonType::kUnknownAction:
      NOTIMPLEMENTED();
      break;
  }

  GenerateImpressionResult(impression);
  SetNeedsUpdate(impression->type, true);
  UpdateThrottling(client_state, impression);

  if (update_db)
    MaybeUpdateDb(client_state->type);
}

void ImpressionHistoryTrackerImpl::OnDismissInternal(
    SchedulerClientType type,
    const std::string& notification_guid,
    bool update_db) {
  auto* impression = FindImpressionNeedsUpdate(type, notification_guid);
  if (!impression)
    return;

  auto it = client_states_.find(impression->type);
  if (it == client_states_.end())
    return;
  ClientState* client_state = it->second.get();

  impression->feedback = UserFeedback::kDismiss;
  SetNeedsUpdate(impression->type, true);

  // Check consecutive dismisses.
  AnalyzeImpressionHistory(client_state);
  MaybeUpdateDb(impression->type);
}

}  // namespace notifications
