// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
#define ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_

#include <memory>
#include <vector>

#include "ash/hud_display/hud_constants.h"
#include "ui/views/view.h"

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

 private:
  std::vector<std::unique_ptr<HUDCheckboxHandler>> checkbox_handlers_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
