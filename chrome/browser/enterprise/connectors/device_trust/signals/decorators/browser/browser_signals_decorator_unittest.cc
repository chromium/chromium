// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include "base/callback.h"
#include "base/run_loop.h"
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
constexpr char kFakeDeviceId[] = "fake_device_id";

}  // namespace

class BrowserSignalsDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_dm_token_storage_.SetClientId(kFakeDeviceId);
    auto stub_device_info_fetcher =
        enterprise_signals::DeviceInfoFetcher::CreateStubInstanceForTesting();
    stub_device_info_fetcher_ = stub_device_info_fetcher.get();
    decorator_.emplace(&fake_dm_token_storage_, &mock_cloud_policy_store_,
                       std::move(stub_device_info_fetcher));
  }

  void SetFakeCustomerId() {
    mock_cloud_policy_store_.InitPolicyData();
    mock_cloud_policy_store_.GetMutablePolicyData()->set_obfuscated_customer_id(
        kFakeCustomerId);
  }

  void ValidateStaticSignals(const DeviceTrustSignals& signals) {
    EXPECT_EQ(signals.device_id(), kFakeDeviceId);
    EXPECT_EQ(signals.serial_number(), "twirlchange");
    EXPECT_EQ(signals.is_disk_encrypted(), false);
  }

  base::test::TaskEnvironment task_environment_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  policy::MockCloudPolicyStore mock_cloud_policy_store_;
  enterprise_signals::DeviceInfoFetcher* stub_device_info_fetcher_;
  absl::optional<BrowserSignalsDecorator> decorator_;
};

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithCustomerId) {
  SetFakeCustomerId();

  base::RunLoop run_loop;

  DeviceTrustSignals signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_EQ(kFakeCustomerId, signals.obfuscated_customer_id());
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutCustomerId) {
  base::RunLoop run_loop;

  DeviceTrustSignals signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_FALSE(signals.has_obfuscated_customer_id());
}

}  // namespace enterprise_connectors
