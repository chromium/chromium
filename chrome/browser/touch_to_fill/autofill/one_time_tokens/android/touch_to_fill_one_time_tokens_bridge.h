// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_H_

#include <string>

namespace content {
class WebContents;
}

class TouchToFillOneTimeTokensDelegate;

// The C++ side of the JNI bridge for the one-time tokens Touch-To-Fill.
class TouchToFillOneTimeTokensBridge {
 public:
  virtual ~TouchToFillOneTimeTokensBridge() = default;

  virtual bool Show(content::WebContents* web_contents,
                    TouchToFillOneTimeTokensDelegate* delegate,
                    const std::u16string& token) = 0;
  virtual void Hide() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_H_
