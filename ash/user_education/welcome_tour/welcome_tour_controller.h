// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/user_education/user_education_feature_controller.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/display/display_observer.h"

namespace display {
class Screen;
enum class TabletState;
}  // namespace display

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

class AccessibilityController;
class ScopedNudgePause;
class ScopedToastPause;
class SessionController;
class Shell;
class WelcomeTourAcceleratorHandler;
class WelcomeTourControllerObserver;
class WelcomeTourNotificationBlocker;
class WelcomeTourScrim;
class WelcomeTourWindowMinimizer;

// Controller responsible for the Welcome Tour feature tutorial. Note that the
// `WelcomeTourController` is owned by the `UserEducationController` and exists
// if and only if the Welcome Tour feature is enabled.
class ASH_EXPORT WelcomeTourController : public UserEducationFeatureController,
                                         public AccessibilityObserver,
                                         public SessionObserver,
                                         public ShellObserver,
                                         public display::DisplayObserver {
 public:
  WelcomeTourController();
  WelcomeTourController(const WelcomeTourController&) = delete;
  WelcomeTourController& operator=(const WelcomeTourController&) = delete;
  ~WelcomeTourController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Welcome Tour feature is enabled.
  static WelcomeTourController* Get();

  // Adds/removes the specified `observer` from being notified of events.
  void AddObserver(WelcomeTourControllerObserver* observer);
  void RemoveObserver(WelcomeTourControllerObserver* observer);

  // Returns the initial element context to be used to start the Welcome Tour.
  ui::ElementContext GetInitialElementContext() const;

  // Returns the tutorial description for the Welcome Tour.
  user_education::TutorialDescription GetTutorialDescription() const;

 private:
  // AccessibilityObserver:
  void OnAccessibilityControllerShutdown() override;
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnChromeTerminating() override;
  void OnSessionStateChanged(session_manager::SessionState) override;

  // ShellObserver:
  void OnShellDestroying() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Starts the Welcome Tour if and only if the primary user session is active.
  void MaybeStartWelcomeTour();

  // Aborts the Welcome Tour if and only if the tour is in progress.
  void MaybeAbortWelcomeTour(welcome_tour_metrics::AbortedReason reason);

  // Invoked when the Welcome Tour is started/ended. The latter is called
  // regardless of whether the tour was `completed` or aborted.
  void OnWelcomeTourStarted();
  void OnWelcomeTourEnded(bool completed, base::ElapsedTimer time_since_start);

  // Sets the current step of the tutorial, since that information is not
  // directly available.
  void SetCurrentStep(std::optional<welcome_tour_metrics::Step> step);

  // The reason the tour was aborted.
  welcome_tour_metrics::AbortedReason aborted_reason_ =
      welcome_tour_metrics::AbortedReason::kUnknown;

  // The current step of the Welcome Tour, if it is active. Tracked here because
  // it is not directly available from the tutorial.
  std::optional<welcome_tour_metrics::Step> current_step_;

  // The elapsed time since the beginning of the `current_step_`.
  base::ElapsedTimer current_step_timer_;

  // Handles accelerator actions during the Welcome Tour. Exists only while the
  // Welcome Tour is in progress.
  std::unique_ptr<WelcomeTourAcceleratorHandler> accelerator_handler_;

  // Blocks all notifications while the Welcome Tour is in progress. Any
  // notifications received during the tour will appear in the Notification
  // Center after the tour is over.
  std::unique_ptr<WelcomeTourNotificationBlocker> notification_blocker_;

  // Suppresses all nudges during the Welcome Tour. Exists only while the
  // Welcome Tour is in progress.
  std::unique_ptr<ScopedNudgePause> nudge_pause_;

  // Used to apply a scrim to the help bubble container on all root windows
  // while the Welcome Tour is in progress. Exists only while the Welcome Tour
  // is in progress.
  std::unique_ptr<WelcomeTourScrim> scrim_;

  // Suppresses all toasts during the Welcome Tour. Exists only while the
  // Welcome Tour is in progress.
  std::unique_ptr<ScopedToastPause> toast_pause_;

  // Minimizes any app windows that are visible at the start of the Welcome
  // Tour, and any that attempt to become visible during the tour. Exists only
  // while the Welcome Tour is in progress.
  std::unique_ptr<WelcomeTourWindowMinimizer> window_minimizer_;

  // The collection of observers to be notified of events.
  base::ObserverList<WelcomeTourControllerObserver> observer_list_;

  // The accessibility controller is observed only while the Welcome Tour is in
  // progress, and will trigger an abort of the tour if ChromeVox is enabled.
  base::ScopedObservation<AccessibilityController, AccessibilityObserver>
      accessibility_observation_{this};

  // Sessions are observed only until the primary user session is activated for
  // the first time at which point the Welcome Tour is started.
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  // Shell is observed only while the Welcome Tour is in progress. The Welcome
  // Tour is aborted when Shell is destroying to ensure the Welcome Tour does
  // not outlive its dependencies.
  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};

  // Display is observed only while the Welcome Tour is in progress, and will
  // trigger an abort of the tour if the device switches to tablet mode.
  base::ScopedObservation<display::Screen, display::DisplayObserver>
      display_observation_{this};

  // It is theoretically possible for the Welcome Tour tutorial to outlive
  // `this` controller during the destruction sequence.
  base::WeakPtrFactory<WelcomeTourController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_
