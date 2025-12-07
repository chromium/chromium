// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"

#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kProfileFoo[] = "foo";
constexpr char kProfileBar[] = "bar";

download::BackgroundDownloadService* GetBackgroundDownloadServiceForProfile(
    Profile* profile) {
  return BackgroundDownloadServiceFactory::GetForKey(profile->GetProfileKey());
}

}  // namespace

namespace optimization_guide {

class ChromeProfileDownloadServiceTrackerTest : public testing::Test {
 public:
  ChromeProfileDownloadServiceTrackerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ChromeProfileDownloadServiceTrackerTest(
      const ChromeProfileDownloadServiceTrackerTest&) = delete;
  ChromeProfileDownloadServiceTrackerTest& operator=(
      const ChromeProfileDownloadServiceTrackerTest&) = delete;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(ChromeProfileDownloadServiceTrackerTest, OneProfile) {
  auto service_tracker =
      std::make_unique<ChromeProfileDownloadServiceTracker>();
  auto* foo_profile = profile_manager_.CreateTestingProfile(kProfileFoo);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service_tracker->GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(foo_profile));
}

// ChromeOS does not support deletion of profiles. So, skip the tests involving
// it.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TwoProfiles DISABLED_TwoProfiles
#else
#define MAYBE_TwoProfiles TwoProfiles
#endif

TEST_F(ChromeProfileDownloadServiceTrackerTest, MAYBE_TwoProfiles) {
  auto service_tracker =
      std::make_unique<ChromeProfileDownloadServiceTracker>();
  auto* foo_profile = profile_manager_.CreateTestingProfile(kProfileFoo);
  auto* bar_profile = profile_manager_.CreateTestingProfile(kProfileBar);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service_tracker->GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(foo_profile));

  // When foo profile is deleted, the download manager should be picked from
  // bar profile.
  profile_manager_.DeleteTestingProfile(kProfileFoo);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(service_tracker->GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(bar_profile));

  // When another profile is created, its still picked from the bar profile
  profile_manager_.CreateTestingProfile(kProfileFoo);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(service_tracker->GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(bar_profile));
}

TEST_F(ChromeProfileDownloadServiceTrackerTest,
       ServiceTrackerCreatedAfterProfile) {
  auto* foo_profile = profile_manager_.CreateTestingProfile(kProfileFoo);
  base::RunLoop().RunUntilIdle();

  auto service_tracker =
      std::make_unique<ChromeProfileDownloadServiceTracker>();

  EXPECT_EQ(service_tracker->GetBackgroundDownloadService(),
            GetBackgroundDownloadServiceForProfile(foo_profile));
}

}  // namespace optimization_guide
