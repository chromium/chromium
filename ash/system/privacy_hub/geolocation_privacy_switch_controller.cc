// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

GeolocationPrivacySwitchController::GeolocationPrivacySwitchController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

GeolocationPrivacySwitchController::~GeolocationPrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void GeolocationPrivacySwitchController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserGeolocationAllowed,
      base::BindRepeating(
          &GeolocationPrivacySwitchController::OnPreferenceChanged,
          base::Unretained(this)));
  // TODO(zauri): Set 0-state
}

void GeolocationPrivacySwitchController::OnPreferenceChanged() {
  // TODO(zauri): This is a stub code. Sync the state with
  // SimpleGeolocationProvider.
  const bool geolocation_state = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kUserGeolocationAllowed);
  DLOG(ERROR) << "Privacy Hub: Geolocation switch state = "
              << geolocation_state;
}

}  // namespace ash
