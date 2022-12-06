// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_GEOLOCATION_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_GEOLOCATION_PRIVACY_SWITCH_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"

class PrefChangeRegistrar;

namespace ash {

class ASH_EXPORT GeolocationPrivacySwitchController : public SessionObserver {
 public:
  GeolocationPrivacySwitchController();
  ~GeolocationPrivacySwitchController() override;

  GeolocationPrivacySwitchController(
      const GeolocationPrivacySwitchController&) = delete;
  GeolocationPrivacySwitchController& operator=(
      const GeolocationPrivacySwitchController&) = delete;

  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  void OnPreferenceChanged();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_GEOLOCATION_PRIVACY_SWITCH_CONTROLLER_H_
