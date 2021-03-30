// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_actions_history.h"
#include <vector>

#include "base/containers/adapters.h"
#include "base/optional.h"
#include "base/util/values/values_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestEntry {
  permissions::PermissionAction action;
  permissions::RequestType type;
  bool advance_clock;
} kTestEntries[]{
    {permissions::PermissionAction::DISMISSED,
     permissions::RequestType::kNotifications, true},
    {permissions::PermissionAction::GRANTED,
     permissions::RequestType::kNotifications, false},
    {permissions::PermissionAction::DISMISSED,
     permissions::RequestType::kVrSession, true},
    {permissions::PermissionAction::IGNORED,
     permissions::RequestType::kCameraStream, false},
    {permissions::PermissionAction::DISMISSED,
     permissions::RequestType::kGeolocation, false},
    {permissions::PermissionAction::DENIED,
     permissions::RequestType::kNotifications, true},
    {permissions::PermissionAction::GRANTED,
     permissions::RequestType::kNotifications, false},
};
}  // namespace

class PermissionActionHistoryTest : public testing::Test {
 public:
  PermissionActionHistoryTest()
      : testing_profile_(std::make_unique<TestingProfile>()) {}
  ~PermissionActionHistoryTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    RecordSetUpActions();
  }

  PermissionActionHistoryTest(const PermissionActionHistoryTest&) =
      delete;
  PermissionActionHistoryTest& operator=(
      const PermissionActionHistoryTest&) = delete;

  PermissionActionsHistory* GetPermissionActionsHistory() {
    return PermissionActionsHistory::GetForProfile(profile());
  }

  std::vector<PermissionActionsHistory::Entry> GetHistory(
      base::Optional<permissions::RequestType> type) {
    if (type.has_value())
      return GetPermissionActionsHistory()->GetHistory(base::Time(),
                                                            type.value());
    else
      return GetPermissionActionsHistory()->GetHistory(base::Time());
  }

  void RecordSetUpActions() {
    // Record the actions needed to support test cases. This is the structure
    // 3-days ago: {notification, dismissed}
    // 2-days ago: {notification, granted}, {vr, dismissed}
    // 1-days ago: {geolocation, ignored}, {camera, dismissed}, {notification,
    // denied}
    // 0-days ago: {notification, granted}
    for (const auto& entry : kTestEntries) {
      GetPermissionActionsHistory()->RecordAction(entry.action,
                                                       entry.type);
      if (entry.advance_clock)
        task_environment_.AdvanceClock(base::TimeDelta::FromDays(1));
    }
  }

  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> testing_profile_;
};

TEST_F(PermissionActionHistoryTest, GetHistorySortedOrder) {
  auto all_entries = GetHistory(base::nullopt);

  EXPECT_EQ(7u, all_entries.size());

  size_t index = 0;
  for (const auto& entry : kTestEntries)
    EXPECT_EQ(entry.action, all_entries[index++].action);

  for (const auto& request_type : {permissions::RequestType::kVrSession,
                                   permissions::RequestType::kCameraStream,
                                   permissions::RequestType::kGeolocation,
                                   permissions::RequestType::kNotifications}) {
    auto permission_entries = GetHistory(request_type);

    index = 0;
    for (const auto& entry : kTestEntries) {
      if (entry.type != request_type) {
        continue;
      }

      EXPECT_EQ(entry.action, permission_entries[index++].action);
    }
  }

  auto entries_1_day = GetPermissionActionsHistory()->GetHistory(
      base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_TRUE(std::equal(entries_1_day.begin(), entries_1_day.end(),
                         std::vector<PermissionActionsHistory::Entry>(
                             all_entries.begin() + 3, all_entries.end())
                             .begin()));
}

TEST_F(PermissionActionHistoryTest, NotificationRecordAction) {
  size_t general_count = GetHistory(base::nullopt).size();
  size_t notification_count =
      GetHistory(permissions::RequestType::kNotifications).size();

  GetPermissionActionsHistory()->RecordAction(
      permissions::PermissionAction::GRANTED,
      permissions::RequestType::kNotifications);

  EXPECT_EQ(general_count + 1, GetHistory(base::nullopt).size());
  EXPECT_EQ(notification_count + 1,
            GetHistory(permissions::RequestType::kNotifications).size());

  GetPermissionActionsHistory()->RecordAction(
      permissions::PermissionAction::GRANTED,
      permissions::RequestType::kGeolocation);

  EXPECT_EQ(general_count + 2, GetHistory(base::nullopt).size());
  EXPECT_EQ(notification_count + 1,
            GetHistory(permissions::RequestType::kNotifications).size());
}

TEST_F(PermissionActionHistoryTest, ClearHistory) {
  struct {
    base::Time begin;
    base::Time end;
    size_t generic_count;
    size_t notifications_count;
  } kTests[] = {
      // Misc and baseline tests cases.
      {base::Time(), base::Time::Max(), 0, 0},
      {base::Time(), base::Time::Now(), 1, 1},
      {base::Time(), base::Time::Now() + base::TimeDelta::FromMicroseconds(1),
       0, 0},

      // Test cases specifying only the upper bound.
      {base::Time(), base::Time::Now() - base::TimeDelta::FromDays(1), 4, 2},
      {base::Time(), base::Time::Now() - base::TimeDelta::FromDays(2), 6, 3},
      {base::Time(), base::Time::Now() - base::TimeDelta::FromDays(3), 7, 4},

      // Test cases specifying only the lower bound.
      {base::Time::Now() - base::TimeDelta::FromDays(3), base::Time::Max(), 0,
       0},
      {base::Time::Now() - base::TimeDelta::FromDays(2), base::Time::Max(), 1,
       1},
      {base::Time::Now() - base::TimeDelta::FromDays(1), base::Time::Max(), 3,
       2},
      {base::Time::Now(), base::Time::Max(), 6, 3},

      // Test cases with both bounds.
      {base::Time::Now() - base::TimeDelta::FromDays(3),
       base::Time::Now() + base::TimeDelta::FromMicroseconds(1), 0, 0},
      {base::Time::Now() - base::TimeDelta::FromDays(3), base::Time::Now(), 1,
       1},
      {base::Time::Now() - base::TimeDelta::FromDays(3),
       base::Time::Now() - base::TimeDelta::FromDays(1), 4, 2},
      {base::Time::Now() - base::TimeDelta::FromDays(3),
       base::Time::Now() - base::TimeDelta::FromDays(2), 6, 3},
      {base::Time::Now() - base::TimeDelta::FromDays(3),
       base::Time::Now() - base::TimeDelta::FromDays(3), 7, 4},

      {base::Time::Now() - base::TimeDelta::FromDays(2),
       base::Time::Now() + base::TimeDelta::FromMicroseconds(1), 1, 1},
      {base::Time::Now() - base::TimeDelta::FromDays(2), base::Time::Now(), 2,
       2},
      {base::Time::Now() - base::TimeDelta::FromDays(2),
       base::Time::Now() - base::TimeDelta::FromDays(1), 5, 3},
      {base::Time::Now() - base::TimeDelta::FromDays(2),
       base::Time::Now() - base::TimeDelta::FromDays(2), 7, 4},

      {base::Time::Now() - base::TimeDelta::FromDays(1),
       base::Time::Now() + base::TimeDelta::FromMicroseconds(1), 3, 2},
      {base::Time::Now() - base::TimeDelta::FromDays(1), base::Time::Now(), 4,
       3},
      {base::Time::Now() - base::TimeDelta::FromDays(1),
       base::Time::Now() - base::TimeDelta::FromDays(1), 7, 4},

      {base::Time::Now(),
       base::Time::Now() + base::TimeDelta::FromMicroseconds(1), 6, 3},
      {base::Time::Now(), base::Time::Now(), 7, 4},
  };

  // We need to account for much we have already advanced the time for each test
  // case and so we keep track of how much we need to offset the initial test
  // values.
  base::TimeDelta current_offset;

  for (auto& test : kTests) {
    test.begin += current_offset;
    test.end += current_offset;

    GetPermissionActionsHistory()->ClearHistory(test.begin, test.end);
    EXPECT_EQ(test.generic_count, GetHistory(base::nullopt).size());
    EXPECT_EQ(test.notifications_count,
              GetHistory(permissions::RequestType::kNotifications).size());

    // Clean up for next test and update offset.
    base::Time last_now = base::Time::Now();
    GetPermissionActionsHistory()->ClearHistory(base::Time(),
                                                     base::Time::Max());
    RecordSetUpActions();
    current_offset += base::Time::Now() - last_now;
  }
}
