// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestEnabledProvider : public ash::quick_pair::BaseEnabledProvider {
 public:
  using BaseEnabledProvider::SetEnabledAndInvokeCallback;
};

}  // namespace

namespace ash {
namespace quick_pair {

class BaseEnabledProviderTest : public testing::Test {
  void SetUp() override { provider_ = std::make_unique<TestEnabledProvider>(); }

 protected:
  std::unique_ptr<TestEnabledProvider> provider_;
};

TEST_F(BaseEnabledProviderTest, IsInitiallyDisabled) {
  EXPECT_FALSE(provider_->is_enabled());
}

TEST_F(BaseEnabledProviderTest, CanSetEnabledWithoutACallback) {
  provider_->SetEnabledAndInvokeCallback(true);
  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(BaseEnabledProviderTest, CallbackInvoked) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  provider_->SetCallback(callback.Get());
  provider_->SetEnabledAndInvokeCallback(true);
}

TEST_F(BaseEnabledProviderTest, CallbackInvokedMultipleTimes) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true)).Times(2);
  EXPECT_CALL(callback, Run(false));

  provider_->SetCallback(callback.Get());
  provider_->SetEnabledAndInvokeCallback(true);
  provider_->SetEnabledAndInvokeCallback(false);
  provider_->SetEnabledAndInvokeCallback(true);
}

TEST_F(BaseEnabledProviderTest, CallbackNotInvokedForSameValue) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run).Times(0);

  provider_->SetCallback(callback.Get());
  provider_->SetEnabledAndInvokeCallback(false);
}

TEST_F(BaseEnabledProviderTest, ReplacesCallback) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  provider_->SetCallback(callback.Get());
  provider_->SetEnabledAndInvokeCallback(true);

  provider_->SetCallback(base::DoNothing());
  EXPECT_CALL(callback, Run).Times(0);
  provider_->SetEnabledAndInvokeCallback(false);
}

}  // namespace quick_pair
}  // namespace ash
