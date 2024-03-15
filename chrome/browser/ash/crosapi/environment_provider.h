// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_

#include <optional>

#include "base/time/time.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-forward.h"
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

  // Returns the default paths, such as Downloads, Documents (MyFiles) and the
  // mount point for Drive. These are provided by ash because they are part of
  // the device account, not the Lacros profile.
  virtual crosapi::mojom::DefaultPathsPtr GetDefaultPaths();

  // Returns the account used to sign into the device. May be a Gaia account or
  // a Microsoft Active Directory account.
  // Returns a `nullopt` for Guest Sessions, Managed Guest Sessions,
  // Demo Mode, and Kiosks.
  virtual std::optional<account_manager::Account> GetDeviceAccount();

  // Getter for last device policy fetch attempt timestamp.
  virtual base::Time GetLastPolicyFetchAttemptTimestamp();
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
