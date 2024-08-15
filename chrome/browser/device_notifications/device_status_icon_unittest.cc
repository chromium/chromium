// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_status_icon_unittest.h"

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  NOTREACHED();
}

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

}  // namespace

DeviceStatusIconTestBase::DeviceStatusIconTestBase(
    std::u16string about_device_label,
    std::u16string device_content_settings_label)
    : about_device_label_(std::move(about_device_label)),
      device_content_settings_label_(std::move(device_content_settings_label)) {
}

void DeviceStatusIconTestBase::SetUp() {
  DeviceSystemTrayIconTestBase::SetUp();
  TestingBrowserProcess::GetGlobal()->SetStatusTray(
      std::make_unique<MockStatusTray>());
}

void DeviceStatusIconTestBase::TearDown() {
  DeviceSystemTrayIconTestBase::TearDown();
  TestingBrowserProcess::GetGlobal()->SetStatusTray(nullptr);
}

void DeviceStatusIconTestBase::CheckIcon(
    const std::vector<DeviceSystemTrayIconTestBase::ProfileItem>&
        profile_connection_counts) {
  const auto* status_tray = static_cast<MockStatusTray*>(
      TestingBrowserProcess::GetGlobal()->status_tray());
  ASSERT_TRUE(status_tray);
  ASSERT_EQ(status_tray->GetStatusIconsForTest().size(), 1u);
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
#if !BUILDFLAG(IS_MAC)
  // For non-MacOS, the system tray icon title (i.e menu_idx == 0) will be
  // checked at the end when |total_connection_count| is calculated.
  int menu_idx = 1;
#else
  // For MacOs, the system tray icon title is the tooltip.
  int menu_idx = 0;
#endif  //! BUILDFLAG(IS_MAC)
  int expected_command_id = IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST;
  CheckClickableMenuItem(menu_item, menu_idx++, about_device_label_,
                         expected_command_id++, /*click=*/false);
  for (const auto& [profile, origin_items] : sorted_profile_connection_counts) {
    total_origin_count += origin_items.size();
    auto sorted_origin_items = origin_items;
    // Sort the |origin_items| by origin. This is necessary because the origin
    // items for each profile in the menu are created by iterating through a
    // structure of flat_map<url::Origin, ...>.
    base::ranges::sort(sorted_origin_items);
    auto* connection_tracker = GetDeviceConnectionTracker(profile,
                                                          /*create=*/false);
    ASSERT_TRUE(connection_tracker);
    CheckSeparatorMenuItem(menu_item, menu_idx++);
    CheckMenuItemLabel(menu_item, menu_idx++,
                       base::UTF8ToUTF16(profile->GetProfileUserName()));
    EXPECT_CALL(*GetMockDeviceConnectionTracker(connection_tracker),
                ShowContentSettingsExceptions());
    CheckClickableMenuItem(menu_item, menu_idx++,
                           device_content_settings_label_,
                           expected_command_id++, /*click=*/true);
    for (const auto& [origin, connection_count, name] : sorted_origin_items) {
      EXPECT_CALL(*GetMockDeviceConnectionTracker(connection_tracker),
                  ShowSiteSettings(origin));
      CheckClickableMenuItem(menu_item, menu_idx++,
                             GetExpectedOriginConnectionCountLabel(
                                 profile, origin, name, connection_count),
                             expected_command_id++, /*click=*/true);
      total_connection_count += connection_count;
    }
  }
#if !BUILDFLAG(IS_MAC)
  CheckMenuItemLabel(
      menu_item, 0,
      GetExpectedTitle(total_origin_count,
                       override_title_total_connection_count_.value_or(
                           total_connection_count)));
#endif  //! BUILDFLAG(IS_MAC)
  EXPECT_EQ(status_icon->tool_tip(),
            GetExpectedTitle(total_origin_count,
                             override_title_total_connection_count_.value_or(
                                 total_connection_count)));
  EXPECT_LE(expected_command_id, IDC_DEVICE_SYSTEM_TRAY_ICON_LAST + 1);
}

void DeviceStatusIconTestBase::CheckIconHidden() {
  const auto* status_tray = static_cast<MockStatusTray*>(
      TestingBrowserProcess::GetGlobal()->status_tray());
  ASSERT_TRUE(status_tray);
  EXPECT_TRUE(status_tray->GetStatusIconsForTest().empty());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void DeviceStatusIconTestBase::TestNumCommandIdOverLimitExtensionOrigin() {
  // There are only 40 command ids available. The test creates a scenario that
  // will use more than 40 command ids.

  // Each profile with one origin requires two command IDs (one for "About
  // Device" and one for "extension is connecting to 1 device"). The below for
  // loop sets up 19 profiles, which will consume 39 menu items (1 + 19 * 2).
  size_t num_profiles = 19;
  std::vector<DeviceSystemTrayIconTestBase::ProfileItem>
      profile_connection_counts;
  for (size_t idx = 0; idx < num_profiles; idx++) {
    std::string profile_name = base::StringPrintf("user%zu", idx);
    auto* profile = CreateTestingProfile(profile_name);
    auto extension = CreateExtensionWithName("Test Extension");
    AddExtensionToProfile(profile, extension.get());
    auto* connection_tracker =
        GetDeviceConnectionTracker(profile, /*create=*/true);
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
    auto* connection_tracker = GetDeviceConnectionTracker(profile,
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

void DeviceStatusIconTestBase::TestProfileUserNameExtensionOrigin() {
  std::vector<DeviceSystemTrayIconTestBase::ProfileItem>
      profile_connection_counts;
  // std::get<1>(profiles[i]) is the old profile name.
  // std::get<2>(profiles[i]) is the new profile name.
  std::vector<std::tuple<Profile*, std::string, std::string>> profiles;
  for (size_t idx = 0; idx < 2; idx++) {
    std::string profile_name = base::StringPrintf("user%zu", idx);
    std::string new_profile_name = base::StringPrintf("user%zu-newname", idx);
    auto* profile = CreateTestingProfile(profile_name);
    auto extension = CreateExtensionWithName("Test Extension");
    AddExtensionToProfile(profile, extension.get());
    auto* connection_tracker = GetDeviceConnectionTracker(profile,
                                                          /*create=*/true);
    connection_tracker->IncrementConnectionCount(extension->origin());
    profile_connection_counts.push_back(
        {profile, {{extension->origin(), 1, extension->name()}}});
    profiles.emplace_back(profile, profile_name, new_profile_name);
  }
  CheckIcon(profile_connection_counts);

  const auto* status_tray = static_cast<MockStatusTray*>(
      TestingBrowserProcess::GetGlobal()->status_tray());
  ASSERT_TRUE(status_tray);
  ASSERT_EQ(status_tray->GetStatusIconsForTest().size(), 1u);
  const auto* status_icon = static_cast<MockStatusIcon*>(
      status_tray->GetStatusIconsForTest().back().get());

  // Sort the |profiles| by the address of the profile
  // pointer. This is necessary because the menu items are created by
  // iterating through a structure of flat_map<Profile*, bool>.
  base::ranges::sort(profiles);

  // The below is status icon items layout for non MacOS devices, profile1 name
  // is on [3] and profile2
  // name is on [7].
  // ---------------------------------------------------
  // [0]|Google Chrome is accessing Device device(s)   |
  // [1]|About Device device                           |
  // [2]|---------------Separator----------------------|
  // [3]|Profile1 name                                 |
  // [4]|Device Content Setting for Proifle1           |
  // [5]|Extension name                                |
  // [6]|---------------Separator----------------------|
  // [7]|Profile2 name                                 |
  // [8]|Device Content Setting for Proifle2           |
  // [9]|Extension name                                |
  // ---------------------------------------------------
  // The title of the status icon menu is from its tooltip on MacOS. The below
  // is status icon items layout in MacOs, profile1 name is on [2] and profile2
  // name is on [6].
  // ---------------------------------------------------
  // [0]|About Device device                           |
  // [1]|---------------Separator----------------------|
  // [2]|Profile1 name                                 |
  // [3]|Device Content Setting for Proifle1           |
  // [4]|Extension name                                |
  // [5]|---------------Separator----------------------|
  // [6]|Profile2 name                                 |
  // [7]|Device Content Setting for Proifle2           |
  // [8]|Extension name                                |

#if !BUILDFLAG(IS_MAC)
  int profile_position1 = 3;
  int profile_position2 = 7;
#else
  int profile_position1 = 2;
  int profile_position2 = 6;
#endif  //! BUILDFLAG(IS_MAC)

  // Check the current profile names.
  {
    auto* menu_item = status_icon->menu_item();
    CheckMenuItemLabel(
        menu_item, profile_position1,
        base::UTF8ToUTF16(std::get<0>(profiles[0])->GetProfileUserName()));
    CheckMenuItemLabel(
        menu_item, profile_position2,
        base::UTF8ToUTF16(std::get<0>(profiles[1])->GetProfileUserName()));
  }

  // Change the first profile name.
  {
    profile_manager()
        ->profile_attributes_storage()
        ->GetProfileAttributesWithPath(std::get<0>(profiles[0])->GetPath())
        ->SetLocalProfileName(base::UTF8ToUTF16(std::get<2>(profiles[0])),
                              /*is_default_name*/ false);

    auto* menu_item = status_icon->menu_item();
    CheckMenuItemLabel(menu_item, profile_position1,
                       base::UTF8ToUTF16(std::get<2>(profiles[0])));
    CheckMenuItemLabel(menu_item, profile_position2,
                       base::UTF8ToUTF16(std::get<1>(profiles[1])));
  }

  // Change the second profile name.
  {
    profile_manager()
        ->profile_attributes_storage()
        ->GetProfileAttributesWithPath(std::get<0>(profiles[1])->GetPath())
        ->SetLocalProfileName(base::UTF8ToUTF16(std::get<2>(profiles[1])),
                              /*is_default_name*/ false);

    auto* menu_item = status_icon->menu_item();
    CheckMenuItemLabel(menu_item, profile_position1,
                       base::UTF8ToUTF16(std::get<2>(profiles[0])));
    CheckMenuItemLabel(menu_item, profile_position2,
                       base::UTF8ToUTF16(std::get<2>(profiles[1])));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void DeviceStatusIconTestBase::CheckSeparatorMenuItem(
    StatusIconMenuModel* menu_item,
    size_t menu_idx) {
  ASSERT_LT(menu_idx, menu_item->GetItemCount());
  EXPECT_EQ(menu_item->GetSeparatorTypeAt(menu_idx), ui::NORMAL_SEPARATOR);
}

void DeviceStatusIconTestBase::CheckMenuItemLabel(
    StatusIconMenuModel* menu_item,
    size_t menu_idx,
    std::u16string label) {
  ASSERT_LT(menu_idx, menu_item->GetItemCount());
  EXPECT_EQ(menu_item->GetLabelAt(menu_idx), label);
}

void DeviceStatusIconTestBase::CheckClickableMenuItem(
    StatusIconMenuModel* menu_item,
    size_t menu_idx,
    std::u16string label,
    int command_id,
    bool click) {
  CheckMenuItemLabel(menu_item, menu_idx, label);
  ASSERT_LT(menu_idx, menu_item->GetItemCount());
  EXPECT_EQ(menu_item->GetCommandIdAt(menu_idx), command_id);
  if (click) {
    menu_item->ActivatedAt(menu_idx);
  }
}
