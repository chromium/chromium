// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_DELEGATE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_DELEGATE_H_

#include <string>

// This delegate provides a C++ interface for the Java side of the touch to fill
// one-time tokens UI.
class TouchToFillOneTimeTokensDelegate {
 public:
  virtual ~TouchToFillOneTimeTokensDelegate() = default;

  // Called when the bottom sheet is dismissed by the user, either by accepting
  // or rejecting the token.
  virtual void OnDismissed(bool token_accepted) = 0;

  // Called when the user accepts the one-time token.
  virtual void OnTokenAccepted(const std::u16string& token) = 0;

  // Called when the user rejects the one-time token.
  virtual void OnTokenRejected() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_DELEGATE_H_
