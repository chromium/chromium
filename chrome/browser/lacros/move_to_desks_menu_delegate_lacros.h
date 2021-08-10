// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_MOVE_TO_DESKS_MENU_DELEGATE_LACROS_H_
#define CHROME_BROWSER_LACROS_MOVE_TO_DESKS_MENU_DELEGATE_LACROS_H_

#include "ui/base/models/simple_menu_model.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

// Provides the SimpleMenuModel::Delegate implementation for Move window to desk
// menu.
class MoveToDesksMenuDelegateLacros : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MoveToDesksMenuDelegateLacros(views::Widget* widget);
  MoveToDesksMenuDelegateLacros(const MoveToDesksMenuDelegateLacros&) = delete;
  MoveToDesksMenuDelegateLacros& operator=(
      const MoveToDesksMenuDelegateLacros&) = delete;
  ~MoveToDesksMenuDelegateLacros() override = default;

  // Returns whether the move to desks menu should be shown, i.e. there are more
  // than two desks.
  static bool ShouldShowMoveToDesksMenu(aura::Window* window);

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // MoveToDesksMenuDelegateLacros is indirectly owned by BrowserFrame,
  // and guaranteed to be destroyed before Widget.
  views::Widget* const widget_;
};

#endif  // CHROME_BROWSER_LACROS_MOVE_TO_DESKS_MENU_DELEGATE_LACROS_H_
