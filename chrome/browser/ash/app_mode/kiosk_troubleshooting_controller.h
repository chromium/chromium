// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"

namespace chromeos {

// This class determines if the kiosk troubleshooting tools can be used and
// registers kiosk troubleshooting accelerators to be able to handle them in the
// kiosk session. See `TroubleshootingAcceleratorAction` for which accelerators
// are currently handled.
//
// `AreKioskTroubleshootingToolsEnabled` depends on the dynamically refreshed
// `ash::prefs::kKioskTroubleshootingToolsEnabled` policy.
// If the policy gets disabled during the active kiosk session,
// `KioskBrowserSession` should be shutdown to prevent active troubleshooting
// tools from being displayed.
class KioskTroubleshootingController : public ui::AcceleratorTarget {
 public:
  KioskTroubleshootingController(
      PrefService* pref_service,
      base::OnceClosure shutdown_kiosk_browser_session_callback);
  KioskTroubleshootingController(const KioskTroubleshootingController&) =
      delete;
  KioskTroubleshootingController& operator=(
      const KioskTroubleshootingController&) = delete;
  ~KioskTroubleshootingController() override;

  // Returns `false` if `ash::prefs::kKioskTroubleshootingToolsEnabled`
  // preference is not found in the pref service, otherwise returns its value.
  bool AreKioskTroubleshootingToolsEnabled() const;

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

  // This function is called once
  // `ash::prefs::kKioskTroubleshootingToolsEnabled` preference is updated.
  void PolicyChanged();

  std::map<ui::Accelerator, TroubleshootingAcceleratorAction>
      accelerators_with_actions_;
  const raw_ptr<PrefService> pref_service_;
  // Register `ash::prefs::kKioskTroubleshootingToolsEnabled` preference to
  // support dynamic refresh.
  PrefChangeRegistrar pref_change_registrar_;
  base::OnceClosure shutdown_kiosk_browser_session_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_H_
