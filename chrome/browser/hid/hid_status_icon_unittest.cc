// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

std::u16string GetExpectedOriginConnectionCountLabel(Profile* profile,
                                                     const url::Origin& origin,
                                                     const std::string& name,
                                                     int connection_count) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    if (connection_count == 0) {
      return base::UTF8ToUTF16(base::StringPrintf(
          "Extension \"%s\" was accessing devices", name.c_str()));
    }
    return base::UTF8ToUTF16(base::StringPrintf(
        "Extension \"%s\" is accessing %d %s", name.c_str(), connection_count,
        (connection_count <= 1 ? "device" : "devices")));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

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

  void CheckSeparatorMenuItem(StatusIconMenuModel* menu_item, int menu_idx) {
    EXPECT_EQ(menu_item->GetSeparatorTypeAt(menu_idx), ui::NORMAL_SEPARATOR);
  }

  void CheckMenuItemLabel(StatusIconMenuModel* menu_item,
                          int menu_idx,
                          std::u16string label) {
    EXPECT_EQ(menu_item->GetLabelAt(menu_idx), label);
  }

  void CheckClickableMenuItem(StatusIconMenuModel* menu_item,
                              int menu_idx,
                              std::u16string label,
                              int command_id,
                              bool click) {
    CheckMenuItemLabel(menu_item, menu_idx, label);
    EXPECT_EQ(menu_item->GetCommandIdAt(menu_idx), command_id);
    if (click) {
      menu_item->ActivatedAt(menu_idx);
    }
  }

  void CheckIcon(const std::vector<HidSystemTrayIconTestBase::ProfileItem>&
                     profile_connection_counts) override {
    const auto* status_tray = static_cast<MockStatusTray*>(
        TestingBrowserProcess::GetGlobal()->status_tray());
    ASSERT_TRUE(status_tray);
    EXPECT_EQ(status_tray->GetStatusIconsForTest().size(), 1u);
    const auto* status_icon = static_cast<MockStatusIcon*>(
        status_tray->GetStatusIconsForTest().back().get());

    // Sort the |profile_connection_counts| by the address of the profile
    // pointer. This is necessary because the menu items are created by
    // iterating through a structure of flat_map<Profile*, bool>.
    auto sorted_profile_connection_counts = profile_connection_counts;
    base::ranges::sort(sorted_profile_connection_counts);
    size_t total_connection_count = 0;
    size_t total_origin_count = 0;
    auto* menu_item = status_icon->menu_item();
    // The system tray icon title (i.e menu_idx == 0) will be checked at the end
    // when |total_connection_count| is calculated.
    int menu_idx = 1;
    int expected_command_id = IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST;
    CheckClickableMenuItem(menu_item, menu_idx++, u"About HID devices",
                           expected_command_id++, /*click=*/false);
    for (const auto& [profile, origin_items] :
         sorted_profile_connection_counts) {
      total_origin_count += origin_items.size();
      auto sorted_origin_items = origin_items;
      // Sort the |origin_items| by origin. This is necessary because the origin
      // items for each profile in the menu are created by iterating through a
      // structure of flat_map<url::Origin, ...>.
      base::ranges::sort(sorted_origin_items);
      auto* hid_connection_tracker = static_cast<MockHidConnectionTracker*>(
          HidConnectionTrackerFactory::GetForProfile(profile,
                                                     /*create=*/false));
      ASSERT_TRUE(hid_connection_tracker);
      CheckSeparatorMenuItem(menu_item, menu_idx++);
      CheckMenuItemLabel(menu_item, menu_idx++,
                         base::UTF8ToUTF16(profile->GetProfileUserName()));
      EXPECT_CALL(*hid_connection_tracker, ShowContentSettingsExceptions());
      CheckClickableMenuItem(menu_item, menu_idx++, u"HID settings",
                             expected_command_id++, /*click=*/true);
      for (const auto& [origin, connection_count, name] : sorted_origin_items) {
        EXPECT_CALL(*hid_connection_tracker, ShowSiteSettings(origin));
        CheckClickableMenuItem(menu_item, menu_idx++,
                               GetExpectedOriginConnectionCountLabel(
                                   profile, origin, name, connection_count),
                               expected_command_id++, /*click=*/true);
        total_connection_count += connection_count;
      }
    }
    CheckMenuItemLabel(
        menu_item, 0,
        GetExpectedTitle(total_origin_count,
                         override_title_total_connection_count_.value_or(
                             total_connection_count)));
    EXPECT_LE(expected_command_id, IDC_DEVICE_SYSTEM_TRAY_ICON_LAST + 1);
  }

  void CheckIconHidden() override {
    const auto* status_tray = static_cast<MockStatusTray*>(
        TestingBrowserProcess::GetGlobal()->status_tray());
    ASSERT_TRUE(status_tray);
    EXPECT_TRUE(status_tray->GetStatusIconsForTest().empty());
  }

 protected:
  // This is specifically used in the test case of
  // NumCommandIdOverLimitExtensionOrigin, where the input parameter
  // profile_connection_counts may not capture all of the origins because the
  // limit is exceeded.
  absl::optional<int> override_title_total_connection_count_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(HidStatusIconTest, SingleProfileEmptyNameExtentionOrigins) {
  // Current TestingProfileManager can't support empty profile name as it uses
  // profile name for profile path. Passing empty would result in a failure in
  // ProfileManager::IsAllowedProfilePath(). Changing the way
  // TestingProfileManager creating profile path like adding "profile" prefix
  // doesn't work either as some tests are written in a way that takes
  // assumption of testing profile path pattern. Hence it creates testing
  // profile with non-empty name and then change the profile name to empty which
  // can still achieve what this file wants to test.
  profile()->set_profile_name("");
  TestSingleProfileExtentionOrigins();
}

TEST_F(HidStatusIconTest, SingleProfileNonEmptyNameExtentionOrigins) {
  TestSingleProfileExtentionOrigins();
}

TEST_F(HidStatusIconTest, BounceConnectionExtensionOrigins) {
  TestBounceConnectionExtensionOrigins();
}

TEST_F(HidStatusIconTest, MultipleProfilesExtensionOrigins) {
  TestMultipleProfilesExtensionOrigins();
}

TEST_F(HidStatusIconTest, NumCommandIdOverLimitExtensionOrigin) {
  // There are only 40 command ids available. The test creates a scenario that
  // will use more than 40 command ids.

  // Each profile with one origin requires two command IDs (one for "About HID
  // Device" and one for "extension is connecting to 1 device"). The below for
  // loop sets up 19 profiles, which will consume 39 menu items (1 + 19 * 2).
  size_t num_profiles = 19;
  std::vector<HidSystemTrayIconTestBase::ProfileItem> profile_connection_counts;
  for (size_t idx = 0; idx < num_profiles; idx++) {
    std::string profile_name = base::StringPrintf("user%zu", idx);
    auto* profile = CreateTestingProfile(profile_name);
    auto extension = CreateExtensionWithName("Test Extension");
    AddExtensionToProfile(profile, extension.get());
    auto* connection_tracker =
        HidConnectionTrackerFactory::GetForProfile(profile,
                                                   /*create=*/true);
    connection_tracker->IncrementConnectionCount(extension->origin());
    profile_connection_counts.push_back(
        {profile, {{extension->origin(), 1, extension->name()}}});
  }
  CheckIcon(profile_connection_counts);

  // Adding one more profile and it will hit the limit.
  {
    std::string profile_name = base::StringPrintf("user%zu", num_profiles);
    auto* profile = CreateTestingProfile(profile_name);
    auto extension = CreateExtensionWithName("Test Extension");
    AddExtensionToProfile(profile, extension.get());
    auto* connection_tracker =
        HidConnectionTrackerFactory::GetForProfile(profile,
                                                   /*create=*/true);
    connection_tracker->IncrementConnectionCount(extension->origin());
    // The origin connection menu item will not be added because the limit of
    // connections has been reached. However, icon items are inserted by
    // iterating over a flat_map<Profile*, bool> structure, so it needs to
    // identify the last profile by sorting profiles and remove its origin
    // count.
    profile_connection_counts.push_back(
        {profile, {{extension->origin(), 1, extension->name()}}});
    base::ranges::sort(profile_connection_counts);
    profile_connection_counts.back().second.clear();
    // The total connection count in the title still captures all of the origins
    override_title_total_connection_count_ = 20;
    CheckIcon(profile_connection_counts);
  }
}

TEST_F(HidStatusIconTest, ExtensionRemoval) {
  TestExtensionRemoval();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
