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

namespace ash {

// The bar container of birch chips.
class BirchBarView : public views::BoxLayoutView,
                     public BirchChipButton::Delegate {
  METADATA_HEADER(BirchBarView, views::BoxLayoutView)

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

  // BirchChipButton::Delegate:
  void RemoveChip(BirchChipButton* chip) override;

 private:
  std::vector<raw_ptr<BirchChipButton>> chips_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
