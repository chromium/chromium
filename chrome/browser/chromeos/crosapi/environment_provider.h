// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_ENVIRONMENT_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_ENVIRONMENT_PROVIDER_H_

#include <string>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace crosapi {

// Provides ash-chrome specific flags/configurations (like session type).
class EnvironmentProvider {
 public:
  EnvironmentProvider();
  EnvironmentProvider(const EnvironmentProvider&) = delete;
  EnvironmentProvider& operator=(const EnvironmentProvider&) = delete;
  virtual ~EnvironmentProvider();

  // Virtual for tests.
  virtual crosapi::mojom::SessionType GetSessionType();
  virtual crosapi::mojom::DeviceMode GetDeviceMode();

  // Returns the default paths, such as Downloads and Documents (MyFiles).
  // These are provided by ash because they are part of the device account,
  // not the Lacros profile.
  virtual crosapi::mojom::DefaultPathsPtr GetDefaultPaths();
  virtual std::string GetDeviceAccountGaiaId();
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ENVIRONMENT_PROVIDER_H_
