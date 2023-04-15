// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_delegate_impl.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FastCheckoutDelegateImplTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    fast_checkout_delegate_ = std::make_unique<FastCheckoutDelegateImpl>(
        web_contents(), &fast_checkout_client_);
  }

  testing::NiceMock<MockFastCheckoutClient> fast_checkout_client_;
  std::unique_ptr<FastCheckoutDelegateImpl> fast_checkout_delegate_;
};

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutSucceeds) {
  EXPECT_CALL(fast_checkout_client_, TryToStart)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(fast_checkout_delegate_->TryToShowFastCheckout(
      autofill::FormData(), autofill::FormFieldData(), nullptr));
}

TEST_F(FastCheckoutDelegateImplTest, IsShowingFastCheckoutUI) {
  EXPECT_CALL(fast_checkout_client_, IsShowing).WillOnce(testing::Return(true));
  EXPECT_TRUE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
}

TEST_F(FastCheckoutDelegateImplTest, HideFastCheckoutWhenShowing) {
  EXPECT_CALL(fast_checkout_client_, IsShowing).WillOnce(testing::Return(true));
  EXPECT_CALL(fast_checkout_client_, Stop(false));
  fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
}

TEST_F(FastCheckoutDelegateImplTest, HideFastCheckoutWhenNotShowing) {
  EXPECT_CALL(fast_checkout_client_, IsShowing)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(fast_checkout_client_, Stop(false)).Times(0);
  fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
}
