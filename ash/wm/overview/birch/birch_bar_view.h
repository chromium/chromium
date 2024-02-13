// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_

#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}

namespace ash {

// The birch chips bar container holds up to four birch chips. It has a
// responsive layout to adjust the chips position according to the number of
// chips present and the available space. The chips will be in a row if they can
// fit in the space. Otherwise, the chips will be in the 2x2 grids. The birch
// bar has a two levels nested box layout view:
//
// BirchBarView (2x1)
//      |
//      -----Primary Row (1xn)
//      |
//      -----Secondary Row (1xn)
//
// The BirchBarView owns the primary and secondary chips rows, which are both
// horizontal box layout views. The chips will be in the primary row, if they
// fit in the work area. Otherwise, the third and fourth chips will be moved to
// the secondary row.

class BirchBarView : public views::BoxLayoutView,
                     public BirchChipButton::Delegate {
  METADATA_HEADER(BirchBarView, views::BoxLayoutView)

 public:
  explicit BirchBarView(aura::Window* root_window);
  BirchBarView(const BirchBarView&) = delete;
  BirchBarView& operator=(const BirchBarView&) = delete;
  ~BirchBarView() override;

  // Gets the paddings for the container of the birch bar.
  gfx::Insets GetContainerPaddings() const;

  // Updates the birch bar's available space and relayout the bar according to
  // the updated available space.
  void UpdateAvailableSpace(int available_space);

  // Adds a new birch chip to the bar.
  // TODO(zxdan): move the function to private when using model and replace the
  // arguments with chip data structure.
  void AddChip(const ui::ImageModel& icon,
               const std::u16string& title,
               const std::u16string& sub_title,
               views::Button::PressedCallback callback,
               std::optional<std::u16string> button_title = std::nullopt,
               std::optional<views::Button::PressedCallback> button_callback =
                   std::nullopt);

  // BirchChipButton::Delegate:
  void RemoveChip(BirchChipButton* chip) override;

 private:
  // The layouts that the birch bar may use. When current available space can
  // hold all present chips, a 1x4 grids layout is used. Otherwise, a 2x2 grids
  // layout is used.
  enum class LayoutType {
    kOneByFour,
    kTwoByTwo,
  };

  // Calculate the chip size according to current shelf position and display
  // size.
  gfx::Size GetChipSize() const;

  // Get expected layout types according to the number of chips and available
  // space.
  LayoutType GetExpectedLayoutType() const;

  // Rearrange the chips according to current expected layout type.
  void Relayout();

  // The root window hosting the birch bar.
  raw_ptr<aura::Window> root_window_;

  // Cached chip size.
  const gfx::Size chip_size_;

  // Cached available space.
  int available_space_ = 0;

  // Chips rows owned by this.
  raw_ptr<BoxLayoutView> primary_row_;
  raw_ptr<BoxLayoutView> secondary_row_;

  // The chips are owned by either primary or secondary row.
  std::vector<raw_ptr<BirchChipButton>> chips_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
