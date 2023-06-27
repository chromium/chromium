// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_ASH_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_ASH_H_

#include <map>
#include <vector>

#include "chrome/browser/chromeos/app_mode/kiosk_troubleshooting_controller.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

// This class registers kiosk troubleshooting accelerators to be able to handle
// them in the kiosk session. See `TroubleshootingAcceleratorAction` for
// which accelerators are currently handled.
class KioskTroubleshootingControllerAsh
    : public chromeos::KioskTroubleshootingController,
      public ui::AcceleratorTarget {
 public:
  KioskTroubleshootingControllerAsh(
      PrefService* pref_service,
      base::OnceClosure shutdown_kiosk_browser_session_callback);
  KioskTroubleshootingControllerAsh(const KioskTroubleshootingControllerAsh&) =
      delete;
  KioskTroubleshootingControllerAsh& operator=(
      const KioskTroubleshootingControllerAsh&) = delete;
  ~KioskTroubleshootingControllerAsh() override;

 private:
  enum class TroubleshootingAcceleratorAction {
    NEW_WINDOW,
    SWITCH_WINDOWS_FORWARD,
    SWITCH_WINDOWS_BACKWARD,
    SHOW_TASK_MANAGER,
    OPEN_FEEDBACK_PAGE,
    TOGGLE_OVERVIEW,
  };

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  void RegisterTroubleshootingAccelerators();

  std::vector<ui::Accelerator> GetAllAccelerators() const;

  std::map<ui::Accelerator, TroubleshootingAcceleratorAction>
      accelerators_with_actions_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_ASH_H_
