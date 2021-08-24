// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

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
 public:
  void SetUp() override {
    fake_dm_token_storage_.SetClientId(kFakeDeviceId);
    decorator_.emplace(&fake_dm_token_storage_, &mock_cloud_policy_store_);
  }

  void SetFakeCustomerId() {
    mock_cloud_policy_store_.InitPolicyData();
    mock_cloud_policy_store_.GetMutablePolicyData()->set_obfuscated_customer_id(
        kFakeCustomerId);
  }

 protected:
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  policy::MockCloudPolicyStore mock_cloud_policy_store_;
  absl::optional<BrowserSignalsDecorator> decorator_;
};

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithCustomerId) {
  SetFakeCustomerId();

  DeviceTrustSignals signals;
  decorator_->Decorate(signals);

  EXPECT_EQ(kFakeDeviceId, signals.device_id());
  EXPECT_EQ(kFakeCustomerId, signals.obfuscated_customer_id());
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutCustomerId) {
  DeviceTrustSignals signals;
  decorator_->Decorate(signals);

  EXPECT_EQ(kFakeDeviceId, signals.device_id());
  EXPECT_FALSE(signals.has_obfuscated_customer_id());
}

}  // namespace enterprise_connectors
