// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

#include <memory>

AcknowledgeGroupedCredentialSheetController::
    AcknowledgeGroupedCredentialSheetController()
    : bridge_(std::make_unique<AcknowledgeGroupedCredentialSheetBridge>()) {}

AcknowledgeGroupedCredentialSheetController::
    AcknowledgeGroupedCredentialSheetController(
        base::PassKey<
            class AcknowledgeGroupedCredentialSheetControllerTestHelper>,
        std::unique_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge)
    : bridge_(std::move(bridge)) {}

AcknowledgeGroupedCredentialSheetController::
    AcknowledgeGroupedCredentialSheetController(
        base::PassKey<class AcknowledgeGroupedCredentialSheetControllerTest>,
        std::unique_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge)
    : bridge_(std::move(bridge)) {}

AcknowledgeGroupedCredentialSheetController::
    ~AcknowledgeGroupedCredentialSheetController() = default;

void AcknowledgeGroupedCredentialSheetController::ShowAcknowledgeSheet(
    std::string current_origin,
    std::string credential_origin,
    gfx::NativeWindow window,
    base::OnceCallback<void(bool)> on_close_callback) {
  bridge_->Show(std::move(current_origin), std::move(credential_origin), window,
                std::move(on_close_callback));
}
