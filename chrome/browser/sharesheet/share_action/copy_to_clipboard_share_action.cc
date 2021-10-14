// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/copy_to_clipboard_share_action.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/views/view.h"

namespace sharesheet {

CopyToClipboardShareAction::CopyToClipboardShareAction() = default;

CopyToClipboardShareAction::~CopyToClipboardShareAction() = default;

const std::u16string CopyToClipboardShareAction::GetActionName() {
  // TODO(crbug.com/1244143) Add translation string.
  return base::UTF8ToUTF16(base::StringPiece("Copy to clipboard"));
}

const gfx::VectorIcon& CopyToClipboardShareAction::GetActionIcon() {
  // TODO(crbug.com/1244143) Add actual icon.
  return kAddIcon;
}

void CopyToClipboardShareAction::LaunchAction(SharesheetController* controller,
                                              views::View* root_view,
                                              apps::mojom::IntentPtr intent) {
  controller_ = controller;
  // TODO(crbug.com/1244143) Add copying logic.
  controller_->CloseBubble(SharesheetResult::kSuccess);
}

void CopyToClipboardShareAction::OnClosing(SharesheetController* controller) {
  controller_ = nullptr;
}

}  // namespace sharesheet
