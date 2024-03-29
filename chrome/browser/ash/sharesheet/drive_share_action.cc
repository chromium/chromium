// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sharesheet/drive_share_action.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace ash {
namespace sharesheet {

DriveShareAction::DriveShareAction() {}

DriveShareAction::~DriveShareAction() = default;

::sharesheet::ShareActionType DriveShareAction::GetActionType() const {
  return ::sharesheet::ShareActionType::kDriveShare;
}

const std::u16string DriveShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);
}

const gfx::VectorIcon& DriveShareAction::GetActionIcon() {
  return kSharesheetShareWithOthersIcon;
}

void DriveShareAction::LaunchAction(
    ::sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::IntentPtr intent) {
  controller_ = controller;
  DCHECK(intent->drive_share_url.has_value());
  if (!ash::NewWindowDelegate::GetPrimary()) {
    return;
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      intent->drive_share_url.value(),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  controller_->CloseBubble(::sharesheet::SharesheetResult::kSuccess);
}

void DriveShareAction::OnClosing(
    ::sharesheet::SharesheetController* controller) {
  controller_ = nullptr;
}

bool DriveShareAction::ShouldShowAction(const apps::IntentPtr& intent,
                                        bool contains_hosted_document) {
  return intent->drive_share_url.has_value() &&
         !intent->drive_share_url->is_empty();
}

}  // namespace sharesheet
}  // namespace ash
