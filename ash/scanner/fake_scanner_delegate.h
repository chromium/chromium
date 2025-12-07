// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_FAKE_SCANNER_DELEGATE_H_
#define ASH_SCANNER_FAKE_SCANNER_DELEGATE_H_

#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "base/functional/callback_forward.h"

namespace ash {

// A fake ScannerDelegate which can be used in tests.
class FakeScannerDelegate : public ScannerDelegate {
 public:
  using OpenFeedbackDialogCallback = base::RepeatingCallback<
      void(const AccountId&, ScannerFeedbackInfo, SendFeedbackCallback)>;
  FakeScannerDelegate();
  FakeScannerDelegate(const FakeScannerDelegate&) = delete;
  FakeScannerDelegate& operator=(const FakeScannerDelegate&) = delete;
  ~FakeScannerDelegate() override;

  // ScannerDelegate:
  ScannerProfileScopedDelegate* GetProfileScopedDelegate() override;
  void OpenFeedbackDialog(const AccountId& account_id,
                          ScannerFeedbackInfo feedback_info,
                          SendFeedbackCallback send_feedback_callback) override;

  void SetOpenFeedbackDialogCallback(OpenFeedbackDialogCallback callback);

 private:
  FakeScannerProfileScopedDelegate fake_scanner_profile_scoped_delegate_;
  OpenFeedbackDialogCallback open_feedback_dialog_callback_;
};

}  // namespace ash

#endif  // ASH_SCANNER_FAKE_SCANNER_DELEGATE_H_
