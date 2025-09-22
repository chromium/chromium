// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_MOCK_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_MOCK_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_H_

#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockTouchToFillOneTimeTokensBridge
    : public TouchToFillOneTimeTokensBridge {
 public:
  MockTouchToFillOneTimeTokensBridge();
  ~MockTouchToFillOneTimeTokensBridge() override;

  MOCK_METHOD(bool,
              Show,
              (content::WebContents*,
               TouchToFillOneTimeTokensDelegate*,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, Hide, (), (override));
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_MOCK_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_H_
