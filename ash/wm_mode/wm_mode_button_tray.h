// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODE_WM_MODE_BUTTON_TRAY_H_
#define ASH_WM_MODE_WM_MODE_BUTTON_TRAY_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
}

namespace ash {

class Shelf;
class TrayBubbleView;

// Defines a shelf tray button that is used to toggle WM Mode on and off.
class WmModeButtonTray : public TrayBackgroundView, public SessionObserver {
  METADATA_HEADER(WmModeButtonTray, TrayBackgroundView)

 public:
  explicit WmModeButtonTray(Shelf* shelf);
  WmModeButtonTray(const WmModeButtonTray&) = delete;
  WmModeButtonTray& operator=(const WmModeButtonTray&) = delete;
  ~WmModeButtonTray() override;

  // Updates the icon used on `image_view_` and the active state of this button
  // based on the given `is_wm_mode_active`.
  void UpdateButtonVisuals(bool is_wm_mode_active);

  // TrayBackgroundView:
  void OnThemeChanged() override;
  void UpdateAfterLoginStatusChange() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override {}
  // No need to override since the icon and activation state of this tray will
  // change and get updated simultaneously in `UpdateButtonVisuals()`.
  void UpdateTrayItemColor(bool is_active) override {}
  void HideBubble(const TrayBubbleView* bubble_view) override {}

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  views::ImageView* GetImageViewForTesting() { return image_view_; }

 private:
  // Updates the visibility of this tray button based on the current state of
  // the user session (i.e. whether it's blocked or not).
  void UpdateButtonVisibility();

  // The view that hosts the button icon.
  const raw_ptr<views::ImageView> image_view_;
};

}  // namespace ash

#endif  // ASH_WM_MODE_WM_MODE_BUTTON_TRAY_H_
