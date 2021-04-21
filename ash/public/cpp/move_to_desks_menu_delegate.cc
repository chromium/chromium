// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/move_to_desks_menu_delegate.h"

#include "ash/public/cpp/desks_helper.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/move_to_desks_menu_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace {

int MapCommandIdToDeskIndex(int command_id) {
  DCHECK_GE(command_id, chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1);
  DCHECK_LE(command_id, chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_8);
  return command_id - chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1;
}

bool IsAssignToAllDesksCommand(int command_id) {
  return command_id ==
         chromeos::MoveToDesksMenuModel::TOGGLE_ASSIGN_TO_ALL_DESKS;
}

}  // namespace

namespace ash {

MoveToDesksMenuDelegate::MoveToDesksMenuDelegate(views::Widget* widget)
    : widget_(widget), desks_helper_(DesksHelper::Get()) {}

// static
bool MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu() {
  auto* desks_helper = DesksHelper::Get();
  DCHECK(desks_helper);
  return desks_helper->GetNumberOfDesks() > 1;
}

bool MoveToDesksMenuDelegate::IsCommandIdChecked(int command_id) const {
  const bool assigned_to_all_desks =
      widget_ && widget_->IsVisibleOnAllWorkspaces();
  if (IsAssignToAllDesksCommand(command_id))
    return assigned_to_all_desks;

  return !assigned_to_all_desks && MapCommandIdToDeskIndex(command_id) ==
                                       desks_helper_->GetActiveDeskIndex();
}

bool MoveToDesksMenuDelegate::IsCommandIdEnabled(int command_id) const {
  if (IsAssignToAllDesksCommand(command_id))
    return true;

  return MapCommandIdToDeskIndex(command_id) <
         desks_helper_->GetNumberOfDesks();
}

bool MoveToDesksMenuDelegate::IsCommandIdVisible(int command_id) const {
  return IsCommandIdEnabled(command_id);
}

bool MoveToDesksMenuDelegate::IsItemForCommandIdDynamic(int command_id) const {
  return chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1 <= command_id &&
         command_id <=
             chromeos::MoveToDesksMenuModel::TOGGLE_ASSIGN_TO_ALL_DESKS;
}

std::u16string MoveToDesksMenuDelegate::GetLabelForCommandId(
    int command_id) const {
  if (IsAssignToAllDesksCommand(command_id))
    return l10n_util::GetStringUTF16(IDS_ASSIGN_TO_ALL_DESKS);

  return desks_helper_->GetDeskName(MapCommandIdToDeskIndex(command_id));
}

void MoveToDesksMenuDelegate::ExecuteCommand(int command_id, int event_flags) {
  if (!widget_)
    return;

  if (IsAssignToAllDesksCommand(command_id)) {
    widget_->SetVisibleOnAllWorkspaces(!widget_->IsVisibleOnAllWorkspaces());
  } else if (desks_helper_) {
    desks_helper_->SendToDeskAtIndex(widget_->GetNativeWindow(),
                                     MapCommandIdToDeskIndex(command_id));
  }
}

}  // namespace ash
