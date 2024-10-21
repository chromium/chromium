// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

AcknowledgeGroupedCredentialSheetController::
    AcknowledgeGroupedCredentialSheetController(
        std::unique_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge)
    : bridge_(std::move(bridge)) {}

AcknowledgeGroupedCredentialSheetController::
    ~AcknowledgeGroupedCredentialSheetController() = default;

void AcknowledgeGroupedCredentialSheetController::ShowAcknowledgeSheet(
    base::OnceCallback<void(bool)> on_close_callback) {
  bridge_->Show(std::move(on_close_callback));
}
