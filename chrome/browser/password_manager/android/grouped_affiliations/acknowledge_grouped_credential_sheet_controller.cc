// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

AcknowledgeGroupedCredentialSheetController::
    AcknowledgeGroupedCredentialSheetController() = default;

AcknowledgeGroupedCredentialSheetController::
    ~AcknowledgeGroupedCredentialSheetController() = default;

void AcknowledgeGroupedCredentialSheetController::ShowAcknowledgeSheet(
    base::OnceCallback<void(bool)> on_close_callback) {
  // TODO (crbug.com/372635361): Implement actually showing the sheet. For now
  // just run the callback as if the sheet was shown and canceled by the user.
  on_close_callback_ = std::move(on_close_callback);
  std::move(on_close_callback_).Run(false);
}
