// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_STATUS_ICON_UNITTEST_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_STATUS_ICON_UNITTEST_H_

#include <string>

#include "chrome/browser/device_notifications/device_system_tray_icon_unittest.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"

class DeviceStatusIconTestBase : public DeviceSystemTrayIconTestBase {
 public:
  DeviceStatusIconTestBase(std::u16string about_device_label,
                           std::u16string device_content_settings_label);

  void SetUp() override;
  void TearDown() override;
  void CheckIcon(const std::vector<DeviceSystemTrayIconTestBase::ProfileItem>&
                     profile_connection_counts) override;
  void CheckIconHidden() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void TestNumCommandIdOverLimitExtensionOrigin();
  void TestProfileUserNameExtensionOrigin();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

 protected:
  // This is specifically used in the test case of
  // NumCommandIdOverLimitExtensionOrigin, where the input parameter
  // profile_connection_counts may not capture all of the origins because the
  // limit is exceeded.
  std::optional<int> override_title_total_connection_count_;

 private:
  void CheckSeparatorMenuItem(StatusIconMenuModel* menu_item, size_t menu_idx);
  void CheckMenuItemLabel(StatusIconMenuModel* menu_item,
                          size_t menu_idx,
                          std::u16string label);
  void CheckClickableMenuItem(StatusIconMenuModel* menu_item,
                              size_t menu_idx,
                              std::u16string label,
                              int command_id,
                              bool click);

  std::u16string about_device_label_;
  std::u16string device_content_settings_label_;
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_STATUS_ICON_UNITTEST_H_
