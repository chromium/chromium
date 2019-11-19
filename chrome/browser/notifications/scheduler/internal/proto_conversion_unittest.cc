// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using IconProto = notifications::proto::Icon;

namespace notifications {
namespace {

const char kGuid[] = "testGuid";
const char kData[] = "bitmapdata";

void TestClientStateConversion(ClientState* client_state) {
  DCHECK(client_state);
  notifications::proto::ClientState proto;
  ClientState expected;
  ClientStateToProto(client_state, &proto);
  ClientStateFromProto(&proto, &expected);
  EXPECT_EQ(*client_state, expected)
      << " \n Output: \n " << test::DebugString(client_state)
      << " \n Expected: \n"
      << test::DebugString(&expected);
}

void TestNotificationEntryConversion(NotificationEntry* entry) {
  DCHECK(entry);
  notifications::proto::NotificationEntry proto;
  NotificationEntry expected(SchedulerClientType::kTest1, "");
  NotificationEntryToProto(entry, &proto);
  NotificationEntryFromProto(&proto, &expected);
  EXPECT_EQ(entry->notification_data, expected.notification_data);
  EXPECT_EQ(entry->schedule_params, expected.schedule_params);

  EXPECT_EQ(*entry, expected)
      << "Output: " << test::DebugString(entry)
      << " \n Expected: " << test::DebugString(&expected);
}

NotificationData::Button CreateButton(const char* text,
                                      ActionButtonType type,
                                      const char* id) {
  NotificationData::Button button;
  button.text = base::UTF8ToUTF16(text);
  button.type = type;
  button.id = id;
  return button;
}

TEST(ProtoConversionTest, IconEntryFromProto) {
  IconProto proto;
  proto.set_icon(kData);
  IconEntry entry;

  IconEntryFromProto(&proto, &entry);

  // Verify entry data.
  EXPECT_EQ(entry.data, kData);
}

TEST(ProtoConversionTest, IconEntryToProto) {
  IconEntry entry;
  entry.data = kData;
  IconProto proto;

  IconEntryToProto(&entry, &proto);

  // Verify proto data.
  EXPECT_EQ(proto.icon(), kData);
}

// Verifies client state proto conversion.
TEST(ProtoConversionTest, ClientStateProtoConversion) {
  // Verify basic fields.
  ClientState client_state;
  test::ImpressionTestData test_data{
      SchedulerClientType::kTest1, 3, {}, base::nullopt};
  test::AddImpressionTestData(test_data, &client_state);
  TestClientStateConversion(&client_state);

  // Verify suppression info.
  base::Time last_trigger_time;
  bool success =
      base::Time::FromString("04/25/20 01:00:00 AM", &last_trigger_time);
  DCHECK(success);
  auto duration = base::TimeDelta::FromDays(7);
  auto suppression = SuppressionInfo(last_trigger_time, duration);
  suppression.recover_goal = 5;
  client_state.suppression_info = std::move(suppression);
  TestClientStateConversion(&client_state);
}

// Verifies impression proto conversion.
TEST(ProtoConversionTest, ImpressionProtoConversion) {
  ClientState client_state;
  client_state.type = SchedulerClientType::kTest1;
  base::Time create_time;
  bool success = base::Time::FromString("03/25/19 00:00:00 AM", &create_time);
  DCHECK(success);

  Impression impression = test::CreateImpression(
      create_time, UserFeedback::kHelpful, ImpressionResult::kPositive,
      true /*integrated*/, kGuid, SchedulerClientType::kTest1);
  client_state.impressions.emplace_back(impression);
  TestClientStateConversion(&client_state);

  auto& first_impression = *client_state.impressions.begin();

  // Verify all feedback types.
  std::vector<UserFeedback> feedback_types{
      UserFeedback::kNoFeedback, UserFeedback::kHelpful,
      UserFeedback::kNotHelpful, UserFeedback::kClick,
      UserFeedback::kDismiss,    UserFeedback::kIgnore};
  for (const auto feedback_type : feedback_types) {
    first_impression.feedback = feedback_type;
    TestClientStateConversion(&client_state);
  }

  // Verify all impression result types.
  std::vector<ImpressionResult> impression_results{
      ImpressionResult::kInvalid, ImpressionResult::kPositive,
      ImpressionResult::kNegative, ImpressionResult::kNeutral};
  for (const auto impression_result : impression_results) {
    first_impression.impression = impression_result;
    TestClientStateConversion(&client_state);
  }

  // Verify impression mapping.
  first_impression.impression_mapping[UserFeedback::kClick] =
      ImpressionResult::kNeutral;
  TestClientStateConversion(&client_state);

  // Verify custom data.
  first_impression.custom_data = {{"url", "https://www.example.com"}};
  TestClientStateConversion(&client_state);

  // Verify custom suppression duration.
  first_impression.custom_suppression_duration = base::TimeDelta::FromDays(3);
  TestClientStateConversion(&client_state);
}

// Verifies multiple impressions are serialized correctly.
TEST(ProtoConversionTest, MultipleImpressionConversion) {
  ClientState client_state;
  base::Time create_time;
  bool success = base::Time::FromString("04/25/20 01:00:00 AM", &create_time);
  DCHECK(success);

  Impression impression = test::CreateImpression(
      create_time, UserFeedback::kHelpful, ImpressionResult::kPositive,
      true /*integrated*/, "guid1", SchedulerClientType::kUnknown);
  Impression other_impression = test::CreateImpression(
      create_time, UserFeedback::kNoFeedback, ImpressionResult::kNegative,
      false /*integrated*/, "guid2", SchedulerClientType::kUnknown);
  client_state.impressions.emplace_back(std::move(impression));
  client_state.impressions.emplace_back(std::move(other_impression));
  TestClientStateConversion(&client_state);
}

// Verifies notification entry proto conversion.
TEST(ProtoConversionTest, NotificationEntryConversion) {
  NotificationEntry entry(SchedulerClientType::kTest2, kGuid);
  bool success =
      base::Time::FromString("04/25/20 01:00:00 AM", &entry.create_time);
  DCHECK(success);
  TestNotificationEntryConversion(&entry);

  // Test notification data.
  entry.notification_data.title = base::UTF8ToUTF16("title");
  entry.notification_data.message = base::UTF8ToUTF16("message");
  entry.icons_uuid.emplace(IconType::kSmallIcon, "small_icon_uuid");
  entry.icons_uuid.emplace(IconType::kLargeIcon, "large_icon_uuid");
  entry.notification_data.custom_data = {{"url", "https://www.example.com"}};
  TestNotificationEntryConversion(&entry);

  // Test scheduling params.
  const ScheduleParams::Priority priorities[] = {
      ScheduleParams::Priority::kLow, ScheduleParams::Priority::kNoThrottle};
  for (auto priority : priorities) {
    entry.schedule_params.priority = priority;
    TestNotificationEntryConversion(&entry);
  }
  entry.schedule_params.impression_mapping[UserFeedback::kDismiss] =
      ImpressionResult::kPositive;
  entry.schedule_params.impression_mapping[UserFeedback::kClick] =
      ImpressionResult::kNeutral;
  TestNotificationEntryConversion(&entry);

  entry.schedule_params.deliver_time_start = entry.create_time;
  entry.schedule_params.deliver_time_end =
      entry.create_time + base::TimeDelta::FromMinutes(10);
  entry.schedule_params.custom_suppression_duration =
      base::TimeDelta::FromDays(3);
  TestNotificationEntryConversion(&entry);
}

// Verifies buttons are converted correctly to proto buffers.
TEST(ProtoConversionTest, NotificationEntryButtonsConversion) {
  NotificationEntry entry(SchedulerClientType::kTest2, kGuid);
  bool success =
      base::Time::FromString("04/25/20 01:00:00 AM", &entry.create_time);
  DCHECK(success);

  NotificationData::Button button;
  entry.notification_data.buttons.emplace_back(
      CreateButton("text1", ActionButtonType::kUnknownAction, "id1"));
  entry.notification_data.buttons.emplace_back(
      CreateButton("text2", ActionButtonType::kHelpful, "id2"));
  entry.notification_data.buttons.emplace_back(
      CreateButton("text3", ActionButtonType::kUnhelpful, "id3"));
  TestNotificationEntryConversion(&entry);
}

}  // namespace
}  // namespace notifications
