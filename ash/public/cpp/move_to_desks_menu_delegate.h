// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MOVE_TO_DESKS_MENU_DELEGATE_H_
#define ASH_PUBLIC_CPP_MOVE_TO_DESKS_MENU_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace views {
class Widget;
}

namespace ash {

class DesksHelper;

// A `ui::SimpleMenuModel::Delegate` for the Move to Desks menu.
class ASH_PUBLIC_EXPORT MoveToDesksMenuDelegate
    : public ui::SimpleMenuModel::Delegate {
 public:
  MoveToDesksMenuDelegate(views::Widget* widget);
  MoveToDesksMenuDelegate(const MoveToDesksMenuDelegate&) = delete;
  MoveToDesksMenuDelegate& operator=(const MoveToDesksMenuDelegate&) = delete;
  ~MoveToDesksMenuDelegate() override = default;

  // Returns whether the move to desks menu should be shown, i.e. there are more
  // than two desks.
  static bool ShouldShowMoveToDesksMenu();

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  views::Widget* const widget_;
  DesksHelper* const desks_helper_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MOVE_TO_DESKS_MENU_DELEGATE_H_
