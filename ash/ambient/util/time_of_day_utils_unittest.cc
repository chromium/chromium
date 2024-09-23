// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/time_of_day_utils.h"

#include <tuple>
#include <utility>

#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
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
    feature_list_.InitWithFeatures(
        personalization_app::GetTimeOfDayEnabledFeatures(), {});
    dlcservice_client_.set_install_root_path(kTestDlcRootPath);
  }

  base::test::TaskEnvironment task_environment_;
  FakeDlcserviceClient dlcservice_client_;
  base::test::ScopedFeatureList feature_list_;
};

class TimeOfDayUtilsMetricsTest
    : public TimeOfDayUtilsTest,
      public testing::WithParamInterface<
          std::tuple<const char*, std::pair<const char*, DlcError>>> {};

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

TEST_P(TimeOfDayUtilsMetricsTest, GetAmbientVideoHtmlPathDlcMetrics) {
  dlcservice_client_.set_install_error(std::get<1>(GetParam()).first);
  base::test::TestFuture<base::FilePath> future;
  base::HistogramTester histogram_tester;
  GetAmbientVideoHtmlPath(std::get<0>(GetParam()), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf("Ash.AmbientMode.VideoDlcInstall.%s.Error",
                         std::get<0>(GetParam())),
      static_cast<int>(std::get<1>(GetParam()).second),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    AllDlcErrorsAndLabels,
    TimeOfDayUtilsMetricsTest,
    testing::Combine(
        testing::Values(ambient::kAmbientVideoDlcForegroundLabel,
                        ambient::kAmbientVideoDlcBackgroundLabel),
        testing::Values(
            std::make_pair(dlcservice::kErrorNone, DlcError::kNone),
            std::make_pair(dlcservice::kErrorInternal, DlcError::kInternal),
            std::make_pair(dlcservice::kErrorBusy, DlcError::kBusy),
            std::make_pair(dlcservice::kErrorNeedReboot, DlcError::kNeedReboot),
            std::make_pair(dlcservice::kErrorInvalidDlc, DlcError::kInvalidDlc),
            std::make_pair(dlcservice::kErrorAllocation, DlcError::kAllocation),
            std::make_pair(dlcservice::kErrorNoImageFound,
                           DlcError::kNoImageFound),
            std::make_pair("InvalidDlcErrorCode", DlcError::kUnknown))),
    [](const testing::TestParamInfo<TimeOfDayUtilsMetricsTest::ParamType>&
           info) {
      // gtest does not allow periods in the test case name.
      std::string dlc_error_code;
      base::RemoveChars(std::get<1>(info.param).first, ".", &dlc_error_code);
      return base::StringPrintf("%s_%s", std::get<0>(info.param),
                                dlc_error_code.c_str());
    });

}  // namespace
}  // namespace ash::personalization_app
