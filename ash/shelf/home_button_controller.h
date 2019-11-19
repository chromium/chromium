// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
#define ASH_SHELF_HOME_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"

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
                             public SessionObserver,
                             public TabletModeObserver,
                             public AssistantStateObserver,
                             public AssistantUiModelObserver {
 public:
  explicit HomeButtonController(HomeButton* button);
  ~HomeButtonController() override;

  // Maybe handles a gesture event based on the event and whether the Assistant
  // is available. Returns true if the event is consumed; otherwise, HomeButton
  // should pass the event along to Button to consume.
  bool MaybeHandleGestureEvent(ui::GestureEvent* event);

  // Whether the Assistant is available via long-press.
  bool IsAssistantAvailable();

  // Whether the Assistant UI currently showing.
  bool IsAssistantVisible();

  bool is_showing_app_list() const { return is_showing_app_list_; }

 private:
  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;

  // AssistantStateObserver:
  void OnAssistantStatusChanged(mojom::AssistantState state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  void OnAppListShown();
  void OnAppListDismissed();

  void StartAssistantAnimation();

  // Initialize the Assistant overlay.
  void InitializeAssistantOverlay();

  // True if the app list is currently showing for the button's display.
  // This is useful because other app_list_visible functions aren't per-display.
  bool is_showing_app_list_ = false;

  // The button that owns this controller.
  HomeButton* const button_;

  // Owned by the button's view hierarchy. Null if the Assistant is not
  // enabled.
  AssistantOverlay* assistant_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> assistant_animation_delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(HomeButtonController);
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
