// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class InkDropContainerView;
class Label;
class View;
}  // namespace views

namespace ash {

// The main button for the Game Dashboard, which acts as an entry point for
// features in GameDashboardMainMenuView.
//
// The button looks like:
// +-----------+-------+-----------+
// | icon_view | label | icon_view |
// +-----------+-------+-----------+
//
// There are 2 states: Default and Recording
//
// The 'Default' button indicates an idle Game Dashboard menu. Clicking on
// the button will toggle the `GameDashboardMainMenuView`, where the Recording
// Game tile is in the default state.
//
// The 'Recording' button state indicates that the game window is being
// recorded, which is shown after a user has initiated a game window recording.
// The label view is updated to show a count up timer, representing the
// duration, and "Recording" as its status. Clicking on the button will toggle
// the `GameDashboardMainMenuView`, where the Recording Game tile allows the
// user to stop the recording.
//
// The first "icon_view" always shows the gamepad icon.
// The second "icon_view" shows an up or down arrow. Calling `SetToggled()` with
// true will replace the second "icon_view" with an the up arrow. Called with
// false, it will show the down arrow.
class GameDashboardButton : public views::Button {
  METADATA_HEADER(GameDashboardButton, views::Button)

 public:
  GameDashboardButton(PressedCallback callback, float corner_radius);
  GameDashboardButton(const GameDashboardButton&) = delete;
  GameDashboardButton& operator=(const GameDashboardButton&) = delete;
  ~GameDashboardButton() override;

  bool is_recording() const { return is_recording_; }

  bool toggled() const { return toggled_; }

  // Updates the `toggled_` state of the button.
  void SetToggled(bool toggled);

  // Called when the game window recording has started.
  void OnRecordingStarted();

  // Called when the game window recording has ended.
  void OnRecordingEnded();

  // Updates `title_view_`'s text with `duration`.
  void UpdateRecordingDuration(const std::u16string& duration);

  // views::View:
  void AddedToWidget() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;
  void AddLayerToRegion(ui::Layer* new_layer,
                        views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* old_layer) override;

  // views::Button:
  void StateChanged(ButtonState old_state) override;

 private:
  friend class GameDashboardContextTestApi;

  // Updates the icon in `arrow_icon_view_`. If `toggled_` is true, it'll show
  // the up arrow, otherwise the down arrow.
  void UpdateArrowIcon();

  // Updates `is_recording_` with `is_recording`, then updates all the views.
  void UpdateRecordingState(bool is_recording);

  // Updates all the views in the button. If `is_recording_` is true, the
  // UI is updated to show the 'Recording' button, indicating that there's an
  // active video recording session. Otherwise, it will show the 'Default'
  // button. Make sure this is called after this view is added to a widget.
  void UpdateViews();

  // Sets the `title_view` and the tooltip text to `title_text`.
  void SetTitle(const std::u16string& title_text);

  // Owned by views hierarchy.
  raw_ptr<views::ImageView> gamepad_icon_view_;
  raw_ptr<views::Label> title_view_;
  raw_ptr<views::ImageView> arrow_icon_view_;
  // Ensures the ink drop is painted above the button's background.
  raw_ptr<views::InkDropContainerView> ink_drop_container_ = nullptr;

  const float container_corner_radius_;

  // If true, the game window is being recorded, otherwise false.
  bool is_recording_ = false;

  // The button toggle state.
  bool toggled_ = false;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_H_
