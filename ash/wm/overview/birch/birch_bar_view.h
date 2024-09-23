// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
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

class ASH_EXPORT BirchBarView : public views::BoxLayoutView {
  METADATA_HEADER(BirchBarView, views::BoxLayoutView)

 public:
  static constexpr int kMaxChipsNum = 4;

  enum class State {
    kLoading,                    // The bar is waiting for data on creation.
    kLoadingForInformedRestore,  // The bar is waiting for data on creation for
                                 // informed restore.
    kLoadingByUser,  // The bar is waiting for data when enabled by user.
    kReloading,      // The bar is waiting for data when suggestion types are
                     // modified.
    kShuttingDown,   // The bar is shutting down when disabled.
    kNormal,         // The bar is showing chips.
  };

  enum class RelayoutReason {
    // Relayout caused by filling in new suggestions after data fetched from
    // model.
    kSetup,
    // Relayout caused by filling in new suggestions when the bar is enabled by
    // user.
    kSetupByUser,
    // Relayout caused by adding or removing chips.
    kAddRemoveChip,
    // Relayout caused by available space change.
    kAvailableSpaceChanged,
    // Relayout caused by clearing chips when the bar is disabled by user.
    kClearOnDisabled,
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

  const std::vector<raw_ptr<BirchChipButtonBase>>& chips() const {
    return chips_;
  }

  void SetState(State state);

  // Clears the items cached in the `BirchChipButtons`.
  void ShutdownChips();

  // Updates the birch bar's available space and relayout the bar according to
  // the updated available space. Note that the function must be called before
  // getting the view's preferred size.
  void UpdateAvailableSpace(int available_space);

  // Registers a relayout callback.
  void SetRelayoutCallback(RelayoutCallback callback);

  // Gets current number of chips.
  int GetChipsNum() const;

  // Clear existing chips and create new chips with given items.
  void SetupChips(const std::vector<raw_ptr<BirchItem>>& items);

  // Adds a new chip with given item.
  void AddChip(BirchItem* birch_item);

  // Removes the chip of `removed_item` and attaches a new chip with
  // `attached_item` if it's not null.
  void RemoveChip(BirchItem* removed_item, BirchItem* attached_item = nullptr);

  // Re-initializes the chip corresponding to the given `item`.
  void UpdateChip(BirchItem* item);

  // Gets the maximum height of the bar with full chips.
  int GetMaximumHeight() const;

  // Returns if there are on-going animations.
  bool IsAnimating();

 private:
  friend class OverviewGridTestApi;

  // The layouts that the birch bar may use. When current available space can
  // hold all present chips, a 1x4 grids layout is used. Otherwise, a 2x2 grids
  // layout is used.
  enum class LayoutType {
    kOneByFour,
    kTwoByTwo,
  };

  void AttachChip(std::unique_ptr<BirchChipButtonBase> chip);

  // Remove all current chips.
  void Clear();

  // Calculates the chip size according to current shelf position and display
  // size.
  gfx::Size GetChipSize(aura::Window* root_window) const;

  // Gets expected layout types according to the given number of chips and
  // current available space.
  LayoutType GetExpectedLayoutType(int chip_num) const;

  // Rearranges the chips according to current expected layout type.
  void Relayout(RelayoutReason reason);

  // Called after relayout.
  void OnRelayout(RelayoutReason reason);

  // Adds loading chips to show loading animations.
  void AddLoadingChips();

  // Adds reloading chips to show reloading animations.
  void AddReloadingChips();

  // Performs the fade-in animation of chips.
  void FadeInChips();

  // Performs fade-out animation on current chips.
  void FadeOutChips();

  // Called when fade-out animation is aborted.
  void OnFadeOutAborted();

  // Called after chips fading-in animations are done during setting up.
  void OnSetupEnded();

  // Called after chips fading-out animations are done during shutting down.
  void OnShutdownEnded();

  // Called after the removing chip fade-out animation is done.
  void OnRemovingChipFadeOutEnded(BirchChipButtonBase* removing_chip);

  // Called when remove a chip from the bar with the single row.
  void RemoveChipFromOneRowBar(BirchChipButtonBase* removing_chip);

  // Called when remove a chip from the bar with two rows.
  void RemoveChipFromTwoRowsBar(BirchChipButtonBase* removing_chip);

  // Possibly show the privacy nudge about context menu options for
  // controlling suggestion types.
  void MaybeShowPrivacyNudge();

  // Cached chip size.
  const gfx::Size chip_size_;

  // Cached available space.
  int available_space_ = 0;

  // Chips rows owned by this.
  raw_ptr<BoxLayoutView> primary_row_;
  // The secondary row only exists when it holds chips. Otherwise, there will
  // always be child spacing between the rows.
  raw_ptr<BoxLayoutView> secondary_row_ = nullptr;

  State state_ = State::kNormal;

  // The chips are owned by either primary or secondary row.
  std::vector<raw_ptr<BirchChipButtonBase>> chips_;

  // The chip which is waiting to be attached.
  std::unique_ptr<BirchChipButtonBase> chip_to_attach_;

  // Called after relayout.
  RelayoutCallback relayout_callback_;

  // Called after chips fade-out animation.
  base::OnceClosure shutdown_callback_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_VIEW_H_
