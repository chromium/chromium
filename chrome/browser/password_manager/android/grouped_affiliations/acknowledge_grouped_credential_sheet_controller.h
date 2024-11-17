// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"

// Displays the UI to ask for user verification before filling credential, that
// was originally saved for a web site or app grouped with current web site. If
// the user confirms filling, the credential will be saved as an exact match for
// the current web site.
class AcknowledgeGroupedCredentialSheetController {
 public:
  AcknowledgeGroupedCredentialSheetController();
  AcknowledgeGroupedCredentialSheetController(
      base::PassKey<
          class AcknowledgeGroupedCredentialSheetControllerTestHelper>,
      std::unique_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge);
  AcknowledgeGroupedCredentialSheetController(
      base::PassKey<class AcknowledgeGroupedCredentialSheetControllerTest>,
      std::unique_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge);
  AcknowledgeGroupedCredentialSheetController(
      const AcknowledgeGroupedCredentialSheetController&) = delete;
  AcknowledgeGroupedCredentialSheetController& operator=(
      const AcknowledgeGroupedCredentialSheetController&) = delete;

  ~AcknowledgeGroupedCredentialSheetController();

  void ShowAcknowledgeSheet(std::string current_origin,
                            std::string credential_origin,
                            gfx::NativeWindow window,
                            base::OnceCallback<void(bool)> on_close_callback);

 private:
  base::OnceCallback<void(bool)> on_close_callback_ = base::NullCallback();

  std::unique_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_CONTROLLER_H_
