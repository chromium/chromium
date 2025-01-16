// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_delegate.h"

#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

namespace ash {

FakeScannerDelegate::FakeScannerDelegate() = default;

FakeScannerDelegate::~FakeScannerDelegate() = default;

ScannerProfileScopedDelegate* FakeScannerDelegate::GetProfileScopedDelegate() {
  return &fake_scanner_profile_scoped_delegate_;
}

void FakeScannerDelegate::OpenFeedbackDialog(
    ScannerFeedbackInfo feedback_info) {
  if (!open_feedback_dialog_callback_.is_null()) {
    open_feedback_dialog_callback_.Run(std::move(feedback_info));
  }
}

void FakeScannerDelegate::SetOpenFeedbackDialogCallback(
    OpenFeedbackDialogCallback callback) {
  open_feedback_dialog_callback_ = std::move(callback);
}

}  // namespace ash
