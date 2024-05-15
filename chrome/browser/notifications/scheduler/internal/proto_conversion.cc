// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"

namespace notifications {

namespace {

// Helper method to convert base::TimeDelta to integer for serialization. Loses
// precision beyond miliseconds.
int64_t TimeDeltaToMilliseconds(const base::TimeDelta& delta) {
  return delta.InMilliseconds();
}

// Helper method to convert serialized time delta as integer to base::TimeDelta
// for deserialization. Loses precision beyond miliseconds.
base::TimeDelta MillisecondsToTimeDelta(int64_t serialized_delat_ms) {
  return base::Milliseconds(serialized_delat_ms);
}

// Helper method to convert base::Time to integer for serialization. Loses
// precision beyond miliseconds.
int64_t TimeToMilliseconds(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InMilliseconds();
}

// Helper method to convert serialized time as integer to base::Time for
// deserialization. Loses precision beyond miliseconds.
base::Time MillisecondsToTime(int64_t serialized_time_ms) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(serialized_time_ms));
}

// Converts SchedulerClientType to its associated enum in proto buffer.
proto::SchedulerClientType ToSchedulerClientType(SchedulerClientType type) {
  switch (type) {
    case SchedulerClientType::kTest1:
      return proto::SchedulerClientType::TEST_1;
    case SchedulerClientType::kTest2:
      return proto::SchedulerClientType::TEST_2;
    case SchedulerClientType::kTest3:
      return proto::SchedulerClientType::TEST_3;
    case SchedulerClientType::kUnknown:
    case SchedulerClientType::kDeprecatedFeatureGuide:
      return proto::SchedulerClientType::UNKNOWN;
    case SchedulerClientType::kWebUI:
      return proto::SchedulerClientType::WEBUI;
    case SchedulerClientType::kChromeUpdate:
      return proto::SchedulerClientType::CHROME_UPDATE;
    case SchedulerClientType::kPrefetch:
      return proto::SchedulerClientType::PREFETCH;
    case SchedulerClientType::kReadingList:
      return proto::SchedulerClientType::READING_LIST;
  }
  NOTREACHED_IN_MIGRATION();
}

// Converts SchedulerClientType from its associated enum in proto buffer.
SchedulerClientType FromSchedulerClientType(
    proto::SchedulerClientType proto_type) {
  switch (proto_type) {
    case proto::SchedulerClientType::TEST_1:
      return SchedulerClientType::kTest1;
    case proto::SchedulerClientType::TEST_2:
      return SchedulerClientType::kTest2;
    case proto::SchedulerClientType::TEST_3:
      return SchedulerClientType::kTest3;
    case proto::SchedulerClientType::UNKNOWN:
      return SchedulerClientType::kUnknown;
    case proto::SchedulerClientType::WEBUI:
      return SchedulerClientType::kWebUI;
    case proto::SchedulerClientType::CHROME_UPDATE:
      return SchedulerClientType::kChromeUpdate;
    case proto::SchedulerClientType::PREFETCH:
      return SchedulerClientType::kPrefetch;
    case proto::SchedulerClientType::READING_LIST:
      return SchedulerClientType::kReadingList;
  }
  NOTREACHED_IN_MIGRATION();
}

// Converts UserFeedback to its associated enum in proto buffer.
proto::Impression_UserFeedback ToUserFeedback(UserFeedback feedback) {
  switch (feedback) {
    case UserFeedback::kNoFeedback:
      return proto::Impression_UserFeedback_NO_FEEDBACK;
    case UserFeedback::kHelpful:
      return proto::Impression_UserFeedback_HELPFUL;
    case UserFeedback::kNotHelpful:
      return proto::Impression_UserFeedback_NOT_HELPFUL;
    case UserFeedback::kClick:
      return proto::Impression_UserFeedback_CLICK;
    case UserFeedback::kDismiss:
      return proto::Impression_UserFeedback_DISMISS;
    case UserFeedback::kIgnore:
      return proto::Impression_UserFeedback_IGNORE;
  }
  NOTREACHED_IN_MIGRATION();
}

// Converts UserFeedback from its associated enum in proto buffer.
UserFeedback FromUserFeedback(proto::Impression_UserFeedback feedback) {
  switch (feedback) {
    case proto::Impression_UserFeedback_NO_FEEDBACK:
      return UserFeedback::kNoFeedback;
    case proto::Impression_UserFeedback_HELPFUL:
      return UserFeedback::kHelpful;
    case proto::Impression_UserFeedback_NOT_HELPFUL:
      return UserFeedback::kNotHelpful;
    case proto::Impression_UserFeedback_CLICK:
      return UserFeedback::kClick;
    case proto::Impression_UserFeedback_DISMISS:
      return UserFeedback::kDismiss;
    case proto::Impression_UserFeedback_IGNORE:
      return UserFeedback::kIgnore;
  }
  NOTREACHED_IN_MIGRATION();
}

// Converts ImpressionResult to its associated enum in proto buffer.
proto::Impression_ImpressionResult ToImpressionResult(ImpressionResult result) {
  switch (result) {
    case ImpressionResult::kInvalid:
      return proto::Impression_ImpressionResult_INVALID;
    case ImpressionResult::kPositive:
      return proto::Impression_ImpressionResult_POSITIVE;
    case ImpressionResult::kNegative:
      return proto::Impression_ImpressionResult_NEGATIVE;
    case ImpressionResult::kNeutral:
      return proto::Impression_ImpressionResult_NEUTRAL;
  }
  NOTREACHED_IN_MIGRATION();
}

// Converts ImpressionResult from its associated enum in proto buffer.
ImpressionResult FromImpressionResult(
    proto::Impression_ImpressionResult result) {
  switch (result) {
    case proto::Impression_ImpressionResult_INVALID:
      return ImpressionResult::kInvalid;
    case proto::Impression_ImpressionResult_POSITIVE:
      return ImpressionResult::kPositive;
    case proto::Impression_ImpressionResult_NEGATIVE:
      return ImpressionResult::kNegative;
    case proto::Impression_ImpressionResult_NEUTRAL:
      return ImpressionResult::kNeutral;
  }
  NOTREACHED_IN_MIGRATION();
}

proto::IconType ToIconType(IconType type) {
  switch (type) {
    case IconType::kUnknownType:
      return proto::IconType::UNKNOWN_ICON_TYPE;
    case IconType::kSmallIcon:
      return proto::IconType::SMALL_ICON;
    case IconType::kLargeIcon:
      return proto::IconType::LARGE_ICON;
  }
  NOTREACHED_IN_MIGRATION();
}

IconType FromIconType(proto::IconType proto_type) {
  switch (proto_type) {
    case proto::IconType::UNKNOWN_ICON_TYPE:
      return IconType::kUnknownType;
    case proto::IconType::SMALL_ICON:
      return IconType::kSmallIcon;
    case proto::IconType::LARGE_ICON:
      return IconType::kLargeIcon;
  }
  NOTREACHED_IN_MIGRATION();
}

proto::ActionButtonType ToActionButtonType(ActionButtonType type) {
  switch (type) {
    case ActionButtonType::kUnknownAction:
      return proto::ActionButtonType::UNKNOWN_ACTION;
    case ActionButtonType::kHelpful:
      return proto::ActionButtonType::HELPFUL;
    case ActionButtonType::kUnhelpful:
      return proto::ActionButtonType::UNHELPFUL;
  }
  NOTREACHED_IN_MIGRATION();
}

ActionButtonType FromActionButtonType(proto::ActionButtonType proto_type) {
  switch (proto_type) {
    case proto::ActionButtonType::UNKNOWN_ACTION:
      return ActionButtonType::kUnknownAction;
    case proto::ActionButtonType::HELPFUL:
      return ActionButtonType::kHelpful;
    case proto::ActionButtonType::UNHELPFUL:
      return ActionButtonType::kUnhelpful;
  }
}

// Converts NotificationData to proto buffer type.
void NotificationDataToProto(NotificationData* notification_data,
                             proto::NotificationData* proto) {
  proto->set_title(base::UTF16ToUTF8(notification_data->title));
  proto->set_message(base::UTF16ToUTF8(notification_data->message));
  for (const auto& pair : notification_data->custom_data) {
    auto* data = proto->add_custom_data();
    data->set_key(pair.first);
    data->set_value(pair.second);
  }

  for (const auto& button : notification_data->buttons) {
    auto* proto_button = proto->add_buttons();
    proto_button->set_text(base::UTF16ToUTF8(button.text));
    proto_button->set_button_type(ToActionButtonType(button.type));
    proto_button->set_id(button.id);
  }
}

// Converts NotificationData from proto buffer type.
void NotificationDataFromProto(proto::NotificationData* proto,
                               NotificationData* notification_data) {
  notification_data->title = base::UTF8ToUTF16(proto->title());
  notification_data->message = base::UTF8ToUTF16(proto->message());
  for (int i = 0; i < proto->custom_data_size(); ++i) {
    const auto& pair = proto->custom_data(i);
    notification_data->custom_data.emplace(pair.key(), pair.value());
  }

  for (int i = 0; i < proto->buttons_size(); ++i) {
    NotificationData::Button button;
    const auto& proto_button = proto->buttons(i);
    button.text = base::UTF8ToUTF16(proto_button.text());
    button.type = FromActionButtonType(proto_button.button_type());
    button.id = proto_button.id();
    notification_data->buttons.emplace_back(button);
  }
}

// Converts ScheduleParams::Priority to proto buffer type.
proto::ScheduleParams_Priority ScheduleParamsPriorityToProto(
    ScheduleParams::Priority priority) {
  using Priority = ScheduleParams::Priority;
  switch (priority) {
    case Priority::kLow:
      return proto::ScheduleParams_Priority_LOW;
    case Priority::kNoThrottle:
      return proto::ScheduleParams_Priority_NO_THROTTLE;
  }
}

// Converts ScheduleParams::Priority from proto buffer type.
ScheduleParams::Priority ScheduleParamsPriorityFromProto(
    proto::ScheduleParams_Priority priority) {
  using Priority = ScheduleParams::Priority;
  switch (priority) {
    case proto::ScheduleParams_Priority_LOW:
      return Priority::kLow;
    case proto::ScheduleParams_Priority_NO_THROTTLE:
      return Priority::kNoThrottle;
  }
}

// Converts ScheduleParams to proto buffer type.
void ScheduleParamsToProto(ScheduleParams* params,
                           proto::ScheduleParams* proto) {
  proto->set_priority(ScheduleParamsPriorityToProto(params->priority));

  for (const auto& mapping : params->impression_mapping) {
    auto* proto_impression_mapping = proto->add_impression_mapping();
    proto_impression_mapping->set_user_feedback(ToUserFeedback(mapping.first));
    proto_impression_mapping->set_impression_result(
        ToImpressionResult(mapping.second));
  }

  if (params->deliver_time_start.has_value()) {
    proto->set_deliver_time_start(
        TimeToMilliseconds(params->deliver_time_start.value()));
  }

  if (params->deliver_time_end.has_value()) {
    proto->set_deliver_time_end(
        TimeToMilliseconds(params->deliver_time_end.value()));
  }

  if (params->ignore_timeout_duration.has_value()) {
    proto->set_ignore_timeout_duration(
        TimeDeltaToMilliseconds(params->ignore_timeout_duration.value()));
  }
}

// Converts ScheduleParams from proto buffer type.
void ScheduleParamsFromProto(proto::ScheduleParams* proto,
                             ScheduleParams* params) {
  params->priority = ScheduleParamsPriorityFromProto(proto->priority());

  for (int i = 0; i < proto->impression_mapping_size(); ++i) {
    const auto& proto_impression_mapping = proto->impression_mapping(i);
    auto user_feedback =
        FromUserFeedback(proto_impression_mapping.user_feedback());
    auto impression_result =
        FromImpressionResult(proto_impression_mapping.impression_result());
    params->impression_mapping[user_feedback] = impression_result;
  }
  if (proto->has_deliver_time_start()) {
    params->deliver_time_start =
        MillisecondsToTime(proto->deliver_time_start());
  }
  if (proto->has_deliver_time_end()) {
    params->deliver_time_end = MillisecondsToTime(proto->deliver_time_end());
  }
  if (proto->has_ignore_timeout_duration()) {
    params->ignore_timeout_duration =
        MillisecondsToTimeDelta(proto->ignore_timeout_duration());
  }
}

}  // namespace

void IconEntryToProto(IconEntry* entry, notifications::proto::Icon* proto) {
  proto->mutable_icon()->swap(entry->data);
}

void IconEntryFromProto(proto::Icon* proto, notifications::IconEntry* entry) {
  DCHECK(proto->has_icon());
  entry->data.swap(*proto->mutable_icon());
}

void ClientStateToProto(ClientState* client_state,
                        notifications::proto::ClientState* proto) {
  proto->set_type(ToSchedulerClientType(client_state->type));
  proto->set_current_max_daily_show(client_state->current_max_daily_show);

  for (const auto& impression : client_state->impressions) {
    auto* impression_ptr = proto->add_impressions();
    impression_ptr->set_create_time(TimeToMilliseconds(impression.create_time));
    impression_ptr->set_feedback(ToUserFeedback(impression.feedback));
    impression_ptr->set_impression(ToImpressionResult(impression.impression));
    impression_ptr->set_integrated(impression.integrated);
    impression_ptr->set_guid(impression.guid);

    for (const auto& mapping : impression.impression_mapping) {
      auto* proto_impression_mapping = impression_ptr->add_impression_mapping();
      proto_impression_mapping->set_user_feedback(
          ToUserFeedback(mapping.first));
      proto_impression_mapping->set_impression_result(
          ToImpressionResult(mapping.second));
    }

    for (const auto& pair : impression.custom_data) {
      auto* data = impression_ptr->add_custom_data();
      data->set_key(pair.first);
      data->set_value(pair.second);
    }

    if (impression.ignore_timeout_duration.has_value()) {
      impression_ptr->set_ignore_timeout_duration(
          TimeDeltaToMilliseconds(impression.ignore_timeout_duration.value()));
    }
  }

  if (client_state->suppression_info.has_value()) {
    const auto& suppression = *client_state->suppression_info;
    auto* suppression_proto = proto->mutable_suppression_info();
    suppression_proto->set_last_trigger_time(
        TimeToMilliseconds(suppression.last_trigger_time));
    suppression_proto->set_duration_ms(
        TimeDeltaToMilliseconds(suppression.duration));
    suppression_proto->set_recover_goal(suppression.recover_goal);
  }

  proto->set_negative_events_count(client_state->negative_events_count);

  if (client_state->last_negative_event_ts.has_value()) {
    proto->set_last_negative_event_ts(
        TimeToMilliseconds(client_state->last_negative_event_ts.value()));
  }

  if (client_state->last_shown_ts.has_value()) {
    proto->set_last_shown_ts(
        TimeToMilliseconds(client_state->last_shown_ts.value()));
  }
}

void ClientStateFromProto(proto::ClientState* proto,
                          notifications::ClientState* client_state) {
  DCHECK(proto->has_type());
  DCHECK(proto->has_current_max_daily_show());
  client_state->type = FromSchedulerClientType(proto->type());
  client_state->current_max_daily_show = proto->current_max_daily_show();

  for (const auto& proto_impression : proto->impressions()) {
    Impression impression;
    DCHECK(proto_impression.has_create_time());
    impression.create_time = MillisecondsToTime(proto_impression.create_time());
    impression.feedback = FromUserFeedback(proto_impression.feedback());
    impression.impression = FromImpressionResult(proto_impression.impression());
    impression.integrated = proto_impression.integrated();
    impression.guid = proto_impression.guid();
    impression.type = client_state->type;

    if (proto_impression.has_ignore_timeout_duration())
      impression.ignore_timeout_duration =
          MillisecondsToTimeDelta(proto_impression.ignore_timeout_duration());

    for (int i = 0; i < proto_impression.impression_mapping_size(); ++i) {
      const auto& proto_impression_mapping =
          proto_impression.impression_mapping(i);
      auto user_feedback =
          FromUserFeedback(proto_impression_mapping.user_feedback());
      auto impression_result =
          FromImpressionResult(proto_impression_mapping.impression_result());
      impression.impression_mapping[user_feedback] = impression_result;
    }

    for (int i = 0; i < proto_impression.custom_data_size(); ++i) {
      const auto& pair = proto_impression.custom_data(i);
      impression.custom_data.emplace(pair.key(), pair.value());
    }

    client_state->impressions.emplace_back(std::move(impression));
  }

  if (proto->has_suppression_info()) {
    const auto& proto_suppression = proto->suppression_info();
    DCHECK(proto_suppression.has_last_trigger_time());
    DCHECK(proto_suppression.has_duration_ms());
    DCHECK(proto_suppression.has_recover_goal());

    SuppressionInfo suppression_info(
        MillisecondsToTime(proto_suppression.last_trigger_time()),
        MillisecondsToTimeDelta(proto_suppression.duration_ms()));
    suppression_info.recover_goal = proto_suppression.recover_goal();
    client_state->suppression_info = std::move(suppression_info);
  }

  client_state->negative_events_count = proto->negative_events_count();

  if (proto->has_last_shown_ts()) {
    client_state->last_shown_ts = MillisecondsToTime(proto->last_shown_ts());
  }

  if (proto->has_last_negative_event_ts()) {
    client_state->last_negative_event_ts =
        MillisecondsToTime(proto->last_negative_event_ts());
  }
}

void NotificationEntryToProto(NotificationEntry* entry,
                              proto::NotificationEntry* proto) {
  proto->set_type(ToSchedulerClientType(entry->type));
  proto->set_guid(entry->guid);
  proto->set_create_time(TimeToMilliseconds(entry->create_time));
  auto* proto_notification_data = proto->mutable_notification_data();
  for (const auto& icon_type_uuid_pair : entry->icons_uuid) {
    auto* proto_icons = proto_notification_data->add_icons_uuid();
    proto_icons->set_type(ToIconType(icon_type_uuid_pair.first));
    proto_icons->set_uuid(icon_type_uuid_pair.second);
  }
  NotificationDataToProto(&entry->notification_data, proto_notification_data);

  auto* proto_schedule_params = proto->mutable_schedule_params();
  ScheduleParamsToProto(&entry->schedule_params, proto_schedule_params);
}

void NotificationEntryFromProto(proto::NotificationEntry* proto,
                                NotificationEntry* entry) {
  entry->type = FromSchedulerClientType(proto->type());
  entry->guid = proto->guid();
  entry->create_time = MillisecondsToTime(proto->create_time());
  NotificationDataFromProto(proto->mutable_notification_data(),
                            &entry->notification_data);
  ScheduleParamsFromProto(proto->mutable_schedule_params(),
                          &entry->schedule_params);

  for (int i = 0; i < proto->notification_data().icons_uuid_size(); i++) {
    const auto& icon_uuid_pair = proto->notification_data().icons_uuid(i);
    entry->icons_uuid.emplace(FromIconType(icon_uuid_pair.type()),
                              icon_uuid_pair.uuid());
  }
}

}  // namespace notifications
