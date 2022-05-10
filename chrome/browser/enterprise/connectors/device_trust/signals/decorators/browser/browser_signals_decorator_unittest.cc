// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "components/device_signals/core/common/signals_constants.h"
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

  void ValidateStaticSignals(const base::Value::Dict& signals) {
    EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceId),
              kFakeDeviceId);
    EXPECT_EQ(*signals.FindString(device_signals::names::kSerialNumber),
              "twirlchange");
    EXPECT_EQ(*signals.FindBool(device_signals::names::kIsDiskEncrypted),
              false);
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

  base::Value::Dict signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);

  EXPECT_EQ(kFakeCustomerId,
            *signals.FindString(device_signals::names::kObfuscatedCustomerId));
  EXPECT_EQ(kFakeEnrollmentDomain,
            *signals.FindString(device_signals::names::kEnrollmentDomain));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCachedLatencyHistogram, 0);

  // Running a second time will exercise the caching code.
  base::RunLoop second_run_loop;
  base::Value::Dict second_signals;
  decorator_->Decorate(second_signals, second_run_loop.QuitClosure());

  second_run_loop.Run();

  EXPECT_EQ(*signals.FindString(device_signals::names::kSerialNumber),
            *second_signals.FindString(device_signals::names::kSerialNumber));
  EXPECT_EQ(*signals.FindBool(device_signals::names::kIsDiskEncrypted),
            *second_signals.FindBool(device_signals::names::kIsDiskEncrypted));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCachedLatencyHistogram, 1);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutPolicyData) {
  base::RunLoop run_loop;

  base::Value::Dict signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_FALSE(signals.contains(device_signals::names::kObfuscatedCustomerId));
  EXPECT_FALSE(signals.contains(device_signals::names::kEnrollmentDomain));
}

}  // namespace enterprise_connectors
