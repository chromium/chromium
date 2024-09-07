// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TOUCH_SELECTION_MENU_TOUCH_SELECTION_MENU_CHROMEOS_H_
#define ASH_COMPONENTS_ARC_TOUCH_SELECTION_MENU_TOUCH_SELECTION_MENU_CHROMEOS_H_

#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ui/views/touchui/touch_selection_menu_views.h"

namespace views {
class TouchSelectionMenuRunnerViews;
}

// A ChromeOS specific subclass of the the bubble menu. It provides an
// additional button for a Smart Text Selection action based on the current
// user's text selection.
class TouchSelectionMenuChromeOS : public views::TouchSelectionMenuViews {
 public:
  TouchSelectionMenuChromeOS(views::TouchSelectionMenuRunnerViews* owner,
                             base::WeakPtr<ui::TouchSelectionMenuClient> client,
                             aura::Window* context,
                             arc::mojom::TextSelectionActionPtr action);

  TouchSelectionMenuChromeOS(const TouchSelectionMenuChromeOS&) = delete;
  TouchSelectionMenuChromeOS& operator=(const TouchSelectionMenuChromeOS&) =
      delete;

  void SetActionsForTesting(
      std::vector<arc::mojom::TextSelectionActionPtr> actions);

 protected:
  // views:TouchSelectionMenuViews.
  void CreateButtons() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

 private:
  ~TouchSelectionMenuChromeOS() override;

  void ActionButtonPressed();

  arc::mojom::TextSelectionActionPtr action_;
  int64_t display_id_;
};

#endif  // ASH_COMPONENTS_ARC_TOUCH_SELECTION_MENU_TOUCH_SELECTION_MENU_CHROMEOS_H_
