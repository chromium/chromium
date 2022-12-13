// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon_unittest.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr size_t kMenuMaxItemCount =
    (IDC_MANAGE_HID_DEVICES_LAST - IDC_MANAGE_HID_DEVICES_FIRST + 1);

}  // namespace

class MockStatusIcon : public StatusIcon {
 public:
  explicit MockStatusIcon(const std::u16string& tool_tip)
      : tool_tip_(tool_tip) {}
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {
    tool_tip_ = tool_tip;
  }
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {
    menu_item_ = menu;
  }
  const std::u16string& tool_tip() const { return tool_tip_; }
  StatusIconMenuModel* menu_item() const { return menu_item_; }

 private:
  raw_ptr<StatusIconMenuModel> menu_item_ = nullptr;
  std::u16string tool_tip_;
};

class MockStatusTray : public StatusTray {
 public:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>(tool_tip);
  }

  const StatusIcons& GetStatusIconsForTest() const { return status_icons(); }
};

class HidStatusIconTest : public HidSystemTrayIconTestBase {
 public:
  void SetUp() override {
    HidSystemTrayIconTestBase::SetUp();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(
        std::make_unique<MockStatusTray>());
  }

  void TearDown() override {
    HidSystemTrayIconTestBase::TearDown();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(nullptr);
  }

  void CheckIcon(const std::vector<std::pair<Profile*, size_t>>&
                     profile_connection_counts) override {
    const auto* status_tray = static_cast<MockStatusTray*>(
        TestingBrowserProcess::GetGlobal()->status_tray());
    ASSERT_TRUE(status_tray);
    EXPECT_EQ(status_tray->GetStatusIconsForTest().size(), 1u);
    const auto* status_icon = static_cast<MockStatusIcon*>(
        status_tray->GetStatusIconsForTest().back().get());

    size_t total_connection_count = 0;
    auto* menu_item = status_icon->menu_item();
    EXPECT_EQ(menu_item->GetItemCount(),
              std::min(profile_connection_counts.size(), kMenuMaxItemCount));
    // Check each button label and behavior of clicking the button.
    for (size_t idx = 0; idx < profile_connection_counts.size(); idx++) {
      auto* hid_connection_tracker = static_cast<MockHidConnectionTracker*>(
          HidConnectionTrackerFactory::GetForProfile(
              profile_connection_counts[idx].first,
              /*create=*/false));
      EXPECT_TRUE(hid_connection_tracker);
      if (idx < kMenuMaxItemCount) {
        EXPECT_EQ(menu_item->GetCommandIdAt(idx),
                  IDC_MANAGE_HID_DEVICES_FIRST + static_cast<int>(idx));
        EXPECT_EQ(menu_item->GetLabelAt(idx),
                  GetExpectedButtonTitleForProfile(
                      profile_connection_counts[idx].first));
        EXPECT_CALL(*hid_connection_tracker,
                    ShowHidContentSettingsExceptions());
        SimulateButtonClick(idx);
      }
      total_connection_count += profile_connection_counts[idx].second;
    }

    // Check the icon tool tip is with the right singular/plural term according
    // to total connection count across profiles.
    EXPECT_EQ(status_icon->tool_tip(),
              GetExpectedIconTooltip(total_connection_count));
  }

  void CheckIconHidden() override {
    const auto* status_tray = static_cast<MockStatusTray*>(
        TestingBrowserProcess::GetGlobal()->status_tray());
    ASSERT_TRUE(status_tray);
    EXPECT_TRUE(status_tray->GetStatusIconsForTest().empty());
  }

 private:
  void SimulateButtonClick(size_t button_idx) {
    const auto* status_tray = static_cast<MockStatusTray*>(
        TestingBrowserProcess::GetGlobal()->status_tray());
    ASSERT_TRUE(status_tray);
    EXPECT_EQ(status_tray->GetStatusIconsForTest().size(), 1u);
    const auto* status_icon = static_cast<MockStatusIcon*>(
        status_tray->GetStatusIconsForTest().back().get());
    auto* menu_item = status_icon->menu_item();
    EXPECT_LT(button_idx, menu_item->GetItemCount());
    menu_item->ActivatedAt(button_idx);
  }
};

TEST_F(HidStatusIconTest, SingleProfileEmptyName) {
  // Current TestingProfileManager can't support empty profile name as it uses
  // profile name for profile path. Passing empty would result in a failure in
  // ProfileManager::IsAllowedProfilePath(). Changing the way
  // TestingProfileManager creating profile path like adding "profile" prefix
  // doesn't work either as some tests are written in a way that takes
  // assumption of testing profile path pattern. Hence it creates testing
  // profile with non-empty name and then change the profile name to empty which
  // can still achieve what this file wants to test.
  profile()->set_profile_name("");
  TestSingleProfile();
}

TEST_F(HidStatusIconTest, SingleProfileNonEmptyName) {
  TestSingleProfile();
}

TEST_F(HidStatusIconTest, MultipleProfiles) {
  TestMultipleProfiles();
}

TEST_F(HidStatusIconTest, NumProfilesOverLimit) {
  // Set to 10 more profiles than the max limit.
  size_t num_profiles = kMenuMaxItemCount + 10;
  std::vector<std::pair<Profile*, size_t>> profile_connection_counts;
  std::vector<HidConnectionTracker*> hid_connection_trackers;
  for (size_t idx = 0; idx < num_profiles; idx++) {
    std::string profile_name = base::StringPrintf("user%zu", idx);
    auto* profile = CreateTestingProfile(profile_name);
    hid_connection_trackers.emplace_back(
        HidConnectionTrackerFactory::GetForProfile(profile,
                                                   /*create=*/true));
    hid_connection_trackers[idx]->IncrementConnectionCount();
    profile_connection_counts.push_back({profile, 1});
  }
  // CheckIcon has the logic to expect the icon button size is
  // |kMenuMaxItemCount|.
  CheckIcon(profile_connection_counts);
}
