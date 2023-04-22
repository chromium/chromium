// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
#define ASH_SHELF_HOME_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {

class AssistantOverlay;
class HomeButton;

// Controls behavior of the HomeButton, including a possible long-press
// action (for Assistant).
// Behavior is tested indirectly in HomeButtonTest and ShelfViewInkDropTest.
class HomeButtonController : public AppListControllerObserver,
                             public TabletModeObserver,
                             public AssistantStateObserver,
                             public AssistantUiModelObserver {
 public:
  explicit HomeButtonController(HomeButton* button);

  HomeButtonController(const HomeButtonController&) = delete;
  HomeButtonController& operator=(const HomeButtonController&) = delete;

  ~HomeButtonController() override;

  // Maybe handles a gesture event based on the event and whether the Assistant
  // is available. Returns true if the event is consumed; otherwise, HomeButton
  // should pass the event along to Button to consume.
  bool MaybeHandleGestureEvent(ui::GestureEvent* event);

  // Whether the Assistant is available via long-press.
  bool IsAssistantAvailable();

  // Whether the Assistant UI currently showing.
  bool IsAssistantVisible();

 private:
  // AppListControllerObserver:
  void OnAppListVisibilityWillChange(bool shown, int64_t display_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;

  // AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      assistant::AssistantAllowedState) override;
  void OnAssistantSettingsEnabled(bool enabled) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      absl::optional<AssistantEntryPoint> entry_point,
      absl::optional<AssistantExitPoint> exit_point) override;

  void OnAppListShown();
  void OnAppListDismissed();

  void StartAssistantAnimation();

  // Initialize the Assistant overlay.
  void InitializeAssistantOverlay();

  // The button that owns this controller.
  const raw_ptr<HomeButton, ExperimentalAsh> button_;

  // Owned by the button's view hierarchy.
  raw_ptr<AssistantOverlay, ExperimentalAsh> assistant_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> assistant_animation_delay_timer_;
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
