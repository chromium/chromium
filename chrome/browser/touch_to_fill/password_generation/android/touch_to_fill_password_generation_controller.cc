// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"

#include <memory>
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

TouchToFillPasswordGenerationController::
    ~TouchToFillPasswordGenerationController() {
  RemoveSuppressShowingImeCallback();
}

TouchToFillPasswordGenerationController::
    TouchToFillPasswordGenerationController(
        base::WeakPtr<password_manager::ContentPasswordManagerDriver>
            frame_driver,
        content::WebContents* web_contents,
        std::unique_ptr<TouchToFillPasswordGenerationBridge> bridge)
    : frame_driver_(frame_driver),
      web_contents_(web_contents),
      bridge_(std::move(bridge)) {
  suppress_showing_ime_callback_ = base::BindRepeating([]() {
    // This controller exists only while the TTF is being shown, so
    // always suppress the keyboard.
    return true;
  });
}

bool TouchToFillPasswordGenerationController::ShowTouchToFill() {
  if (!bridge_->Show(web_contents_)) {
    return false;
  }

  AddSuppressShowingImeCallback();
  return true;
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
