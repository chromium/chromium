// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sharesheet/copy_to_clipboard_share_action.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {
namespace sharesheet {

CopyToClipboardShareAction::CopyToClipboardShareAction() = default;

CopyToClipboardShareAction::~CopyToClipboardShareAction() = default;

const std::u16string CopyToClipboardShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(
      IDS_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_LABEL);
}

const gfx::VectorIcon& CopyToClipboardShareAction::GetActionIcon() {
  return kCopyIcon;
}

void CopyToClipboardShareAction::LaunchAction(
    ::sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::mojom::IntentPtr intent) {
  controller_ = controller;
  // TODO(crbug.com/1244143) Add copying logic.
  controller_->CloseBubble(::sharesheet::SharesheetResult::kSuccess);
}

void CopyToClipboardShareAction::OnClosing(
    ::sharesheet::SharesheetController* controller) {
  controller_ = nullptr;
}

}  // namespace sharesheet
}  // namespace ash
