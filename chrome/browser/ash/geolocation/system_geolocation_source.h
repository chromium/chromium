// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_
#define CHROME_BROWSER_ASH_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/scoped_observation.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

namespace device {
class GeolocationSystemPermissionManager;
}

class PrefService;
class PrefChangeRegistrar;

namespace ash {

// The SystemGeolocationSource is responsible for listening to geolocation
// permissions from the operation system and allows the
// device::GeolocationSystemPermissionManager to access it in a platform
// agnostic manner. This concrete implementation is to be used within the Ash
// browser.
class SystemGeolocationSource : public device::SystemGeolocationSource,
                                public SessionObserver {
 public:
  SystemGeolocationSource();
  ~SystemGeolocationSource() override;

  static std::unique_ptr<device::GeolocationSystemPermissionManager>
  CreateGeolocationSystemPermissionManagerOnAsh();

  // device::SystemGeolocationSource:
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override;
  void OpenSystemPermissionSetting() override;

 private:
  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void OnPrefChanged(const std::string& pref_name);

  PermissionUpdateCallback permission_update_callback_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<SessionController, SessionObserver> observer_{this};
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_H_
