// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_

#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

// The bar container to show/hide birch chips. The birch chips will
// be shown in a row with a hiding chips button at the end. When pressing the
// hiding button, the birch chips will fade out and the showing chips
// button will appear in the center.
class BirchBarView : public views::View, public BirchChipButton::Delegate {
  METADATA_HEADER(BirchBarView, views::View)

 public:
  // TODO(zxdan): When the data model is implemented, pass in the model to
  // generate birch chips.
  BirchBarView();
  BirchBarView(const BirchBarView&) = delete;
  BirchBarView& operator=(const BirchBarView&) = delete;
  ~BirchBarView() override;

  // Note: these are helper functions for test use.
  static void ShowWidgetForTesting(std::unique_ptr<BirchBarView> bar_view);
  static void HideWidgetForTesting();

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

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  // BirchChipButton::Delegate:
  void RemoveChip(BirchChipButton* chip) override;

 private:
  class BirchChipsContainer;

  void OnAnimationsEnded(bool show);
  void OnShowHideChipsButtonPressed(bool show);

  // The container of the birch chips with the hiding chips button.
  raw_ptr<BirchChipsContainer> chips_container_ = nullptr;
  // A view contains the show chips button. To sync the scaling and opacity
  // animations of the show chips button and its blurred background shield
  // (which is stacked below the button's layer during animation), we set the
  // button in this container view and animate the container instead of the
  // button.
  raw_ptr<views::View> show_chips_button_container_ = nullptr;
  // Indicating whether there is a showing/hiding animation in progress.
  bool animation_in_progress_ = false;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
