// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_DELEGATE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"

namespace ash::shimless_rma {

// A delegate which exposes browser functionality from //chrome to the Shimless
// RMA UI.
class ShimlessRmaDelegate {
 public:
  virtual ~ShimlessRmaDelegate() = default;

  // Exits the current RMA session then restarts the Chrome session without RMA.
  virtual void ExitRmaThenRestartChrome() = 0;

  // Starts the post-boot diagnostics app.
  virtual void ShowDiagnosticsDialog() = 0;

  // Sets the AccessibilityManager profile to the active profile to enable
  // accessibility features.
  virtual void RefreshAccessibilityManagerProfile() = 0;

  // Returns a string encoded QR code image of `url`.
  virtual void GenerateQrCode(
      const std::string& url,
      base::OnceCallback<void(const std::string& qr_code_image)> callback) = 0;
};

}  // namespace ash::shimless_rma

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_DELEGATE_H_
