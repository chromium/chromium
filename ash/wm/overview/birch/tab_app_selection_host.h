// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_
#define ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_

#include <memory>

#include "ui/views/widget/widget.h"

namespace ash {
class BirchChipButtonBase;

class TabAppSelectionHost : public views::Widget {
 public:
  explicit TabAppSelectionHost(BirchChipButtonBase* coral_button);
  TabAppSelectionHost(const TabAppSelectionHost&) = delete;
  TabAppSelectionHost& operator=(const TabAppSelectionHost&) = delete;
  ~TabAppSelectionHost() override;

  static std::unique_ptr<TabAppSelectionHost> Create();

 private:
  gfx::Rect GetDesiredBoundsInScreen();

  // TODO(sammiequon): Ensure that `owner_` outlives `this`.
  const raw_ptr<BirchChipButtonBase> owner_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_HOST_H_
