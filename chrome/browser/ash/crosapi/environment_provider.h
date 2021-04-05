// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_

#include <string>

#include "base/optional.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/account_manager_core/account.h"

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

  // Deprecated. Use `GetDeviceAccount` instead.
  // TODO(crbug.com/1195865): Remove this in M93.
  virtual std::string GetDeviceAccountGaiaId();

  // Returns the account used to sign into the device. May be a Gaia account or
  // a Microsoft Active Directory account.
  // Returns a `nullopt` for Guest Sessions, Managed Guest Sessions,
  // Demo Mode, and Kiosks.
  virtual base::Optional<account_manager::Account> GetDeviceAccount();

  // Getter and setter for device account policy data. Used to pass data from
  // Ash to Lacros. The format is serialized PolicyFetchResponse object. See
  // components/policy/proto/device_management_backend.proto for details.
  virtual std::string GetDeviceAccountPolicy();
  virtual void SetDeviceAccountPolicy(const std::string& policy_blob);

 private:
  // The serialized PolicyFetchResponse object corresponding to the policy of
  // device account. Used to pass the data from Ash to Lacros.
  std::string device_account_policy_blob_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
