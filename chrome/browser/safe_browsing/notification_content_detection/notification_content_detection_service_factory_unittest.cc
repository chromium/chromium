// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_content_detection/notification_content_detection_service_factory.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildTestOptimizationGuideKeyedService(
    content::BrowserContext* browser_context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

}  // namespace

namespace safe_browsing {

class NotificationContentDetectionServiceFactoryTest : public testing::Test {
 protected:
  NotificationContentDetectionServiceFactoryTest() = default;
  ~NotificationContentDetectionServiceFactoryTest() override = default;
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

 private:
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
};

TEST_F(NotificationContentDetectionServiceFactoryTest,
       DisabledForAndroidArmAndLowEndDevices) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile(
      "profile",
      {
          TestingProfile::TestingFactory{
              OptimizationGuideKeyedServiceFactory::GetInstance(),
              base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
      });
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  EXPECT_EQ(nullptr,
            NotificationContentDetectionServiceFactory::GetForProfile(profile));
#elif BUILDFLAG(IS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_EQ(
        nullptr,
        NotificationContentDetectionServiceFactory::GetForProfile(profile));
  } else {
    EXPECT_NE(
        nullptr,
        NotificationContentDetectionServiceFactory::GetForProfile(profile));
  }
#else
  EXPECT_NE(nullptr,
            NotificationContentDetectionServiceFactory::GetForProfile(profile));
#endif
}

TEST_F(NotificationContentDetectionServiceFactoryTest,
       DisabledForIncognitoMode) {
  TestingProfile* profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(nullptr,
            NotificationContentDetectionServiceFactory::GetForProfile(profile));
}

TEST_F(NotificationContentDetectionServiceFactoryTest, DisabledForGuestMode) {
  Profile* profile =
      profile_manager_->CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            NotificationContentDetectionServiceFactory::GetForProfile(profile));
}

}  // namespace safe_browsing
