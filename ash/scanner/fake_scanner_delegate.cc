// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_delegate.h"

#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "components/account_id/account_id.h"

namespace ash {

FakeScannerDelegate::FakeScannerDelegate() = default;

FakeScannerDelegate::~FakeScannerDelegate() = default;

ScannerProfileScopedDelegate* FakeScannerDelegate::GetProfileScopedDelegate() {
  return &fake_scanner_profile_scoped_delegate_;
}

void FakeScannerDelegate::OpenFeedbackDialog(
    const AccountId& account_id,
    ScannerFeedbackInfo feedback_info,
    SendFeedbackCallback send_feedback_callback) {
  if (!open_feedback_dialog_callback_.is_null()) {
    open_feedback_dialog_callback_.Run(account_id, std::move(feedback_info),
                                       std::move(send_feedback_callback));
  }
}

void FakeScannerDelegate::SetOpenFeedbackDialogCallback(
    OpenFeedbackDialogCallback callback) {
  open_feedback_dialog_callback_ = std::move(callback);
}

}  // namespace ash
