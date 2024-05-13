// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"

#include <memory>

FacilitatedPaymentsController::FacilitatedPaymentsController() = default;
FacilitatedPaymentsController::~FacilitatedPaymentsController() = default;

bool FacilitatedPaymentsController::Show(
    std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
        view,
    content::WebContents* web_contents) {
  // Abort if facilitated payments surface is already shown.
  if (view_) {
    return false;
  }

  if (!view->RequestShowContent(web_contents)) {
    return false;
  }

  view_ = std::move(view);
  return true;
}
