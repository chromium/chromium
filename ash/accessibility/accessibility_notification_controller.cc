// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_notification_controller.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

const std::string kAccessibilityToastId = "AccessibilityToast";

ToastData GetToastData(AccessibilityToastType type) {
  switch (type) {
    case AccessibilityToastType::kDictationNoFocusedTextField:
      return {/*id=*/kAccessibilityToastId,
              /*catalog_name=*/ToastCatalogName::kDictationNoFocusedTextField,
              /*text=*/
              l10n_util::GetStringUTF16(
                  IDS_ASH_ACCESSIBILITY_NUDGE_DICTATION_NO_FOCUSED_TEXT_FIELD)};
    case AccessibilityToastType::kDictationMicMuted:
      return {/*id=*/kAccessibilityToastId,
              /*catalog_name=*/ToastCatalogName::kDictationMicMuted,
              /*text=*/
              l10n_util::GetStringUTF16(
                  IDS_ASH_ACCESSIBILITY_NUDGE_DICTATION_MIC_MUTED)};
    case AccessibilityToastType::kTrackpadDisabled:
      return {/*id=*/kAccessibilityToastId,
              /*catalog_name=*/ToastCatalogName::kTrackpadDisabled,
              /*text=*/
              l10n_util::GetStringUTF16(
                  IDS_ASH_ACCESSIBILITY_NUDGE_TRACKPAD_DISABLED)};
  }
}

}  // namespace

AccessibilityNotificationController::AccessibilityNotificationController() =
    default;
AccessibilityNotificationController::~AccessibilityNotificationController() =
    default;

void AccessibilityNotificationController::ShowToast(
    AccessibilityToastType type) {
  Shell::Get()->toast_manager()->Show(GetToastData(type));
  if (show_anchored_nudge_callback_for_testing_) {
    show_anchored_nudge_callback_for_testing_.Run(type);
  }
}

void AccessibilityNotificationController::AddShowToastCallbackForTesting(
    base::RepeatingCallback<void(AccessibilityToastType)> callback) {
  show_anchored_nudge_callback_for_testing_ = std::move(callback);
}

}  // namespace ash
