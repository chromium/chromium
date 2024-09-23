// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATE_FETCHER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATE_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

// Forward declarations
class PrefService;

namespace ash {
class DeviceSettingsService;
class OobeConfiguration;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace private_membership::rlwe {
class PrivateMembershipRlweClient;
}

namespace policy {

class DeviceManagementService;
class ServerBackedStateKeysBroker;

// This class asynchronously determines the enrollment state by querying state
// availability via PSM and - if state is available - requesting the enrollment
// state from the DMServer.
//
// The operation is aborted with state kNoEnrollment:
//   * we are running on a ChromeOS Flex device, or
//   * when RLZ brand code or serial number are missing, or
//   * when the embargo date is present and has not yet passed.
// All these values are retrieved using StatisticsProvider, which can be faked
// using `FakeStatisticsProvider` in tests.
//
// Additionally, the operation can be aborted with state kConnectionError:
//   * when system clock failed to synchronize, or
//   * server-backed state keys could not be retrieved.
//
// The operation will be concluded by calling `report_result` with the retrieved
// enrollment state or error. In case we retrieved state or determined lack
// thereof, additional details are stored as a dictionary under key
// `prefs::kServerBackedDeviceState` in `local_state`, which can contain entries
// with the following keys:
//  * kDeviceStateMode,
//  * kDeviceStateManagementDomain,
//  * kDeviceStateDisabledMessage,
//  * kDeviceStateLicenseType,
//  * kDeviceStatePackagedLicense,
//  * kDeviceStateAssignedUpgradeType.
//
// TODO(b/265923216): Document possible values for the dict entries above.
// TODO(b/265923216): Return the above values as part of the `report_result`
// callback. The caller will then be responsible for setting prefs or using
// these values otherwise.
class EnrollmentStateFetcher {
 public:
  using RlweClient = private_membership::rlwe::PrivateMembershipRlweClient;
  using RlweClientFactory = base::RepeatingCallback<std::unique_ptr<RlweClient>(
      private_membership::rlwe::RlweUseCase,
      const private_membership::rlwe::RlwePlaintextId&)>;
  using Factory =
      base::RepeatingCallback<std::unique_ptr<EnrollmentStateFetcher>(
          base::OnceCallback<void(AutoEnrollmentState)> report_result,
          PrefService* local_state,
          RlweClientFactory rlwe_client_factory,
          DeviceManagementService* device_management_service,
          scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
          ServerBackedStateKeysBroker* state_key_broker,
          ash::DeviceSettingsService* device_settings_service,
          ash::OobeConfiguration* oobe_configuration)>;

  // Creates an instance of EnrollmentStateFetcher.
  //
  // Does not take ownership of any passed raw pointers.
  static std::unique_ptr<EnrollmentStateFetcher> Create(
      base::OnceCallback<void(AutoEnrollmentState)> report_result,
      PrefService* local_state,
      RlweClientFactory rlwe_client_factory,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ServerBackedStateKeysBroker* state_key_broker,
      ash::DeviceSettingsService* device_settings_service,
      ash::OobeConfiguration* oobe_configuration);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual ~EnrollmentStateFetcher();

  virtual void Start() = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATE_FETCHER_H_
