// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CORE_OOBE_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CORE_OOBE_H_

#include "base/values.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/version_info_updater.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_source.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

/**
 * --- CoreOobe ---
 * Holds all the logic of the underlying OOBE structure that hosts the screens.
 * The actual UI part is controlled by (CoreOobeHandler & CoreOobeView) creating
 * a separation of the browser logic and the actual renderer implementation of
 * it. It can be thought of as a 'BaseScreen' in OOBE's architecture.
 *
 *  ------------------------------------------------
 *  | Browser / Business Logic |   Renderer / UI   |
 *  ------------------------------------------------
 *  |      BaseScreen          | BaseScreenHandler | All OOBE screens follow
 *  |                          |        View       | this pattern % exceptions
 *  ------------------------------------------------
 *  |       CoreOobe           |  CoreOobeHandler  | Main OOBE structure that
 *  |                          |   CoreOobeView    | hosts the screens in the UI
 *  ------------------------------------------------
 *
 *  Since the initialization of the UI isn't instantaneous, CoreOobeView has the
 *  notion of a |CoreOobeView::UiState| and informs this class whenever it
 *  changes. The possible states are:
 *
 *   - kUninitialized -
 *   When this class is created and the UI is still executing the import
 *   dependencies in |oobe.js|. CoreOobeHandler is not allowed to send JS yet.
 *
 *   - kCoreHandlerInitialized -
 *   The HTML document has been parsed and all the JavaScript imports from
 *   |oobe.js| have been executed. This state is triggered in the first line of
 *   |oobe.js| and at this point the |CoreOobeHandler| is allowed to send JS,
 *   but limited to very basic calls.
 *
 *   - kFullyInitialized -
 *   The frontend is fully initialized and all screens have been added to the
 *   HTML document. Some calls to this class that require the full UI to be
 *   ready will be deferred until we reach this state.
 */

class CoreOobeView;
class PendingFrontendCalls;

class CoreOobe : public VersionInfoUpdater::Delegate,
                 public display::DisplayObserver,
                 public OobeConfiguration::Observer,
                 public ChromeKeyboardControllerClient::Observer {
 public:
  explicit CoreOobe(const std::string& display_type,
                    base::WeakPtr<CoreOobeView> view);
  ~CoreOobe() override;
  CoreOobe(const CoreOobe&) = delete;
  CoreOobe& operator=(const CoreOobe&) = delete;

  // Calls to these methods will be deferred until fully initialized.
  // See |CoreOobeView::UiState| for details.
  void ShowScreenWithData(const OobeScreenId& screen,
                          std::optional<base::Value::Dict> data);
  void ReloadContent();
  void ForwardCancel();

  // These methods can be called at any state of the initialization.
  // See |CoreOobeView::UiState| for details.
  void UpdateClientAreaSize(const gfx::Size& size);
  void TriggerDown();
  void ToggleSystemInfo();
  void LaunchHelpApp(int help_topic_id);

 protected:
  // VersionInfoUpdater::Delegate implementation:
  void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) override;
  void OnDeviceInfoUpdated(const std::string& bluetooth_name) override;
  void OnEnterpriseInfoUpdated(const std::string& message_text,
                               const std::string& asset_id) override {}
  void OnAdbSideloadStatusUpdated(bool enabled) override {}

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // OobeConfiguration::Observer:
  void OnOobeConfigurationChanged() override;

 private:
  // Called by the |CoreOobeHandler| to update the initialization state.
  friend class CoreOobeHandler;
  void UpdateUiInitState(CoreOobeView::UiState state);

  // Methods that need the full OOBE to be initialized will deferred until
  // the |CoreOobeHandler| notifies us with |UiState::kFullyInitialized|
  void ExecutePendingCalls();

  // If there is a pending ShowScreen call for a supported priority screen, we
  // will show it as soon as the state |UiState::kPriorityScreensLoaded| is
  // reached.
  void MaybeShowPriorityScreen();

  // Called when the display tablet state transition has completed.
  void OnTabletModeChanged(bool tablet_mode_enabled);

  class PendingFrontendCalls {
   public:
    PendingFrontendCalls();
    ~PendingFrontendCalls();

    base::OnceClosure on_tablet_mode_changed;
    base::OnceClosure on_oobe_configuration_changed;
    base::OnceClosure show_screen_with_data;
    base::OnceClosure reload_content;
    base::OnceClosure forward_cancel;
  };
  PendingFrontendCalls pending_calls_;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_{this};

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  bool is_oobe_display_ = false;

  CoreOobeView::UiState ui_init_state_ = CoreOobeView::UiState::kUninitialized;

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtr<CoreOobeView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CORE_OOBE_H_
