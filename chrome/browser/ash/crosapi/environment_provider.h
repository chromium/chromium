// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_

#include <string>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/values_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Deprecated. Use `GetDeviceAccount` instead.
  // TODO(crbug.com/1195865): Remove this in M93.
  virtual std::string GetDeviceAccountGaiaId();

  // Returns the account used to sign into the device. May be a Gaia account or
  // a Microsoft Active Directory account.
  // Returns a `nullopt` for Guest Sessions, Managed Guest Sessions,
  // Demo Mode, and Kiosks.
  virtual absl::optional<account_manager::Account> GetDeviceAccount();

  // Getter and setter for device account policy data. Used to pass data from
  // Ash to Lacros. The format is serialized PolicyFetchResponse object. See
  // components/policy/proto/device_management_backend.proto for details.
  virtual std::string GetDeviceAccountPolicy();
  virtual void SetDeviceAccountPolicy(const std::string& policy_blob);

  // Getter and updater of the component policy for given namespace. The policy
  // blob is serialized JSON received from the server, or parsed from the file
  // after is was validated.
  const policy::ComponentPolicyMap& GetDeviceAccountComponentPolicy();

  // Updates the component policy for given namespace. The policy value is JSON
  // received from the server, or parsed from the file after is was validated.
  void SetDeviceAccountComponentPolicy(
      policy::ComponentPolicyMap component_policy);

  // Getter and setter for last device policy fetch attempt timestamp.
  virtual base::Time GetLastPolicyFetchAttemptTimestamp();
  virtual void SetLastPolicyFetchAttemptTimestamp(const base::Time& timestamp);

 private:
  // The serialized PolicyFetchResponse object corresponding to the policy of
  // device account. Used to pass the data from Ash to Lacros.
  std::string device_account_policy_blob_;

  // The component policy to be passed to Lacros. The map value is the
  // JSON corresponding to the policy for namespace.
  policy::ComponentPolicyMap component_policy_;

  // The last timestamp at which device account policy fetch was attempted.
  base::Time last_policy_fetch_attempt_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
