// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/time_of_day_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::personalization_app {
namespace {

constexpr base::FilePath::CharType kTestDlcRootPath[] =
    FILE_PATH_LITERAL("/test/time_of_day");

class TimeOfDayUtilsTest : public ::testing::Test {
 protected:
  TimeOfDayUtilsTest() {
    dlcservice_client_.set_install_root_path(kTestDlcRootPath);
  }

  void EnableDlc() {
    feature_list_.Reset();
    std::vector<base::test::FeatureRef> features_to_enable =
        personalization_app::GetTimeOfDayEnabledFeatures();
    features_to_enable.emplace_back(features::kTimeOfDayDlc);
    feature_list_.InitWithFeatures(features_to_enable, {});
  }

  void DisableDlc() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        personalization_app::GetTimeOfDayEnabledFeatures(),
        {features::kTimeOfDayDlc});
  }

  base::test::TaskEnvironment task_environment_;
  FakeDlcserviceClient dlcservice_client_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TimeOfDayUtilsTest, GetAmbientVideoHtmlPathDlcEnabled) {
  EnableDlc();
  base::test::TestFuture<base::FilePath> future;
  GetAmbientVideoHtmlPath(future.GetCallback());
  EXPECT_EQ(
      future.Get(),
      base::FilePath(kTestDlcRootPath).Append(kTimeOfDayVideoHtmlSubPath));
}

TEST_F(TimeOfDayUtilsTest, GetAmbientVideoHtmlPathDlcDisabled) {
  DisableDlc();
  base::test::TestFuture<base::FilePath> future;
  GetAmbientVideoHtmlPath(future.GetCallback());
  EXPECT_EQ(future.Get(), base::FilePath(kTimeOfDayAssetsRootfsRootDir)
                              .Append(kTimeOfDayVideoHtmlSubPath));
}

TEST_F(TimeOfDayUtilsTest, GetAmbientVideoHtmlPathDlcInstallError) {
  EnableDlc();
  dlcservice_client_.set_install_error(
      "org.chromium.DlcServiceInterface.INTERNAL");
  base::test::TestFuture<base::FilePath> future;
  GetAmbientVideoHtmlPath(future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace
}  // namespace ash::personalization_app
