// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_troubleshooting_controller.h"

#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

KioskTroubleshootingController::KioskTroubleshootingController(
    PrefService* pref_service,
    base::OnceClosure shutdown_kiosk_browser_session_callback)
    : pref_service_(pref_service),
      shutdown_kiosk_browser_session_callback_(
          std::move(shutdown_kiosk_browser_session_callback)) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kKioskTroubleshootingToolsEnabled,
      base::BindRepeating(&KioskTroubleshootingController::PolicyChanged,
                          base::Unretained(this)));
}

KioskTroubleshootingController::~KioskTroubleshootingController() = default;

bool KioskTroubleshootingController::AreKioskTroubleshootingToolsEnabled()
    const {
  return pref_service_->GetBoolean(prefs::kKioskTroubleshootingToolsEnabled);
}

void KioskTroubleshootingController::PolicyChanged() {
  // If the policy value is changed from enabled to disabled, exit the kiosk
  // session.
  if (!AreKioskTroubleshootingToolsEnabled()) {
    LOG(WARNING)
        << "Troubleshooting tools were disabled, ending kiosk session.";
    std::move(shutdown_kiosk_browser_session_callback_).Run();
  }
  // Policy is enabled now, no action needed.
}

}  // namespace chromeos
