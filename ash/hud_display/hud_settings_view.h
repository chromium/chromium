// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
#define ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_

#include <memory>
#include <vector>

#include "ash/hud_display/hud_constants.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}

namespace views {
class LabelButton;
}

namespace ash {
namespace hud_display {

class HUDCheckboxHandler;
class HUDDisplayView;

class HUDSettingsView : public views::View {
 public:
  METADATA_HEADER(HUDSettingsView);

  explicit HUDSettingsView(HUDDisplayView* hud_display);
  ~HUDSettingsView() override;

  HUDSettingsView(const HUDSettingsView&) = delete;
  HUDSettingsView& operator=(const HUDSettingsView&) = delete;

  // Shows/hides the view.
  void ToggleVisibility();

  // Creates Ui Dev Tools.
  void OnEnableUiDevToolsButtonPressed(const ui::Event& event);

 private:
  // Replace "Create Ui Dev Tools" button label with "DevTools running".
  void UpdateDevToolsControlButtonLabel();

  std::vector<std::unique_ptr<HUDCheckboxHandler>> checkbox_handlers_;

  // Container for "Create Ui Dev Tools" button or "DevTools running" label.
  views::LabelButton* ui_dev_tools_control_button_ = nullptr;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
