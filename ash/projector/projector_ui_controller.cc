// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/projector/projector_controller.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ui/base/l10n/l10n_util.h"

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

void ProjectorUiController::ShowToolbar() {}
void ProjectorUiController::CloseToolbar() {}
void ProjectorUiController::ToggleToolbar() {}

void ProjectorUiController::OnKeyIdeaMarked() {
  ShowToast(kMarkedKeyIdeaToastId, IDS_ASH_PROJECTOR_KEY_IDEA_MARKED,
            ToastData::kInfiniteDuration);
}

void ProjectorUiController::OnTranscription(const std::string& transcription,
                                            bool is_final) {}

}  // namespace ash
