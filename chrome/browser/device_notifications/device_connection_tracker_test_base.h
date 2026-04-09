// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_CONNECTION_TRACKER_TEST_BASE_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_CONNECTION_TRACKER_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_notifications/device_connection_tracker.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/device_notifications/device_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/memory/scoped_refptr.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_id.h"
#endif

class Profile;

class DeviceConnectionTrackerTestBase : public InProcessBrowserTest {
 public:
  DeviceConnectionTrackerTestBase();
  DeviceConnectionTrackerTestBase(const DeviceConnectionTrackerTestBase&) =
      delete;
  DeviceConnectionTrackerTestBase& operator=(
      const DeviceConnectionTrackerTestBase&) = delete;
  ~DeviceConnectionTrackerTestBase() override;

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpLocalStatePrefService(PrefService* local_state) override;

  Profile& StartUserSession(const AccountId& account_id);
#endif

  virtual DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                              bool create) = 0;

  virtual MockDeviceSystemTrayIcon* GetMockDeviceSystemTrayIcon() = 0;

  void TestDeviceConnection(
      bool has_system_tray_icon,
      const std::vector<std::pair<url::Origin, std::string>>&
          origin_name_pairs);

  // Test the scenario when the origin is whitelisted.
  void TestWhitelistedOrigin(
      const std::pair<url::Origin, std::string> whitelisted_origin,
      const std::pair<url::Origin, std::string> origin2);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<const extensions::Extension> CreateExtensionWithName(
      const std::string& extension_name);

  // Create a extension with |extension_name| and |extension_id|.
  scoped_refptr<const extensions::Extension> CreateExtensionWithNameAndId(
      const std::string& extension_name,
      const std::string& extension_id);

  void AddExtensionToProfile(Profile* profile,
                             const extensions::Extension* extension);

  void TestDeviceConnectionExtensionOrigins(bool has_system_tray_icon);

  void TestProfileDestroyedExtensionOrigin();

  // Run TestWhitelistedOrigin with a whitelisted origin and a non-whitelisted
  // origin.
  void TestSingleProfileWhitelistedExtension(
      std::string whitelisted_extension_name,
      std::string whitelisted_extension_id);

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS)
 protected:
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId("test@gmail.com", GaiaId("9876543210"));
#endif
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_CONNECTION_TRACKER_TEST_BASE_H_
