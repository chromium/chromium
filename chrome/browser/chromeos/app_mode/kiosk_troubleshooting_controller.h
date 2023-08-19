// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

// This class determines if the kiosk troubleshooting tools can be used.

// `AreKioskTroubleshootingToolsEnabled` depends on the dynamically refreshed
// `prefs::kKioskTroubleshootingToolsEnabled` policy.
// If the policy gets disabled during the active kiosk session,
// `KioskBrowserSession` should be shutdown to prevent active troubleshooting
// tools from being displayed.
class KioskTroubleshootingController {
 public:
  KioskTroubleshootingController(
      PrefService* pref_service,
      base::OnceClosure shutdown_kiosk_browser_session_callback);
  KioskTroubleshootingController(const KioskTroubleshootingController&) =
      delete;
  KioskTroubleshootingController& operator=(
      const KioskTroubleshootingController&) = delete;
  virtual ~KioskTroubleshootingController();

  // Returns `false` if `prefs::kKioskTroubleshootingToolsEnabled` preference is
  // not found in the pref service, otherwise returns its value.
  bool AreKioskTroubleshootingToolsEnabled() const;

 private:
  // This function is called once `prefs::kKioskTroubleshootingToolsEnabled`
  // preference is updated.
  void PolicyChanged();

  const raw_ptr<PrefService> pref_service_;
  // Register `prefs::kKioskTroubleshootingToolsEnabled` preference to support
  // dynamic refresh.
  PrefChangeRegistrar pref_change_registrar_;
  base::OnceClosure shutdown_kiosk_browser_session_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_TROUBLESHOOTING_CONTROLLER_H_
