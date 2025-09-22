// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_delegate.h"

namespace content {
class WebContents;
}

class TouchToFillOneTimeTokensBridge;

// Controller for the one-time token Touch-To-Fill feature.
class TouchToFillOneTimeTokensController
    : public TouchToFillOneTimeTokensDelegate {
 public:
  TouchToFillOneTimeTokensController();
  // Constructor for testing, allowing dependency injection.
  explicit TouchToFillOneTimeTokensController(
      std::unique_ptr<TouchToFillOneTimeTokensBridge> bridge);

  ~TouchToFillOneTimeTokensController() override;

  // Show the bottom sheet with the one-time token.
  void Show(content::WebContents* web_contents, const std::u16string& token);

  // TouchToFillOneTimeTokenDelegate:
  void OnDismissed(bool token_accepted) override;
  void OnTokenAccepted(const std::u16string& token) override;
  void OnTokenRejected() override;

 private:
  void Hide();
  std::unique_ptr<TouchToFillOneTimeTokensBridge> bridge_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_CONTROLLER_H_
