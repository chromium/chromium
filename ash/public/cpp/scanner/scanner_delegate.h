// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct ScannerFeedbackInfo;
class ScannerProfileScopedDelegate;

// Provides access to the browser from //ash/scanner.
class ASH_PUBLIC_EXPORT ScannerDelegate {
 public:
  virtual ~ScannerDelegate() = default;

  virtual ScannerProfileScopedDelegate* GetProfileScopedDelegate() = 0;

  // Opens a feedback form system dialog for the active user profile.
  //
  // TODO: b/382562555 - Consider taking in a `context::BrowserContext*` here
  // to ensure that the dialog is opened for the correct user.
  virtual void OpenFeedbackDialog(ScannerFeedbackInfo feedback_info) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_DELEGATE_H_
