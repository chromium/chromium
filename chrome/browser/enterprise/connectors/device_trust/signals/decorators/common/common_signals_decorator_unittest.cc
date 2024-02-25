// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

namespace {

constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Common";
constexpr char kCachedLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Common.WithCache";

}  // namespace

class CommonSignalsDecoratorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  CommonSignalsDecorator decorator_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(CommonSignalsDecoratorTest, Decorate_StaticValuesPresent) {
  base::RunLoop run_loop;

  base::Value::Dict signals;
  decorator_.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_TRUE(signals.contains(device_signals::names::kOs));
  EXPECT_TRUE(signals.contains(device_signals::names::kOsVersion));
  EXPECT_TRUE(signals.contains(device_signals::names::kDeviceModel));
  EXPECT_TRUE(signals.contains(device_signals::names::kDeviceManufacturer));
  EXPECT_TRUE(signals.contains(device_signals::names::kDisplayName));
  EXPECT_TRUE(signals.contains(device_signals::names::kBrowserVersion));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCachedLatencyHistogram, 0);

  // Run a second time to exercise the caching code.
  base::RunLoop second_run_loop;
  base::Value::Dict second_signals;
  decorator_.Decorate(second_signals, second_run_loop.QuitClosure());
  second_run_loop.Run();

  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceModel),
            *second_signals.FindString(device_signals::names::kDeviceModel));
  EXPECT_EQ(
      *signals.FindString(device_signals::names::kDeviceManufacturer),
      *second_signals.FindString(device_signals::names::kDeviceManufacturer));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCachedLatencyHistogram, 1);
}

}  // namespace enterprise_connectors
