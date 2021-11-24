// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

constexpr char kFakeCustomerId[] = "fake_obfuscated_customer_id";
constexpr char kFakeEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Browser";
constexpr char kCachedLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Browser.WithCache";

}  // namespace

class BrowserSignalsDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_dm_token_storage_.SetClientId(kFakeDeviceId);
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(
        /*should_force=*/true);
    decorator_.emplace(&fake_dm_token_storage_, &mock_cloud_policy_store_);
  }

  void TearDown() override {
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(
        /*should_force=*/false);
  }

  void SetFakePolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_obfuscated_customer_id(kFakeCustomerId);
    policy_data->set_managed_by(kFakeEnrollmentDomain);
    mock_cloud_policy_store_.set_policy_data_for_testing(
        std::move(policy_data));
  }

  void ValidateStaticSignals(const DeviceTrustSignals& signals) {
    EXPECT_EQ(signals.device_id(), kFakeDeviceId);
    EXPECT_EQ(signals.serial_number(), "twirlchange");
    EXPECT_EQ(signals.is_disk_encrypted(), false);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  policy::MockCloudPolicyStore mock_cloud_policy_store_;
  absl::optional<BrowserSignalsDecorator> decorator_;
};

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithPolicyData) {
  SetFakePolicyData();

  base::RunLoop run_loop;

  DeviceTrustSignals signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_EQ(kFakeCustomerId, signals.obfuscated_customer_id());
  EXPECT_EQ(kFakeEnrollmentDomain, signals.enrollment_domain());

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCachedLatencyHistogram, 0);

  // Running a second time will exercise the caching code.
  base::RunLoop second_run_loop;
  DeviceTrustSignals second_signals;
  decorator_->Decorate(second_signals, second_run_loop.QuitClosure());

  second_run_loop.Run();

  EXPECT_EQ(signals.has_serial_number(), second_signals.has_serial_number());
  EXPECT_EQ(signals.has_is_disk_encrypted(),
            second_signals.has_is_disk_encrypted());

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCachedLatencyHistogram, 1);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutPolicyData) {
  base::RunLoop run_loop;

  DeviceTrustSignals signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_FALSE(signals.has_obfuscated_customer_id());
  EXPECT_FALSE(signals.has_enrollment_domain());
}

}  // namespace enterprise_connectors
