// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_DISPLAY_H_
#define ASH_HUD_DISPLAY_HUD_DISPLAY_H_

#include "base/sequence_checker.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

enum class DisplayMode;
class GraphsContainerView;
class HUDHeaderView;
class HUDSettingsView;

// HUDDisplayView class can be used to display a system monitoring overview.
class HUDDisplayView : public views::View {
 public:
  METADATA_HEADER(HUDDisplayView);

  HUDDisplayView();
  HUDDisplayView(const HUDDisplayView&) = delete;
  HUDDisplayView& operator=(const HUDDisplayView&) = delete;

  ~HUDDisplayView() override;

  // Destroys global instance.
  static void Destroy();

  // Creates/Destroys global singleton.
  static void Toggle();

  // Called from ClientView. Responsible for moving widget when clicked outside
  // of the children.
  int NonClientHitTest(const gfx::Point& point);

  // Changes UI display mode.
  void SetDisplayMode(const DisplayMode display_mode);

  // Callback from SettingsButton.
  void OnSettingsToggle();

  // Returns true if HUD is in overlay mode.
  bool IsOverlay();

  // Changes HUD overlay flag.
  void ToggleOverlay();

 private:
  HUDHeaderView* header_view_ = nullptr;             // not owned
  GraphsContainerView* graphs_container_ = nullptr;  // not owned
  HUDSettingsView* settings_view_ = nullptr;         // not owned

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_DISPLAY_H_
