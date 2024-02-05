// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_DISPLAY_H_
#define ASH_HUD_DISPLAY_HUD_DISPLAY_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

enum class HUDDisplayMode;
class GraphsContainerView;
class HUDHeaderView;
class HUDSettingsView;

// HUDDisplayView class can be used to display a system monitoring overview.
class HUDDisplayView : public views::View {
  METADATA_HEADER(HUDDisplayView, views::View)

 public:
  HUDDisplayView();
  HUDDisplayView(const HUDDisplayView&) = delete;
  HUDDisplayView& operator=(const HUDDisplayView&) = delete;

  ~HUDDisplayView() override;

  // Destroys global instance.
  static void Destroy();

  // Creates/Destroys global singleton.
  static void Toggle();

  // True when HUD is shown.
  static bool ASH_EXPORT IsShown();

  // Called from ClientView. Responsible for moving widget when clicked outside
  // of the children.
  int NonClientHitTest(const gfx::Point& point);

  // Changes UI display mode.
  void SetDisplayMode(const HUDDisplayMode display_mode);

  // Callback from SettingsButton.
  void OnSettingsToggle();

  // Returns true if HUD is in overlay mode.
  bool IsOverlay();

  // Changes HUD overlay flag.
  void ToggleOverlay();

  ASH_EXPORT static HUDDisplayView* GetForTesting();
  ASH_EXPORT HUDSettingsView* GetSettingsViewForTesting();
  ASH_EXPORT void ToggleSettingsForTesting();

 private:
  raw_ptr<HUDHeaderView> header_view_ = nullptr;             // not owned
  raw_ptr<GraphsContainerView> graphs_container_ = nullptr;  // not owned
  raw_ptr<HUDSettingsView> settings_view_ = nullptr;         // not owned

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_DISPLAY_H_
