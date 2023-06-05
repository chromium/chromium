// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

TouchToFillPasswordGenerationController::
    ~TouchToFillPasswordGenerationController() {
  HideTouchToFill();
  RemoveSuppressShowingImeCallback();
}

TouchToFillPasswordGenerationController::
    TouchToFillPasswordGenerationController(
        base::WeakPtr<password_manager::ContentPasswordManagerDriver>
            frame_driver,
        content::WebContents* web_contents,
        std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge,
        OnDismissedCallback on_dismissed_callback)
    : frame_driver_(frame_driver),
      web_contents_(web_contents),
      bridge_(std::move(bridge)),
      on_dismissed_callback_(std::move(on_dismissed_callback)) {
  CHECK(bridge_);
  CHECK(on_dismissed_callback_);
  suppress_showing_ime_callback_ = base::BindRepeating([]() {
    // This controller exists only while the TTF is being shown, so
    // always suppress the keyboard.
    return true;
  });
}

bool TouchToFillPasswordGenerationController::ShowTouchToFill(
    std::u16string generated_password,
    std::string account_display_name) {
  if (!bridge_->Show(web_contents_, base::AsWeakPtr(this),
                     std::move(generated_password),
                     std::move(account_display_name))) {
    return false;
  }

  AddSuppressShowingImeCallback();
  return true;
}

void TouchToFillPasswordGenerationController::HideTouchToFill() {
  bridge_->Hide();
}

void TouchToFillPasswordGenerationController::OnDismissed() {
  if (on_dismissed_callback_) {
    std::exchange(on_dismissed_callback_, base::NullCallback()).Run();
  }
}

void TouchToFillPasswordGenerationController::AddSuppressShowingImeCallback() {
  if (suppress_showing_ime_callback_added_) {
    return;
  }
  frame_driver_->render_frame_host()
      ->GetRenderWidgetHost()
      ->AddSuppressShowingImeCallback(suppress_showing_ime_callback_);
  suppress_showing_ime_callback_added_ = true;
}

void TouchToFillPasswordGenerationController::
    RemoveSuppressShowingImeCallback() {
  if (!suppress_showing_ime_callback_added_) {
    return;
  }
  if (frame_driver_) {
    frame_driver_->render_frame_host()
        ->GetRenderWidgetHost()
        ->RemoveSuppressShowingImeCallback(suppress_showing_ime_callback_);
  }
  suppress_showing_ime_callback_added_ = false;
}
