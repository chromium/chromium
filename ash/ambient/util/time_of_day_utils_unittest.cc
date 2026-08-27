// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/time_of_day_utils.h"

#include <utility>

#include "ash/ambient/metrics/ambient_metrics.h"
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

class TimeOfDayUtilsTest : public testing::Test {
 protected:
  TimeOfDayUtilsTest() {
    feature_list_.InitWithFeatures(personalization_app::GetTimeOfDayFeatures(),
                                   {});
    dlcservice_client_.set_install_root_path(kTestDlcRootPath);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  FakeDlcserviceClient dlcservice_client_;
};

TEST_F(TimeOfDayUtilsTest, GetAmbientVideoHtmlPathDlcEnabled) {
  base::test::TestFuture<base::FilePath> future;
  GetAmbientVideoHtmlPath(ambient::kAmbientVideoDlcForegroundLabel,
                          future.GetCallback());
  EXPECT_EQ(
      future.Get(),
      base::FilePath(kTestDlcRootPath).Append(kTimeOfDayVideoHtmlSubPath));
}

TEST_F(TimeOfDayUtilsTest, GetAmbientVideoHtmlPathDlcInstallError) {
  dlcservice_client_.set_install_error(dlcservice::kErrorInternal);
  base::test::TestFuture<base::FilePath> future;
  GetAmbientVideoHtmlPath(ambient::kAmbientVideoDlcForegroundLabel,
                          future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace
}  // namespace ash::personalization_app
