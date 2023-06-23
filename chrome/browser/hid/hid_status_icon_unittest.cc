// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_status_icon.h"

#include <string>

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/device_notifications/device_status_icon_unittest.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {
class MockHidConnectionTracker : public HidConnectionTracker {
 public:
  explicit MockHidConnectionTracker(Profile* profile)
      : HidConnectionTracker(profile) {}
  ~MockHidConnectionTracker() override = default;
  MOCK_METHOD(void, ShowContentSettingsExceptions, (), (override));
  MOCK_METHOD(void, ShowSiteSettings, (const url::Origin&), (override));
};
}  // namespace

class HidStatusIconTest : public DeviceStatusIconTestBase {
 public:
  HidStatusIconTest()
      : DeviceStatusIconTestBase(
            /*about_device_label=*/u"About HID devices",
            /*device_content_settings_label_=*/u"HID settings") {}

  void ResetTestingBrowserProcessSystemTrayIcon() override {
    TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
  }

  std::u16string GetExpectedTitle(size_t num_origins,
                                  size_t num_connections) override {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // The text might use "Google Chrome" or "Chromium" depending
    // is_chrome_branded in the build config file, hence using l10n_util to get
    // the expected string.
    if (num_origins == 1) {
      return l10n_util::GetPluralStringFUTF16(
          IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_SINGLE_EXTENSION,
          static_cast<int>(num_connections));
    }
    return l10n_util::GetPluralStringFUTF16(
        IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_MULTIPLE_EXTENSIONS,
        static_cast<int>(num_connections));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    NOTREACHED_NORETURN();
  }

  void SetDeviceConnectionTrackerTestingFactory(Profile* profile) override {
    HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating([](content::BrowserContext* browser_context) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<MockHidConnectionTracker>(
                  Profile::FromBrowserContext(browser_context)));
        }));
  }

  MockDeviceConnectionTracker* GetDeviceConnectionTracker(
      Profile* profile,
      bool create) override {
    return static_cast<MockDeviceConnectionTracker*>(
        static_cast<DeviceConnectionTracker*>(
            HidConnectionTrackerFactory::GetForProfile(profile, create)));
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/1457424): Re-enable this test
TEST_F(HidStatusIconTest, DISABLED_SingleProfileEmptyNameExtentionOrigins) {
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

// TODO(crbug.com/1457424): Re-enable this test
TEST_F(HidStatusIconTest, DISABLED_SingleProfileNonEmptyNameExtentionOrigins) {
  TestSingleProfileExtentionOrigins();
}

TEST_F(HidStatusIconTest, DISABLED_BounceConnectionExtensionOrigins) {
  TestBounceConnectionExtensionOrigins();
}

TEST_F(HidStatusIconTest, DISABLED_MultipleProfilesExtensionOrigins) {
  TestMultipleProfilesExtensionOrigins();
}

TEST_F(HidStatusIconTest, DISABLED_NumCommandIdOverLimitExtensionOrigin) {
  TestNumCommandIdOverLimitExtensionOrigin();
}

// TODO(crbug.com/1457424): Re-enable this test
TEST_F(HidStatusIconTest, DISABLED_ExtensionRemoval) {
  TestExtensionRemoval();
}

// TODO(crbug.com/1457424): Re-enable this test
TEST_F(HidStatusIconTest, DISABLED_ProfileUserNameExtensionOrigin) {
  TestProfileUserNameExtensionOrigin();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
