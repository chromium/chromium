// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"

class AccountId;

namespace ash {

struct ScannerFeedbackInfo;
class ScannerProfileScopedDelegate;

// Provides access to the browser from //ash/scanner.
class ASH_PUBLIC_EXPORT ScannerDelegate {
 public:
  using SendFeedbackCallback =
      base::OnceCallback<void(ScannerFeedbackInfo feedback_info,
                              const std::string& user_description)>;

  virtual ~ScannerDelegate() = default;

  virtual ScannerProfileScopedDelegate* GetProfileScopedDelegate() = 0;

  // Opens a feedback form system dialog for the given account.
  // Calls the provided callback with the provided feedback info and the user's
  // description when the user clicks "Send". If the user does not click "Send",
  // the callback is not run.
  virtual void OpenFeedbackDialog(
      const AccountId& account_id,
      ScannerFeedbackInfo feedback_info,
      SendFeedbackCallback send_feedback_callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_
