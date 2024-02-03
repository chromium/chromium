// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr uint64_t kDefaultProfileId = 0;
constexpr uint64_t kLacrosProfileId1 = 1001;
constexpr uint64_t kLacrosProfileId2 = 1003;

constexpr char kApp1[] = "app1";
constexpr char kApp2[] = "app2";

class ScrubLacrosProfileIdTest : public testing::Test {
 protected:
  // This acts as a (vastly) simplified representation of restore data for the
  // test. The first level maps application IDs to launch lists. A launch list
  // (the second level) maps a window id to app restore data. For the purposes
  // of the test, the app restore data is simply the lacros profile ID.
  using FakeRestoreData = std::map<std::string, std::map<int, uint64_t>>;

  struct ScrubProfileTestCase {
    std::string test_name;

    // The profile ID to scrub from the saved desk.
    uint64_t scrub_profile;

    // Profile set on the saved desk itself.
    uint64_t input_desk_profile;

    // Profile set on the output saved desk.
    uint64_t expected_desk_profile;

    // Input restore data.
    FakeRestoreData input_windows;

    // Expected output restore data.
    FakeRestoreData expected_windows;
  };

  void RunTests(const std::vector<ScrubProfileTestCase>& tests) {
    for (const auto& test : tests) {
      SCOPED_TRACE(test.test_name);

      auto saved_desk =
          MakeSavedDesk(test.input_desk_profile, test.input_windows);

      // If the input is different from the output, then we expect the scrub
      // function to return true.
      const bool expected_return =
          test.input_windows != test.expected_windows ||
          test.input_desk_profile != test.expected_desk_profile;

      const bool actual_return =
          ScrubLacrosProfileFromSavedDesk(*saved_desk, test.scrub_profile, 0);
      EXPECT_EQ(expected_return, actual_return);

      const uint64_t actual_desk_profile = saved_desk->lacros_profile_id();
      EXPECT_EQ(test.expected_desk_profile, actual_desk_profile);

      const FakeRestoreData actual_restore_data =
          ExtractRestoreData(*saved_desk);
      EXPECT_EQ(test.expected_windows, actual_restore_data);
    }
  }

  std::unique_ptr<DeskTemplate> MakeSavedDesk(
      uint64_t desk_lacros_profile_id,
      const FakeRestoreData& fake_restore_data) {
    auto saved_desk = std::make_unique<DeskTemplate>(
        base::Uuid::GenerateRandomV4(), DeskTemplateSource::kUser, "test",
        base::Time::Now(), DeskTemplateType::kSaveAndRecall);

    if (desk_lacros_profile_id) {
      saved_desk->set_lacros_profile_id(desk_lacros_profile_id);
    }

    if (!fake_restore_data.empty()) {
      auto restore_data = std::make_unique<app_restore::RestoreData>();

      for (const auto& [app_id, fake_launch_list] : fake_restore_data) {
        auto& launch_list =
            restore_data->mutable_app_id_to_launch_list()[app_id];
        for (const auto& [window_id, lacros_profile_id] : fake_launch_list) {
          auto& app_restore_data = launch_list[window_id];
          app_restore_data = std::make_unique<app_restore::AppRestoreData>();
          app_restore_data->browser_extra_info.lacros_profile_id =
              lacros_profile_id;
        }
      }

      saved_desk->set_desk_restore_data(std::move(restore_data));
    }

    return saved_desk;
  }

  FakeRestoreData ExtractRestoreData(const DeskTemplate& saved_desk) {
    FakeRestoreData fake_restore_data;

    if (auto* restore_data = saved_desk.desk_restore_data()) {
      for (const auto& [app_id, launch_list] :
           restore_data->app_id_to_launch_list()) {
        auto& fake_launch_list = fake_restore_data[app_id];
        for (const auto& [window_id, app_restore_data] : launch_list) {
          fake_launch_list[window_id] =
              app_restore_data->browser_extra_info.lacros_profile_id.value_or(
                  0);
        }
      }
    }

    return fake_restore_data;
  }
};

TEST_F(ScrubLacrosProfileIdTest, Suite) {
  RunTests({
      {.test_name = "empty",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kDefaultProfileId,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows = {},
       .expected_windows = {}},
      {.test_name = "empty_with_profile",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kLacrosProfileId1,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows = {},
       .expected_windows = {}},
      {.test_name = "empty_launch_list",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kDefaultProfileId,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows = {{}},
       .expected_windows = {}},
      {.test_name = "no matching windows",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kDefaultProfileId,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows = {{kApp1, {{100, kDefaultProfileId}}}},
       .expected_windows = {{kApp1, {{100, kDefaultProfileId}}}}},
      {.test_name = "all matching windows",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kLacrosProfileId1,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows =
           {{kApp1, {{100, kLacrosProfileId1}, {101, kLacrosProfileId1}}}},
       .expected_windows = {}},
      {.test_name = "mixed 1",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kDefaultProfileId,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows =
           {{kApp1, {{100, kDefaultProfileId}, {101, kLacrosProfileId1}}}},
       .expected_windows = {{kApp1, {{100, kDefaultProfileId}}}}},
      {.test_name = "mixed 2",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kDefaultProfileId,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows =
           {{kApp1, {{100, kLacrosProfileId1}, {101, kDefaultProfileId}}}},
       .expected_windows = {{kApp1, {{101, kDefaultProfileId}}}}},
      {.test_name = "mixed 3",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kLacrosProfileId2,
       .expected_desk_profile = kLacrosProfileId2,
       .input_windows = {{kApp1,
                          {{100, kLacrosProfileId1},
                           {101, kDefaultProfileId},
                           {102, kLacrosProfileId2},
                           {103, kLacrosProfileId1}}}},
       .expected_windows =
           {{kApp1, {{101, kDefaultProfileId}, {102, kLacrosProfileId2}}}}},
      {.test_name = "multiple apps 1",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kLacrosProfileId2,
       .expected_desk_profile = kLacrosProfileId2,
       .input_windows =
           {{kApp1, {{100, kLacrosProfileId1}, {101, kLacrosProfileId1}}},
            {kApp2, {{102, kLacrosProfileId1}, {103, kLacrosProfileId1}}}},
       .expected_windows = {}},
      {.test_name = "multiple apps 2",
       .scrub_profile = kLacrosProfileId1,
       .input_desk_profile = kLacrosProfileId1,
       .expected_desk_profile = kDefaultProfileId,
       .input_windows =
           {{kApp1, {{100, kLacrosProfileId1}, {101, kLacrosProfileId1}}},
            {kApp2, {{102, kDefaultProfileId}, {103, kLacrosProfileId2}}}},
       .expected_windows =
           {{kApp2, {{102, kDefaultProfileId}, {103, kLacrosProfileId2}}}}},
  });
}

}  // namespace
}  // namespace ash
