// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_PERSISTENT_DESKS_BAR_VIEW_H_
#define ASH_WM_DESKS_PERSISTENT_DESKS_BAR_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class PersistentDesksBarDeskButton;
class PersistentDesksBarOverviewButton;
class PersistentDesksBarVerticalDotsButton;

// A bar that resides at the top of the screen in clamshell mode when there are
// more than one desk. It includes the desk buttons that show the corresponding
// desk's name, as well as a toggle button to enter overview mode.
class ASH_EXPORT PersistentDesksBarView : public views::View {
 public:
  PersistentDesksBarView();
  PersistentDesksBarView(const PersistentDesksBarView&) = delete;
  PersistentDesksBarView& operator=(const PersistentDesksBarView&) = delete;
  ~PersistentDesksBarView() override;

  // Updates `desk_buttons_` on desk addition, removal, activation changes and
  // desk name changes. It should just include desk buttons for all of the
  // current desks with current names and keep the background of the desk button
  // for current active desk be painted.
  void RefreshDeskButtons();

 private:
  friend class DesksTestApi;

  // views::View:
  void Layout() override;
  void OnThemeChanged() override;

  // A list of buttons with the desks' name. The buttons here should have the
  // same number as the current desks, same order as well.
  std::vector<PersistentDesksBarDeskButton*> desk_buttons_;

  // A circular button which when clicked will open the context menu of the bar.
  PersistentDesksBarVerticalDotsButton* vertical_dots_button_;

  // A button at the right side of the bar which when clicked will hide the bar
  // and enter overview mode.
  PersistentDesksBarOverviewButton* overview_button_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_PERSISTENT_DESKS_BAR_VIEW_H_
