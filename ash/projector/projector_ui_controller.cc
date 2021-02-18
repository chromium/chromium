// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_bar_view.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kMarkedKeyIdeaToastId[] = "projector_marked_key_idea";

void ShowToast(const std::string& id, int message_id, int32_t duration_ms) {
  DCHECK(Shell::Get());
  DCHECK(Shell::Get()->toast_manager());

  ToastData toast(id, l10n_util::GetStringUTF16(message_id), duration_ms,
                  l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON));
  Shell::Get()->toast_manager()->Show(toast);
}

}  // namespace

ProjectorUiController::ProjectorUiController() = default;

ProjectorUiController::~ProjectorUiController() = default;

void ProjectorUiController::ShowToolbar() {
  if (!projector_bar_widget_) {
    // Create the toolbar.
    projector_bar_widget_ = ProjectorBarView::Create(this);
  }

  projector_bar_widget_->ShowInactive();
  model_.SetBarEnabled(true);
}

void ProjectorUiController::CloseToolbar() {
  projector_bar_widget_->Close();
  model_.SetBarEnabled(false);
}

void ProjectorUiController::ToggleToolbar() {
  if (model_.bar_enabled()) {
    CloseToolbar();
  } else {
    ShowToolbar();
  }
}

void ProjectorUiController::OnKeyIdeaMarked() {
  ShowToast(kMarkedKeyIdeaToastId, IDS_ASH_PROJECTOR_KEY_IDEA_MARKED,
            ToastData::kInfiniteDuration);
}

void ProjectorUiController::OnTranscription(const std::string& transcription,
                                            bool is_final) {}

}  // namespace ash
