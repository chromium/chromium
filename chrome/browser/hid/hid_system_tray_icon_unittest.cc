// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon_unittest.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool ContainsProfile(std::vector<base::WeakPtr<Profile>> profiles,
                     Profile* profile) {
  return base::ranges::count_if(profiles, [profile](const auto& entry) {
           return entry && entry.get() == profile;
         }) > 0;
}

}  // namespace

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
  task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
  CheckIconHidden();
}

void HidSystemTrayIconTestBase::TestProfileShownWhileUnstaging() {
  Profile* profile = CreateTestingProfile("user");
  HidConnectionTracker* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true);
  CheckIconHidden();

  // Check the profile is visible while unstaging during 1000ms interval and
  // removed after that.
  {
    hid_connection_tracker->IncrementConnectionCount();
    CheckIcon({{profile, 1}});

    hid_connection_tracker->DecrementConnectionCount();
    task_environment()->FastForwardBy(base::Seconds(6));
    // Connection count is updated immediately while the profile is scheduled
    // to be removed later.
    CheckIcon({{profile, 0}});
    task_environment()->FastForwardBy(base::Seconds(4));
    CheckIconHidden();
  }

  // Simulate bouncing the device connection and make sure the profile exist
  // during 1000ms interval and removed eventually.
  {
    hid_connection_tracker->IncrementConnectionCount();
    CheckIcon({{profile, 1}});
    hid_connection_tracker->DecrementConnectionCount();
    CheckIcon({{profile, 0}});
    hid_connection_tracker->IncrementConnectionCount();
    CheckIcon({{profile, 1}});
    hid_connection_tracker->DecrementConnectionCount();
    CheckIcon({{profile, 0}});
    hid_connection_tracker->IncrementConnectionCount();
    CheckIcon({{profile, 1}});
    hid_connection_tracker->DecrementConnectionCount();
    CheckIcon({{profile, 0}});
    task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
    CheckIconHidden();
  }
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
  // tray icon immediately.
  profile_manager()->DeleteTestingProfile(profiles[0]->GetProfileUserName());
  CheckIcon({{profiles[1], 1}, {profiles[2], 1}});

  // The remaining two profiles are removed 5 seconds apart.
  hid_connection_trackers[2]->DecrementConnectionCount();
  // Connection count is updated immediately while the profile is scheduled
  // to be removed later.
  CheckIcon({{profiles[1], 1}, {profiles[2], 0}});

  task_environment()->FastForwardBy(base::Seconds(5));
  hid_connection_trackers[1]->DecrementConnectionCount();
  CheckIcon({{profiles[1], 0}, {profiles[2], 0}});

  task_environment()->FastForwardBy(base::Seconds(5));
  CheckIcon({{profiles[1], 0}});
  task_environment()->FastForwardBy(base::Seconds(5));
  CheckIconHidden();
}

class MockHidSystemTrayIcon : public HidSystemTrayIcon {
 public:
  MOCK_METHOD(void, AddProfile, (Profile*), (override));
  MOCK_METHOD(void, RemoveProfile, (Profile*), (override));
  MOCK_METHOD(void, NotifyConnectionCountUpdated, (Profile*), (override));
};

class HidSystemTrayIconTest : public HidSystemTrayIconTestBase {
 public:
  void SetUp() override {
    HidSystemTrayIconTestBase::SetUp();
    hid_system_tray_icon_ = std::make_unique<MockHidSystemTrayIcon>();
  }

  // HidSystemTrayIconTestBase
  void CheckIcon(const std::vector<std::pair<Profile*, size_t>>&
                     profile_connection_counts) override {}
  void CheckIconHidden() override {}

  MockHidSystemTrayIcon& hid_system_tray_icon() {
    return *hid_system_tray_icon_.get();
  }

 private:
  std::unique_ptr<MockHidSystemTrayIcon> hid_system_tray_icon_;
};

TEST_F(HidSystemTrayIconTest, UnstageProfile) {
  Profile* profile = CreateTestingProfile("user");

  // When immediate flag is set to true.
  {
    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile));
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ true);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }

  // When immediate flag is set to false.
  {
    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile)).Times(0);
    EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile));
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);
    EXPECT_TRUE(
        ContainsProfile(hid_system_tray_icon().unstaging_profiles_, profile));
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());

    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile));
    task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }

  // A profile is scheduled to be removed followed by an immediate removal
  // request.
  {
    EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile));
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);
    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile));
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ true);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());

    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile)).Times(0);
    task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }

  // A profile is scheduled to be removed then stage profile request comes in.
  {
    // NotifyConnectionCountUpdated is called twice: once when the profile is
    // unstaged, and again when the profile is staged.
    EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile))
        .Times(2);
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);
    EXPECT_CALL(hid_system_tray_icon(), AddProfile(profile)).Times(0);
    hid_system_tray_icon().StageProfile(profile);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);

    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile)).Times(0);
    task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }

  // Back-to-back requests of scheduled profile removal.
  {
    EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile));
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);

    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile));
    task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }

  // CleanUpProfiles is called after an unstaging profile is destroyed.
  {
    Profile* profile_to_be_destroyed = CreateTestingProfile("user2");
    EXPECT_CALL(hid_system_tray_icon(),
                NotifyConnectionCountUpdated(profile_to_be_destroyed));
    hid_system_tray_icon().UnstageProfile(profile_to_be_destroyed,
                                          /*immediate*/ false);

    profile_manager()->DeleteTestingProfile(
        profile_to_be_destroyed->GetProfileUserName());
    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile_to_be_destroyed))
        .Times(0);
    task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
    // The |unstaging_profiles_| should still be cleared to empty.
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }

  // Two profiles removed 5 seconds apart.
  {
    Profile* second_profile = CreateTestingProfile("user2");
    EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile));
    hid_system_tray_icon().UnstageProfile(profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);
    task_environment()->FastForwardBy(base::Seconds(5));
    EXPECT_CALL(hid_system_tray_icon(),
                NotifyConnectionCountUpdated(second_profile));
    hid_system_tray_icon().UnstageProfile(second_profile, /*immediate*/ false);
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 2u);

    // 10 seconds later, |profile| is removed.
    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile));
    task_environment()->FastForwardBy(base::Seconds(5));
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 1u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());

    // 15 second later, |second_profile) is removed.
    EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(second_profile));
    task_environment()->FastForwardBy(base::Seconds(5));
    EXPECT_EQ(hid_system_tray_icon().unstaging_profiles_.size(), 0u);
    testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());
  }
}

// This test case just to make sure it can run through the scenario without
// causing any unexpected crash.
TEST_F(HidSystemTrayIconTest, CallbackAfterHidSystemTrayIconDestroyed) {
  Profile* profile = CreateTestingProfile("user");
  auto hid_system_tray_icon = std::make_unique<MockHidSystemTrayIcon>();
  EXPECT_CALL(*hid_system_tray_icon, NotifyConnectionCountUpdated(profile));
  hid_system_tray_icon->UnstageProfile(profile, /*immediate*/ false);
  EXPECT_EQ(hid_system_tray_icon->unstaging_profiles_.size(), 1u);
  EXPECT_TRUE(
      ContainsProfile(hid_system_tray_icon->unstaging_profiles_, profile));

  hid_system_tray_icon.reset();
  task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
}
