// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_
#define ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_

#include "ui/views/widget/widget.h"

namespace ash {
class BirchChipButton;

class TabAppSelectionHost : public views::Widget {
 public:
  explicit TabAppSelectionHost(BirchChipButton* coral_chip);
  TabAppSelectionHost(const TabAppSelectionHost&) = delete;
  TabAppSelectionHost& operator=(const TabAppSelectionHost&) = delete;
  ~TabAppSelectionHost() override;

  const BirchChipButton* owner_for_testing() const { return owner_; }

 private:
  gfx::Rect GetDesiredBoundsInScreen();

  const raw_ptr<BirchChipButton> owner_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_
