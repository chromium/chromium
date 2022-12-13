// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon_unittest.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

MockHidConnectionTracker::MockHidConnectionTracker(Profile* profile)
    : HidConnectionTracker(profile) {}

MockHidConnectionTracker::~MockHidConnectionTracker() = default;

std::u16string HidSystemTrayIconTestBase::GetExpectedButtonTitleForProfile(
    Profile* profile) {
  const std::string profile_name = profile->GetProfileUserName();
  if (profile_name.empty()) {
    return u"Manage HID devices";
  }
  return base::UTF8ToUTF16(
      base::StringPrintf("Manage HID devices for %s", profile_name.c_str()));
}

std::u16string HidSystemTrayIconTestBase::GetExpectedIconTooltip(
    size_t num_devices) {
  // It might be either "Chromium is connected to a HID device" or "Google
  // Chrome is connected to a HID device" depending is_chrome_branded in the
  // build config file, hence using l10n_util to get the expected string.
  return l10n_util::GetPluralStringFUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_TOOLTIP,
                                          static_cast<int>(num_devices));
}

BrowserContextKeyedServiceFactory::TestingFactory
HidSystemTrayIconTestBase::GetHidConnectionTrackerTestingFactory() {
  return base::BindRepeating([](content::BrowserContext* browser_context) {
    return static_cast<std::unique_ptr<KeyedService>>(
        std::make_unique<MockHidConnectionTracker>(
            Profile::FromBrowserContext(browser_context)));
  });
}

Profile* HidSystemTrayIconTestBase::CreateTestingProfile(
    const std::string& profile_name) {
  // TODO(crbug.com/1399310): Pass testing factory when creating profile.
  // Ideally, we should be able to pass testing factory when calling profile
  // manager's CreateTestingProfile. However, due to the fact that:
  // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown as
  //    part of setting testing factory.
  // 2) HidConnectionTrackerFactory::BrowserContextShutdown() at some point need
  //    valid profile_metrics::GetBrowserProfileType() as part of
  //    HidConnectionTrackerFactory::GetForProfile().
  // It will hit failure in profile_metrics::GetBrowserProfileType() due to
  // profile is not initialized properly before setting testing factory. As a
  // result, here create a profile then call SetTestingFactory to inject
  // MockHidConnectionTracker.
  Profile* profile = profile_manager()->CreateTestingProfile(profile_name);
  HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
      profile, GetHidConnectionTrackerTestingFactory());
  return profile;
}

void HidSystemTrayIconTestBase::TestSingleProfile() {
  Profile* profile = CreateTestingProfile("user");
  HidConnectionTracker* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true);
  CheckIconHidden();

  hid_connection_tracker->IncrementConnectionCount();
  CheckIcon({{profile, 1}});

  hid_connection_tracker->IncrementConnectionCount();
  CheckIcon({{profile, 2}});

  hid_connection_tracker->IncrementConnectionCount();
  CheckIcon({{profile, 3}});

  hid_connection_tracker->DecrementConnectionCount();
  CheckIcon({{profile, 2}});

  hid_connection_tracker->DecrementConnectionCount();
  CheckIcon({{profile, 1}});

  hid_connection_tracker->DecrementConnectionCount();
  CheckIconHidden();
}

void HidSystemTrayIconTestBase::TestMultipleProfiles() {
  size_t num_profiles = 3;
  std::vector<Profile*> profiles;
  std::vector<HidConnectionTracker*> hid_connection_trackers;
  for (size_t idx = 0; idx < num_profiles; idx++) {
    std::string profile_name = base::StringPrintf("user%zu", idx);
    profiles.emplace_back(CreateTestingProfile(profile_name));
    hid_connection_trackers.emplace_back(
        HidConnectionTrackerFactory::GetForProfile(profiles.back(),
                                                   /*create=*/true));
  }
  CheckIconHidden();

  hid_connection_trackers[0]->IncrementConnectionCount();
  CheckIcon({{profiles[0], 1}});

  hid_connection_trackers[1]->IncrementConnectionCount();
  CheckIcon({{profiles[0], 1}, {profiles[1], 1}});

  hid_connection_trackers[2]->IncrementConnectionCount();
  CheckIcon({{profiles[0], 1}, {profiles[1], 1}, {profiles[2], 1}});

  // Destroyed a profile will remove it from being tracked in the hid system
  // tray icon.
  profile_manager()->DeleteTestingProfile(profiles[0]->GetProfileUserName());
  CheckIcon({{profiles[1], 1}, {profiles[2], 1}});

  hid_connection_trackers[2]->DecrementConnectionCount();
  CheckIcon({{profiles[1], 1}});

  hid_connection_trackers[1]->DecrementConnectionCount();
  CheckIconHidden();
}
