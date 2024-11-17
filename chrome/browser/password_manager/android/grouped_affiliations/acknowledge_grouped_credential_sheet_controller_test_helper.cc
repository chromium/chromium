// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller_test_helper.h"

#include <memory>

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

AcknowledgeGroupedCredentialSheetControllerTestHelper::MockJniDelegate::
    MockJniDelegate() = default;
AcknowledgeGroupedCredentialSheetControllerTestHelper::MockJniDelegate::
    ~MockJniDelegate() = default;

AcknowledgeGroupedCredentialSheetControllerTestHelper::
    AcknowledgeGroupedCredentialSheetControllerTestHelper() = default;

AcknowledgeGroupedCredentialSheetControllerTestHelper::
    ~AcknowledgeGroupedCredentialSheetControllerTestHelper() = default;

std::unique_ptr<AcknowledgeGroupedCredentialSheetController>
AcknowledgeGroupedCredentialSheetControllerTestHelper::CreateController() {
  auto jni_bridge = std::make_unique<
      AcknowledgeGroupedCredentialSheetControllerTestHelper::MockJniDelegate>();
  jni_bridge_ = jni_bridge.get();
  auto bridge = std::make_unique<AcknowledgeGroupedCredentialSheetBridge>(
      base::PassKey<
          class AcknowledgeGroupedCredentialSheetControllerTestHelper>(),
      std::move(jni_bridge));
  bridge_ = bridge.get();
  return std::make_unique<AcknowledgeGroupedCredentialSheetController>(
      base::PassKey<
          class AcknowledgeGroupedCredentialSheetControllerTestHelper>(),
      std::move(bridge));
}

void AcknowledgeGroupedCredentialSheetControllerTestHelper::DismissSheet(
    bool accepted) {
  bridge_->OnDismissed(jni_zero::AttachCurrentThread(), accepted);
}
