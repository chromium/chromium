// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
#define ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_

#include <memory>
#include <vector>

#include "ash/hud_display/ash_tracing_manager.h"
#include "ash/hud_display/hud_constants.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event_observer.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}

namespace views {
class Label;
class LabelButton;
}

namespace ash {
namespace hud_display {

class HUDCheckboxHandler;
class HUDDisplayView;

namespace {
class HUDActionButton;
}

class HUDSettingsView : public AshTracingManager::Observer,
                        public views::View,
                        public ui::EventObserver {
  METADATA_HEADER(HUDSettingsView, views::View)

 public:
  explicit HUDSettingsView(HUDDisplayView* hud_display);
  ~HUDSettingsView() override;

  HUDSettingsView(const HUDSettingsView&) = delete;
  HUDSettingsView& operator=(const HUDSettingsView&) = delete;

  // AshTracingManager::Observer
  void OnTracingStatusChange() override;

  // Shows/hides the view.
  void ToggleVisibility();

  // Creates Ui Dev Tools.
  void OnEnableUiDevToolsButtonPressed(const ui::Event& event);

  // Show or hide cursor position.
  void OnEnableCursorPositionDisplayButtonPressed(const ui::Event& event);

  // Starts tracing.
  void OnEnableTracingButtonPressed(const ui::Event& event);

  ASH_EXPORT void ToggleTracingForTesting();

  // views::View:
  void OnEvent(ui::Event* event) override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

 private:
  // Starts/Stops tracing.
  void ToggleTracing();

  // Replace "Create Ui Dev Tools" button label with "DevTools running".
  void UpdateDevToolsControlButtonLabel();

  // Switches between "Start tracing" and "Stop tracing" button labels.
  void UpdateTracingControlButton();

  std::vector<std::unique_ptr<HUDCheckboxHandler>> checkbox_handlers_;

  // Container for "Create Ui Dev Tools" button or "DevTools running" label.
  raw_ptr<views::LabelButton> ui_dev_tools_control_button_ = nullptr;

  // Conrainer for "Show cursor position: button or "Cursor Position: " label.
  raw_ptr<views::LabelButton> cursor_position_display_button_ = nullptr;

  // Switches whether `cursor_position_display_button_` is showing cursor
  // position or not.
  bool showing_cursor_position_ = false;

  raw_ptr<HUDActionButton> tracing_control_button_ = nullptr;
  raw_ptr<views::Label> tracing_status_message_ = nullptr;

  base::WeakPtrFactory<HUDSettingsView> weak_factory_{this};
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_SETTINGS_VIEW_H_
