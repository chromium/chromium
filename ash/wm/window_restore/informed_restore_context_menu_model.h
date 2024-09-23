// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTEXT_MENU_MODEL_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTEXT_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// A menu model that builds the contents of the informed restore dialog settings
// context menu. Created when clicking on the informed restore dialog settings
// button.
class ASH_EXPORT InformedRestoreContextMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  // Used to identify the `views::Label` appended to the menu.
  static constexpr int kDescriptionId = -1;

  InformedRestoreContextMenuModel();
  InformedRestoreContextMenuModel(const InformedRestoreContextMenuModel&) =
      delete;
  InformedRestoreContextMenuModel& operator=(
      const InformedRestoreContextMenuModel&) = delete;
  ~InformedRestoreContextMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_CONTEXT_MENU_MODEL_H_
