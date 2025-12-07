// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/application_advanced_protection_status_detector.h"

#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace safe_browsing {

namespace {

class MockAdvancedProtectionStatusManager
    : public AdvancedProtectionStatusManager {
 public:
  MockAdvancedProtectionStatusManager() = default;
  ~MockAdvancedProtectionStatusManager() override = default;

  void SetAdvancedProtectionStatusForTesting(bool enrolled) override {
    is_under_advanced_protection_ = enrolled;
    NotifyObserversStatusChanged();
  }

  bool IsUnderAdvancedProtection() const override {
    return is_under_advanced_protection_;
  }

 private:
  bool is_under_advanced_protection_ = false;
};

std::unique_ptr<KeyedService> BuildMockAdvancedProtectionStatusManager(
    content::BrowserContext* context) {
  return std::make_unique<MockAdvancedProtectionStatusManager>();
}

class MockApplicationAdvancedProtectionStatusDetectorObserver
    : public ApplicationAdvancedProtectionStatusDetector::StatusObserver {
 public:
  MOCK_METHOD(void,
              OnApplicationAdvancedProtectionStatusChanged,
              (bool),
              (override));
};

}  // namespace

class ApplicationAdvancedProtectionStatusDetectorTest : public testing::Test {
 public:
  ApplicationAdvancedProtectionStatusDetectorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { EXPECT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override {
    // Necessary to prevent unexpected mock called from test teardown.
    observation_.Reset();
    profile_manager_.DeleteAllTestingProfiles();
  }

  ProfileManager* profile_manager() {
    return profile_manager_.profile_manager();
  }

  // Returns an instance of ApplicationAdvancedProtectionStatusDetector for
  // testing with `observer_` installed. The instance will be cleaned up on test
  // destruction. May only be used once per test.
  ApplicationAdvancedProtectionStatusDetector* MakeTestDetectorWithObserver() {
    application_ap_detector_ =
        std::make_unique<ApplicationAdvancedProtectionStatusDetector>(
            profile_manager());
    observation_.Observe(application_ap_detector_.get());
    return application_ap_detector_.get();
  }

 protected:
  TestingProfile* CreateProfile(const std::string& profile_name) {
    TestingProfile::TestingFactories factories;
    factories.emplace_back(
        AdvancedProtectionStatusManagerFactory::GetInstance(),
        base::BindRepeating(&BuildMockAdvancedProtectionStatusManager));
    return profile_manager_.CreateTestingProfile(profile_name,
                                                 std::move(factories));
  }

  MockAdvancedProtectionStatusManager* GetAPManager(Profile* profile) {
    return static_cast<MockAdvancedProtectionStatusManager*>(
        AdvancedProtectionStatusManagerFactory::GetForProfile(profile));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  testing::StrictMock<MockApplicationAdvancedProtectionStatusDetectorObserver>
      observer_;
  std::unique_ptr<ApplicationAdvancedProtectionStatusDetector>
      application_ap_detector_;
  base::ScopedObservation<
      ApplicationAdvancedProtectionStatusDetector,
      ApplicationAdvancedProtectionStatusDetector::StatusObserver>
      observation_{&observer_};
};

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, NoProfiles) {
  EXPECT_FALSE(MakeTestDetectorWithObserver()->IsUnderAdvancedProtection());
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       InitializedWithExistingProfilesWithAPDisabled) {
  base::HistogramTester histogram_tester;
  CreateProfile("profile1");
  CreateProfile("profile2");

  EXPECT_FALSE(MakeTestDetectorWithObserver()->IsUnderAdvancedProtection());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Disabled",
      ApplicationAdvancedProtectionEvent::kInitialized, /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       InitializedWithExistingProfilesWithAPEnabled) {
  base::HistogramTester histogram_tester;
  CreateProfile("profile1");
  TestingProfile* profile_2 = CreateProfile("profile2");
  GetAPManager(profile_2)->SetAdvancedProtectionStatusForTesting(true);

  auto detector = std::make_unique<ApplicationAdvancedProtectionStatusDetector>(
      profile_manager());
  EXPECT_TRUE(detector->IsUnderAdvancedProtection());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Enabled",
      ApplicationAdvancedProtectionEvent::kInitialized, /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       InitializedWithExistingProfilesWithMultipleAPEnabled) {
  base::HistogramTester histogram_tester;
  TestingProfile* profile_1 = CreateProfile("profile1");
  TestingProfile* profile_2 = CreateProfile("profile2");
  GetAPManager(profile_1)->SetAdvancedProtectionStatusForTesting(true);
  GetAPManager(profile_2)->SetAdvancedProtectionStatusForTesting(true);

  auto detector = std::make_unique<ApplicationAdvancedProtectionStatusDetector>(
      profile_manager());

  EXPECT_TRUE(detector->IsUnderAdvancedProtection());
  // Verify that only one event should be logged.
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Enabled",
      ApplicationAdvancedProtectionEvent::kInitialized, /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       ProfileAddedWithAPDisabled) {
  base::HistogramTester histogram_tester;
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  TestingProfile* profile = CreateProfile("profile1");

  // AdvancedProtectionStatusManager notify observer with false on non-APP
  // profile sign-in.
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(false);
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());
  // Expected no histogram logged for non-AP profile added.
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Enabled",
      ApplicationAdvancedProtectionEvent::kProfileAdded,
      /* expected_count */ 0);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Disabled",
      ApplicationAdvancedProtectionEvent::kProfileAdded,
      /* expected_count */ 0);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       ProfileAddedWithAPEnabled) {
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  TestingProfile* profile = CreateProfile("profile1");
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  // The detector updates its status upon profile addition and AP status change.
  // Since the profile is created with AP enabled, we expect one notification.
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       MultipleAdvancedProtectionStatusChangedNotifiedWithSameValue) {
  base::HistogramTester histogram_tester;

  auto* application_ap_detector = MakeTestDetectorWithObserver();
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(_))
      .Times(0);
  TestingProfile* profile = CreateProfile("profile1");
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(false);
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(false);
  // Expect observer are not notified on addition of non APP profile.
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // The detector updates its status upon profile addition and AP status change.
  // Since the profile is changed to APP, we expect one notification; regardless
  // of how many time AP status manager notifies.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true))
      .Times(1);
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false))
      .Times(1);
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(false);

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Enabled",
      ApplicationAdvancedProtectionEvent::
          kProfileAdvancedProtectionStatusChanged,
      /* expected_count */ 1);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Disabled",
      ApplicationAdvancedProtectionEvent::
          kProfileAdvancedProtectionStatusChanged,
      /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, APEnabledThenDisabled) {
  base::HistogramTester histogram_tester;
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  TestingProfile* profile = CreateProfile("profile1");

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(false);

  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Disabled",
      ApplicationAdvancedProtectionEvent::
          kProfileAdvancedProtectionStatusChanged,
      /* expected_count */ 1);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Enabled",
      ApplicationAdvancedProtectionEvent::
          kProfileAdvancedProtectionStatusChanged,
      /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, MultipleProfiles) {
  base::HistogramTester histogram_tester;
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  TestingProfile* profile1 = CreateProfile("profile1");
  TestingProfile* profile2 = CreateProfile("profile2");

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  GetAPManager(profile1)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Enabling for another profile shouldn't trigger a notification.
  GetAPManager(profile2)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  // Disabling for the first profile shouldn't trigger a notification.
  GetAPManager(profile1)->SetAdvancedProtectionStatusForTesting(false);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  // Disabling for the second profile should trigger a notification.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  GetAPManager(profile2)->SetAdvancedProtectionStatusForTesting(false);
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Disabled",
      ApplicationAdvancedProtectionEvent::
          kProfileAdvancedProtectionStatusChanged,
      /* expected_count */ 1);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Enabled",
      ApplicationAdvancedProtectionEvent::
          kProfileAdvancedProtectionStatusChanged,
      /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, ProfileRemoved) {
  base::HistogramTester histogram_tester;
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  TestingProfile* profile1 = CreateProfile("profile1");
  TestingProfile* profile2 = CreateProfile("profile2");

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  GetAPManager(profile1)->SetAdvancedProtectionStatusForTesting(true);
  GetAPManager(profile2)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  profile_manager_.DeleteTestingProfile("profile1");
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  profile_manager_.DeleteTestingProfile("profile2");

  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ApplicationAdvancedProtectionStatusDetector.Changed."
      "Disabled",
      ApplicationAdvancedProtectionEvent::kProfileRemoved,
      /* expected_count */ 1);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       MultipleProfilesAddRemove) {
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  // 1. Create profile1 (non-AP). Status should be false.
  TestingProfile* profile1 = CreateProfile("profile1");
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  // 2. Create profile2 (non-AP). Status should still be false.
  CreateProfile("profile2");
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  // 3. Enable AP for profile1. Status becomes true.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  GetAPManager(profile1)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // 4. Add multiple profiles. Create profile3 and profile4 and enable AP for
  // profile3. Status remains true.
  TestingProfile* profile3 = CreateProfile("profile3");
  GetAPManager(profile3)->SetAdvancedProtectionStatusForTesting(true);
  CreateProfile("profile4");
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  // 5. Remove profile1 (AP) and profile2 (non-AP). Status remains true due to
  // profile3.
  profile_manager_.DeleteTestingProfile("profile1");
  profile_manager_.DeleteTestingProfile("profile2");
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  //  6. Remove profile3 (AP).
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  profile_manager_.DeleteTestingProfile("profile3");
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, IsOffTheRecordProfile) {
  TestingProfile* ap_profile = CreateProfile("profile1");
  GetAPManager(ap_profile)->SetAdvancedProtectionStatusForTesting(true);
  TestingProfile* non_ap_profile = CreateProfile("profile2");
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  // Add Off the Record Profiles.
  // Status should not change.
  Profile* otf_profile = ap_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(), true);
  application_ap_detector->OnProfileAdded(otf_profile);
  application_ap_detector->OnProfileAdded(
      non_ap_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(), true));

  // Remove Off the Record Profile.
  application_ap_detector->OnProfileWillBeDestroyed(otf_profile);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());

  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  profile_manager_.DeleteTestingProfile("profile1");
  testing::Mock::VerifyAndClearExpectations(&observer_);

  profile_manager_.DeleteTestingProfile("profile2");
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, IsGuestSessionProfile) {
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  // Guest profiles should not be considered for Advanced Protection status.
  profile_manager_.CreateGuestProfile();
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  // Even if AP is enabled for a regular profile, a guest profile should not
  // change the status if no other regular profiles have AP enabled.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  TestingProfile* regular_profile = CreateProfile("regular_profile");
  GetAPManager(regular_profile)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Deleting guest profile should be a no-op.
  profile_manager_.DeleteGuestProfile();

  // Removing the regular profile should revert the status.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  profile_manager_.DeleteTestingProfile("regular_profile");
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, IsSystemProfile) {
  auto* application_ap_detector = MakeTestDetectorWithObserver();
  // System profiles should not be considered for Advanced Protection status.
  profile_manager_.CreateSystemProfile();
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());

  // A system profile should not influence the status.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  TestingProfile* regular_profile = CreateProfile("regular_profile");
  GetAPManager(regular_profile)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Deleting guest profile should be a no-op.
  profile_manager_.DeleteSystemProfile();

  // Removing the regular profile should revert the status.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  profile_manager_.DeleteTestingProfile("regular_profile");
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest, RemoveObserver) {
  auto application_ap_detector =
      std::make_unique<ApplicationAdvancedProtectionStatusDetector>(
          profile_manager());
  application_ap_detector->AddObserver(&observer_);

  TestingProfile* profile = CreateProfile("profile1");

  // Expect to be notified when AP is enabled.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Remove the observer.
  application_ap_detector->RemoveObserver(&observer_);

  // Expect NOT to be notified when AP is disabled after observer removal.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false))
      .Times(0);
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(false);
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ApplicationAdvancedProtectionStatusDetectorTest,
       OnProfileManagerDestroyingResetsStatus) {
  auto* application_ap_detector = MakeTestDetectorWithObserver();

  TestingProfile* profile = CreateProfile("profile1");
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(true));
  GetAPManager(profile)->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_TRUE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Simulate ProfileManager destruction.
  EXPECT_CALL(observer_, OnApplicationAdvancedProtectionStatusChanged(false));
  application_ap_detector->OnProfileManagerDestroying();
  EXPECT_FALSE(application_ap_detector->IsUnderAdvancedProtection());
  testing::Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace safe_browsing
