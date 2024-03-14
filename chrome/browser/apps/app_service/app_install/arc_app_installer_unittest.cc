// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/arc_app_installer.h"

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class ArcAppInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    arc_test_.SetUp(&profile_);
  }

  void TearDown() override { arc_test_.TearDown(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ArcAppTest arc_test_;
};

TEST_F(ArcAppInstallerTest, Install) {
  base::HistogramTester histograms;
  ArcAppInstaller installer(&profile_);
  AppInstallData data(PackageId::FromString("android:com.example.app").value());
  data.app_type_data.emplace<AndroidAppInstallData>();
  base::test::TestFuture<bool> result;
  installer.InstallApp(AppInstallSurface::kAppPreloadServiceOem,
                       std::move(data), result.GetCallback());
  EXPECT_EQ(1,
            arc_test_.app_instance()->start_fast_app_reinstall_request_count());
  EXPECT_TRUE(result.Get());
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.ArcAppInstaller.InstallResult",
      ArcAppInstallResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.ArcAppInstaller.InstallResult."
      "AppPreloadServiceOem",
      ArcAppInstallResult::kSuccess, 1);
}

}  // namespace apps
