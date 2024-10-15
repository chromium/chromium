// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_BASE_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_BASE_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

class BirchItem;

class BirchChipButtonBase : public views::Button {
  METADATA_HEADER(BirchChipButtonBase, views::Button)

 public:
  BirchChipButtonBase();
  BirchChipButtonBase(const BirchChipButtonBase&) = delete;
  BirchChipButtonBase& operator=(const BirchChipButtonBase&) = delete;
  ~BirchChipButtonBase() override;

  // Configure the chip with given `item`.
  virtual void Init(BirchItem* item) = 0;

  // Get birch item attached to the chip.
  virtual const BirchItem* GetItem() const = 0;
  virtual BirchItem* GetItem() = 0;

  // Shut down the chip while destroying the bar view.
  virtual void Shutdown() = 0;

 protected:
  // Updates all the UI that will be affected by rounded corner change (border,
  // background, focus ring). This depends on whether the selection widget is
  // visible, which only the birch coral chip has.
  void UpdateRoundedCorners(bool selection_widget_visible);
};

BEGIN_VIEW_BUILDER(/*no export*/, BirchChipButtonBase, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::BirchChipButtonBase)

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_BASE_H_
