// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_LACROS_H_
#define CHROME_BROWSER_LACROS_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_LACROS_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

namespace device {
class GeolocationSystemPermissionManager;
}

// The SystemGeolocationSource is responsible for listening to geolocation
// permissions from the operation system and allows the
// GeolocationSystemPermissionManager to access it in a platform agnostic
// manner. This concrete implementation is to be used within lacros browser. It
// listens to permission changes in ash.
//
// Note on sequencing:
// There is a race condition as OnPrefChanged is called asynchronously from the
// Crosapi. There are two steps that need to be done during creation: 1) Bind to
// the crosapi (This will schedule an async call (A) to OnPrefChanged() with the
// initial value) 2) Set the callback (The callback must be then called with the
// init value from the crosapi).
//
// Now there are 2 options how this can go:
// 1,2,A - now during (2) we actually don't have the value to report, hence we
// need to rely on A to call the callback that we set in 2. 1,A,2 - now during
// (A) we don't have the callback set yet, hence we just save it to a member
// variable and the callback is going to be called in (2)
//
// The code can handle both situations:
// but it relies on these three tasks (1,2,A) to not run in parallel.
// We achieve this by making sure they are executed by the same sequential task
// runner.
class SystemGeolocationSourceLacros : public device::SystemGeolocationSource,
                                      public crosapi::mojom::PrefObserver {
 public:
  SystemGeolocationSourceLacros();
  ~SystemGeolocationSourceLacros() override;

  static std::unique_ptr<device::GeolocationSystemPermissionManager>
  CreateGeolocationSystemPermissionManagerOnLacros();

  // device::SystemGeolocationSource
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override;
  void OpenSystemPermissionSetting() override;

  // crosapi::mojom::PrefObserver
  // This is called from the receiver and all calls are scheduled under the
  // task_runner.
  void OnPrefChanged(base::Value value) override;

 private:
  PermissionUpdateCallback permission_update_callback_;
  device::LocationSystemPermissionStatus current_status_ =
      device::LocationSystemPermissionStatus::kNotDetermined;
  // Receives mojo messages from ash.
  std::unique_ptr<CrosapiPrefObserver> crosapi_pref_observer_;
  base::WeakPtrFactory<SystemGeolocationSourceLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_GEOLOCATION_SYSTEM_GEOLOCATION_SOURCE_LACROS_H_
