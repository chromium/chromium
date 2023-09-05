// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace ash {

// Main orchestrator of the QuickStart flow in OOBE
class QuickStartController : public OobeUI::Observer {
 public:
  using EntryPointButtonVisibilityCallback = base::OnceCallback<void(bool)>;

  QuickStartController();

  QuickStartController(const QuickStartController&) = delete;
  QuickStartController& operator=(const QuickStartController&) = delete;

  ~QuickStartController() override;

  // Enable QuickStart even when the feature isn't enabled. This is only called
  // when enabling via the keyboard shortcut Ctrl+Alt+Q on the Welcome screen.
  void ForceEnableQuickStart();

  // Whether QuickStart is supported. Used for determining whether the entry
  // point buttons are shown.
  void IsSupported(EntryPointButtonVisibilityCallback callback);

 private:
  void InitTargetDeviceBootstrapController();

  // TODO(b:298609218) Improve the way the QuickStartScreen accesses this.
  friend class QuickStartScreen;
  quick_start::TargetDeviceBootstrapController* bootstrap_controller() {
    return bootstrap_controller_.get();
  }

  void OnGetQuickStartFeatureSupportStatus(
      EntryPointButtonVisibilityCallback callback,
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus status);

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId previous_screen,
                              OobeScreenId current_screen) override;
  void OnDestroyingOobeUI() override;

  // Activates the OobeUI::Observer
  void StartObservingScreenTransitions();

  // Whether QuickStart is supported in this system. Can only be determined
  // by explicitly probing TargetDeviceBootstrapController.
  absl::optional<bool> quickstart_supported_;

  // "Main" controller for interacting with the phone. Only valid when the
  // feature flag is enabled or if the feature was enabled via the keyboard
  // shortcut.
  base::WeakPtr<quick_start::TargetDeviceBootstrapController>
      bootstrap_controller_;

  // Source of truth of OOBE's current state via OobeUI::Observer
  absl::optional<OobeScreenId> current_screen_, previous_screen_;

  base::ScopedObservation<OobeUI, OobeUI::Observer> observation_{this};
  base::WeakPtrFactory<QuickStartController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICKSTART_CONTROLLER_H_
