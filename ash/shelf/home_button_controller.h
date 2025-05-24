// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
#define ASH_SHELF_HOME_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/capture_mode/sunfish_scanner_feature_watcher.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {

class HomeButtonTapOverlay;
class HomeButton;

// Controls behavior of the HomeButton, including a possible long-press
// action (for Assistant).
// Behavior is tested indirectly in HomeButtonTest and ShelfViewInkDropTest.
class HomeButtonController : public AppListControllerObserver,
                             public display::DisplayObserver,
                             public AssistantStateObserver,
                             public AssistantUiModelObserver,
                             public SunfishScannerFeatureWatcher::Observer {
 public:
  explicit HomeButtonController(HomeButton* button);

  HomeButtonController(const HomeButtonController&) = delete;
  HomeButtonController& operator=(const HomeButtonController&) = delete;

  ~HomeButtonController() override;

  // Maybe handles a gesture event based on the event and whether the Assistant
  // is available. Returns true if the event is consumed; otherwise, HomeButton
  // should pass the event along to Button to consume.
  bool MaybeHandleGestureEvent(ui::GestureEvent* event);

  // Whether long-pressing the home button will perform an action, such as
  // opening the Assistant UI or opening a Sunfish-behavior capture session.
  bool IsLongPressActionAvailable();

  // Whether the Assistant UI currently showing.
  bool IsAssistantVisible();

 private:
  // Whether the Assistant is available via long-press.
  bool IsAssistantAvailable();

  // Whether Sunfish or Scanner's UI can be shown.
  bool IsSunfishOrScannerAvailable() const;

  // AppListControllerObserver:
  void OnAppListVisibilityWillChange(bool shown, int64_t display_id) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      assistant::AssistantAllowedState) override;
  void OnAssistantSettingsEnabled(bool enabled) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  // SunfishScannerFeatureWatcher::Observer:
  void OnSunfishScannerFeatureStatesChanged(
      SunfishScannerFeatureWatcher& source) override;

  void OnAppListShown();
  void OnAppListDismissed();

  void StartAssistantAnimation();

  // Initialize the Assistant overlay.
  void InitializeAssistantOverlay();

  // The button that owns this controller.
  const raw_ptr<HomeButton> button_;

  // Owned by the button's view hierarchy.
  raw_ptr<HomeButtonTapOverlay> tap_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> tap_animation_delay_timer_;

  // Observes changes in Sunfish and Scanner feature states.
  base::ScopedObservation<SunfishScannerFeatureWatcher,
                          SunfishScannerFeatureWatcher::Observer>
      sunfish_scanner_feature_observation_{this};

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_CONTROLLER_H_
