// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_controller.h"

#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_bridge_impl.h"
#include "content/public/browser/web_contents.h"

TouchToFillOneTimeTokensController::TouchToFillOneTimeTokensController()
    : bridge_(std::make_unique<TouchToFillOneTimeTokensBridgeImpl>()) {}

TouchToFillOneTimeTokensController::TouchToFillOneTimeTokensController(
    std::unique_ptr<TouchToFillOneTimeTokensBridge> bridge)
    : bridge_(std::move(bridge)) {}

TouchToFillOneTimeTokensController::~TouchToFillOneTimeTokensController() =
    default;

void TouchToFillOneTimeTokensController::Show(
    content::WebContents* web_contents,
    const std::u16string& token) {
  // TODO(crbug.com/415273777): Implement logic to show the bottom sheet.
  bridge_->Show(web_contents, this, token);
}

void TouchToFillOneTimeTokensController::Hide() {
  bridge_->Hide();
}

void TouchToFillOneTimeTokensController::OnDismissed(bool token_accepted) {
  // TODO(crbug.com/415273777): Implement dismissal logic.
}

void TouchToFillOneTimeTokensController::OnTokenAccepted(
    const std::u16string& token) {
  // TODO(crbug.com/415273777): Implement token acceptance logic (e.g., fill the
  // form).
}

void TouchToFillOneTimeTokensController::OnTokenRejected() {
  // TODO(crbug.com/415273777): Implement token rejection logic.
}
