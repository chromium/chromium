// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/persisted_app_info.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace app_time {

using PersistedAppInfoTest = testing::Test;

TEST_F(PersistedAppInfoTest, RemoveActiveTimes) {
  AppId app = AppId(apps::AppType::kArc, "1");
  AppState app_state = AppState::kAvailable;
  base::TimeDelta running_active_time = base::Hours(5);

  base::Time start_time = base::Time::Now();
  base::TimeDelta activity = base::Hours(1);

  AppActivity::ActiveTime to_remove =
      AppActivity::ActiveTime(start_time, start_time + activity);
  AppActivity::ActiveTime to_trim = AppActivity::ActiveTime(
      start_time + 2 * activity, start_time + 3 * activity);
  AppActivity::ActiveTime to_keep = AppActivity::ActiveTime(
      start_time + 4 * activity, start_time + 5 * activity);

  PersistedAppInfo app_info(app, app_state, running_active_time,
                            {{to_remove, to_trim, to_keep}});

  EXPECT_TRUE(base::Contains(app_info.active_times(), to_remove));
  EXPECT_TRUE(base::Contains(app_info.active_times(), to_trim));
  EXPECT_TRUE(base::Contains(app_info.active_times(), to_keep));

  base::Time report_time = start_time + 2.5 * activity;
  app_info.RemoveActiveTimeEarlierThan(report_time);

  EXPECT_EQ(2u, app_info.active_times().size());
  EXPECT_FALSE(base::Contains(app_info.active_times(), to_remove));
  EXPECT_TRUE(base::Contains(app_info.active_times(), to_keep));

  const AppActivity::ActiveTime trimmed(report_time, to_trim.active_to());
  EXPECT_TRUE(base::Contains(app_info.active_times(), trimmed));
}

TEST_F(PersistedAppInfoTest, UpdateAppActivityPreference) {
  AppId app = AppId(apps::AppType::kArc, "1");
  AppState app_state = AppState::kAvailable;
  base::TimeDelta running_active_time = base::Hours(5);

  base::Time start_time = base::Time::Now();
  base::TimeDelta activity = base::Hours(1);

  AppActivity::ActiveTime entry1 =
      AppActivity::ActiveTime(start_time, start_time + activity);
  AppActivity::ActiveTime entry2 = AppActivity::ActiveTime(
      start_time + 2 * activity, start_time + 3 * activity);
  AppActivity::ActiveTime entry3 = AppActivity::ActiveTime(
      start_time + 4 * activity, start_time + 5 * activity);

  PersistedAppInfo app_info(app, app_state, running_active_time,
                            {{entry1, entry2, entry3}});
  base::Value::Dict entry;

  app_info.UpdateAppActivityPreference(entry, /* replace */ false);
  AppActivity::ActiveTime to_append = AppActivity::ActiveTime(
      start_time + 6 * activity, start_time + 7 * activity);
  PersistedAppInfo app_info2(app, app_state, running_active_time,
                             {{to_append}});
  app_info2.UpdateAppActivityPreference(entry, /* replace */ false);

  std::optional<PersistedAppInfo> updated_entry =
      PersistedAppInfo::PersistedAppInfoFromDict(
          &entry, /* include_app_activity_array */ true);
  ASSERT_TRUE(updated_entry.has_value());

  const std::vector<AppActivity::ActiveTime>& active_times =
      updated_entry->active_times();

  EXPECT_EQ(active_times.size(), 4u);
  EXPECT_EQ(active_times[0], entry1);
  EXPECT_EQ(active_times[1], entry2);
  EXPECT_EQ(active_times[2], entry3);
  EXPECT_EQ(active_times[3], to_append);

  app_info2.UpdateAppActivityPreference(entry, /* replace */ true);
  std::optional<PersistedAppInfo> final_entry =
      PersistedAppInfo::PersistedAppInfoFromDict(
          &entry, /* include_app_activity_array */ true);
  EXPECT_TRUE(final_entry.has_value());
  EXPECT_EQ(final_entry->active_times().size(), 1u);
  EXPECT_EQ(final_entry->active_times()[0], to_append);
}

}  // namespace app_time
}  // namespace ash
