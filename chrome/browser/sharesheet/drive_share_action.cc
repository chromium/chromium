// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/drive_share_action.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

DriveShareAction::DriveShareAction() = default;

DriveShareAction::~DriveShareAction() = default;

const base::string16 DriveShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);
}

const gfx::ImageSkia DriveShareAction::GetActionIcon() {
  // TODO(crbug.com/1127750): Update to create the Icon at the
  // Sharesheet bubble view. Only get the VectorIcon here.
  return gfx::CreateVectorIcon(kPersonAddIcon, sharesheet::kIconSize,
                               gfx::kPlaceholderColor);
}

void DriveShareAction::LaunchAction(
    sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::mojom::IntentPtr intent) {
  DCHECK(intent->drive_share_url.has_value());
  NavigateParams params(controller->GetProfile(),
                        intent->drive_share_url.value(),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void DriveShareAction::OnClosing(sharesheet::SharesheetController* controller) {
  controller_ = nullptr;
}

bool DriveShareAction::ShouldShowAction(const apps::mojom::IntentPtr& intent,
                                        bool contains_hosted_document) {
  return intent->drive_share_url.has_value() &&
         !intent->drive_share_url->is_empty();
}
