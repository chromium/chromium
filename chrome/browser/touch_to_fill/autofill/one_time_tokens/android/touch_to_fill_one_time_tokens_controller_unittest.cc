// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_controller.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/mock_touch_to_fill_one_time_tokens_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;

class TouchToFillOneTimeTokensControllerTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(TouchToFillOneTimeTokensControllerTest, ShowCallsBridge) {
  auto bridge = std::make_unique<MockTouchToFillOneTimeTokensBridge>();
  MockTouchToFillOneTimeTokensBridge* bridge_ptr = bridge.get();

  TouchToFillOneTimeTokensController controller(std::move(bridge));

  const std::u16string test_token = u"123456";
  EXPECT_CALL(*bridge_ptr, Show(_, _, Eq(test_token)))
      .WillOnce(testing::Return(true));

  controller.Show(web_contents(), test_token);
}
