// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_CORAL_CHIP_BUTTON_H_
#define ASH_WM_OVERVIEW_BIRCH_CORAL_CHIP_BUTTON_H_

#include "ash/wm/overview/birch/birch_chip_button.h"
#include "base/timer/timer.h"

namespace views {
class AnimatedImageView;
}  // namespace views

namespace ash {

class TabAppSelectionHost;

class ASH_EXPORT CoralChipButton : public BirchChipButton {
  METADATA_HEADER(CoralChipButton, BirchChipButton)

 public:
  CoralChipButton();
  CoralChipButton(const CoralChipButton&) = delete;
  CoralChipButton& operator=(const CoralChipButton&) = delete;
  ~CoralChipButton() override;

  TabAppSelectionHost* tab_app_selection_widget() {
    return tab_app_selection_widget_.get();
  }

  void OnSelectionWidgetVisibilityChanged();

  void ShutdownSelectionWidget();

  // Called when the coral selection view has an item removed, then we need to
  // update the `CoralGroupedIconImage`.
  void ReloadIcon();

  // Called during `Init()` and for a coral chip, when the title gets updated.
  // Handles the title loading animation for a coral chip.
  void UpdateTitle(const std::optional<std::string>& group_title);

  // BirchChipButton:
  void Init(BirchItem* item) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CoralBrowserTest, AsyncGroupTitle);
  FRIEND_TEST_ALL_PREFIXES(CoralBrowserTest, GroupTitleLoadingFail);

  // Callback for the coral addon button. Should only be clicked for a coral
  // item.
  void OnCoralAddonClicked();

  // Builds `title_loading_animated_image_`.
  void BuildTitleLoadingAnimation();

  // Builds `rainbow_border_animated_image_`.
  void BuildBorderAnimation();

  // Destroys `rainbow_border_animated_image_`.
  void DestroyBorderAnimation();

  // Updates the add-on chevron button's tooltip according to current selector
  // menu state and group title.
  void UpdateAddonButtonTooltip();

  raw_ptr<views::AnimatedImageView> title_loading_animated_image_ = nullptr;
  raw_ptr<views::AnimatedImageView> rainbow_border_animated_image_ = nullptr;

  base::OneShotTimer stop_border_animation_timer_;

  // The selection menu to select tabs and apps for coral launching. Created
  // once the coral add on button is clicked.
  std::unique_ptr<TabAppSelectionHost> tab_app_selection_widget_;

  raw_ptr<Button> chevron_button_ = nullptr;

  base::WeakPtrFactory<CoralChipButton> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/*no export*/, CoralChipButton, BirchChipButton)
VIEW_BUILDER_METHOD(Init, BirchItem*)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::CoralChipButton)

#endif  // ASH_WM_OVERVIEW_BIRCH_CORAL_CHIP_BUTTON_H_
