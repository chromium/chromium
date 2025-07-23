// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service_factory.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class NotificationTelemetryServiceFactoryTest : public testing::Test {
 protected:
  NotificationTelemetryServiceFactoryTest() = default;
  ~NotificationTelemetryServiceFactoryTest() override = default;
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());

    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();
  }
  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
};

TEST_F(NotificationTelemetryServiceFactoryTest, EnabledForRegularProfile) {
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("testing_profile");
#if !BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64))
  bool expect_service_created = true;
#else
  // Service is not created for Android arm32 devices.
  bool expect_service_created = false;
#endif  // !(!BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_ANDROID) &&
        // defined(ARCH_CPU_ARM64)))
  EXPECT_EQ(
      NotificationTelemetryServiceFactory::GetForProfile(profile) != nullptr,
      expect_service_created);
}

TEST_F(NotificationTelemetryServiceFactoryTest, DisabledForIncognitoMode) {
  TestingProfile* profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(nullptr,
            NotificationTelemetryServiceFactory::GetForProfile(profile));
}

TEST_F(NotificationTelemetryServiceFactoryTest, DisabledForGuestMode) {
  Profile* profile =
      profile_manager_->CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            NotificationTelemetryServiceFactory::GetForProfile(profile));
}

TEST_F(NotificationTelemetryServiceFactoryTest,
       CreatedWithDatabaseManagerWhenGlobalCacheListDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      kGlobalCacheListForGatingNotificationProtections);
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("testing_profile");
  NotificationTelemetryService* notification_telemetry_service =
      NotificationTelemetryServiceFactory::GetForProfile(profile);
#if !BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64))
  EXPECT_NE(nullptr, notification_telemetry_service);
  EXPECT_NE(nullptr, notification_telemetry_service->database_manager_);
#else
  // Service is not created for Android arm32 devices.
  EXPECT_EQ(nullptr, notification_telemetry_service);
#endif  // !(!BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_ANDROID) &&
        // defined(ARCH_CPU_ARM64)))
}

TEST_F(NotificationTelemetryServiceFactoryTest,
       CreatedWithoutDatabaseManagerWhenGlobalCacheListEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      kGlobalCacheListForGatingNotificationProtections);
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile("testing_profile");
  NotificationTelemetryService* notification_telemetry_service =
      NotificationTelemetryServiceFactory::GetForProfile(profile);
#if !BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64))
  EXPECT_NE(nullptr, notification_telemetry_service);
  EXPECT_EQ(nullptr, notification_telemetry_service->database_manager_);
#else
  // Service is not created for Android arm32 devices.
  EXPECT_EQ(nullptr, notification_telemetry_service);
#endif  // !(!BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_ANDROID) &&
        // defined(ARCH_CPU_ARM64)))
}

}  // namespace safe_browsing
