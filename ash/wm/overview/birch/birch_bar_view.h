// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

class BirchItem;

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

class ASH_EXPORT BirchBarView : public views::BoxLayoutView,
                                public BirchChipButton::Delegate {
  METADATA_HEADER(BirchBarView, views::BoxLayoutView)

 public:
  static constexpr int kMaxChipsNum = 4;

  enum class RelayoutReason {
    // Relayout caused by adding or removing chips.
    kAddRemoveChip,
    // Relayout caused by available space change.
    kAvailableSpaceChanged,
  };

  // The callback which is called when the birch bar view relayouts due to given
  // reason.
  using RelayoutCallback = base::RepeatingCallback<void(RelayoutReason)>;

  explicit BirchBarView(aura::Window* root_window);
  BirchBarView(const BirchBarView&) = delete;
  BirchBarView& operator=(const BirchBarView&) = delete;
  ~BirchBarView() override;

  // Creates a birch bar widget for given `root_window`.
  static std::unique_ptr<views::Widget> CreateBirchBarWidget(
      aura::Window* root_window);

  // Updates the birch bar's available space and relayout the bar according to
  // the updated available space. Note that the function must be called before
  // getting the view's preferred size.
  void UpdateAvailableSpace(int available_space);

  // Registers a relayout callback.
  base::CallbackListSubscription AddRelayoutCallback(RelayoutCallback callback);

  // Gets current number of chips.
  int GetChipsNum() const;

  // Adds a new birch chip to the bar.
  // TODO(zxdan): move the function to private when using model and replace the
  // arguments with chip data structure.
  void AddChip(BirchItem* birch_item);

  // BirchChipButton::Delegate:
  void RemoveChip(BirchChipButton* chip) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BirchBarLayoutTest, ResponsiveLayout);

  // The layouts that the birch bar may use. When current available space can
  // hold all present chips, a 1x4 grids layout is used. Otherwise, a 2x2 grids
  // layout is used.
  enum class LayoutType {
    kOneByFour,
    kTwoByTwo,
  };

  // Calculates the chip size according to current shelf position and display
  // size.
  gfx::Size GetChipSize() const;

  // Gets expected layout types according to the number of chips and available
  // space.
  LayoutType GetExpectedLayoutType() const;

  // Rearranges the chips according to current expected layout type.
  void Relayout(RelayoutReason reason);

  // Called after relayout.
  void OnRelayout(RelayoutReason reason);

  // The root window hosting the birch bar.
  const raw_ptr<aura::Window> root_window_;

  // Cached chip size.
  const gfx::Size chip_size_;

  // Cached available space.
  int available_space_ = 0;

  // Chips rows owned by this.
  raw_ptr<BoxLayoutView> primary_row_;
  // The secondary row only exists when it holds chips. Otherwise, there will
  // always be child spacing between the rows.
  raw_ptr<BoxLayoutView> secondary_row_ = nullptr;

  // The chips are owned by either primary or secondary row.
  std::vector<raw_ptr<BirchChipButton>> chips_;

  base::RepeatingCallbackList<RelayoutCallback::RunType>
      relayout_callback_list_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
