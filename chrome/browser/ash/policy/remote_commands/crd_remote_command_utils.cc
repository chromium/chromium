// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

using chromeos::network_config::mojom::CrosNetworkConfig;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::kNoLimit;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::OncSource;

const ash::KioskAppManagerBase* GetKioskAppManager(
    const user_manager::UserManager& user_manager) {
  if (user_manager.IsLoggedInAsKioskApp())
    return ash::KioskAppManager::Get();
  if (user_manager.IsLoggedInAsArcKioskApp())
    return ash::ArcKioskAppManager::Get();
  if (user_manager.IsLoggedInAsWebKioskApp())
    return ash::WebKioskAppManager::Get();

  // This method should only be invoked when we know we're in a kiosk
  // environment, so one of these app managers must exist.
  NOTREACHED();
  return nullptr;
}

bool IsRunningAutoLaunchedKiosk(const user_manager::UserManager& user_manager) {
  const auto& kiosk_app_manager = CHECK_DEREF(GetKioskAppManager(user_manager));
  return kiosk_app_manager.current_app_was_auto_launched_with_zero_delay();
}

// Helper method that DVLOGs all the given networks.
void LogNetworks(const std::vector<NetworkStatePropertiesPtr>& networks,
                 const char* type) {
  CRD_DVLOG(3) << "Found " << networks.size() << " " << type << " networks:";
  for (const auto& network : networks) {
    CRD_DVLOG(3) << "   --> " << network->name << " (" << network->guid << "): "
                 << " ONC source: " << network->source
                 << ", Type: " << network->type;
  }
}

bool IsNetworkManagedByPolicy(
    const NetworkStatePropertiesPtr& network_properties) {
  return network_properties->source == OncSource::kDevicePolicy ||
         network_properties->source == OncSource::kUserPolicy;
}

template <typename Lambda>
void EraseIf(std::vector<NetworkStatePropertiesPtr>& networks,
             Lambda predicate) {
  networks.erase(std::remove_if(networks.begin(), networks.end(), predicate),
                 networks.end());
}

// Returns if the ChromeOS device is in a managed environment or not.
bool IsInManagedEnvironment(std::vector<NetworkStatePropertiesPtr> networks) {
  LogNetworks(networks, "active");

  // Filter out the unmanaged networks.
  EraseIf(networks, [](const auto& network) {
    return !IsNetworkManagedByPolicy(network);
  });

  // Filter out cellular networks, as managed cellular networks might
  // be found even at the user's home.
  EraseIf(networks, [](const auto& network) {
    return network->type == NetworkType::kCellular;
  });

  LogNetworks(networks, "managed");

  // Now if any networks remain we are in a managed environment.
  return !networks.empty();
}

void BindMojomService(mojo::Remote<CrosNetworkConfig>& network_service) {
  ash::network_config::BindToInProcessInstance(
      network_service.BindNewPipeAndPassReceiver());
}

void CloseMojomConnection(
    std::unique_ptr<mojo::Remote<CrosNetworkConfig>> network_service) {
  // By simply dropping the remote we will close the mojom connection.
  // See `CalculateIsInManagedEnvironmentAsync` where this method is used.
}

}  // namespace

base::TimeDelta GetDeviceIdleTime() {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

UserSessionType GetCurrentUserSessionType() {
  const auto& user_manager = CHECK_DEREF(user_manager::UserManager::Get());

  if (!user_manager.IsUserLoggedIn())
    return UserSessionType::NO_SESSION;

  if (user_manager.IsLoggedInAsAnyKioskApp()) {
    if (IsRunningAutoLaunchedKiosk(user_manager))
      return UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION;
    else
      return UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION;
  }

  if (user_manager.IsLoggedInAsPublicAccount())
    return UserSessionType::MANAGED_GUEST_SESSION;

  if (user_manager.IsLoggedInAsGuest())
    return UserSessionType::GUEST_SESSION;

  if (user_manager.GetActiveUser()->IsAffiliated())
    return UserSessionType::AFFILIATED_USER_SESSION;

  return UserSessionType::UNAFFILIATED_USER_SESSION;
}

bool UserSessionSupportsRemoteAccess(UserSessionType user_session) {
  // Remote access is currently only supported while no user is logged in
  // (and the device sits at the login screen).
  switch (user_session) {
    case UserSessionType::NO_SESSION:
      return true;

    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::AFFILIATED_USER_SESSION:
    case UserSessionType::MANAGED_GUEST_SESSION:
    case UserSessionType::UNAFFILIATED_USER_SESSION:
    case UserSessionType::GUEST_SESSION:
    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      return false;
  }
}

bool UserSessionSupportsRemoteSupport(UserSessionType user_session) {
  switch (user_session) {
    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
    case UserSessionType::AFFILIATED_USER_SESSION:
    case UserSessionType::MANAGED_GUEST_SESSION:
      return true;

    case UserSessionType::NO_SESSION:
    case UserSessionType::UNAFFILIATED_USER_SESSION:
    case UserSessionType::GUEST_SESSION:
    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      return false;
  }
}

const char* UserSessionTypeToString(UserSessionType value) {
#define CASE(type_)            \
  case UserSessionType::type_: \
    return #type_;

  switch (value) {
    CASE(AUTO_LAUNCHED_KIOSK_SESSION);
    CASE(MANUALLY_LAUNCHED_KIOSK_SESSION);
    CASE(NO_SESSION);
    CASE(AFFILIATED_USER_SESSION);
    CASE(UNAFFILIATED_USER_SESSION);
    CASE(MANAGED_GUEST_SESSION);
    CASE(GUEST_SESSION);
    CASE(USER_SESSION_TYPE_UNKNOWN);
  }

#undef CASE
}

const char* CrdSessionTypeToString(CrdSessionType value) {
#define CASE(type_)           \
  case CrdSessionType::type_: \
    return #type_;

  switch (value) {
    CASE(CRD_SESSION_TYPE_UNKNOWN);
    CASE(REMOTE_ACCESS_SESSION);
    CASE(REMOTE_SUPPORT_SESSION);
  }

#undef CASE
}

void CalculateIsInManagedEnvironmentAsync(
    ManagedEnvironmentResultCallback result_callback) {
  auto network_service = std::make_unique<mojo::Remote<CrosNetworkConfig>>();
  BindMojomService(*network_service);

  // Store a raw pointer as we're moving the unique pointer before using the
  // service.
  CrosNetworkConfig* network_service_ptr = network_service.get()->get();

  network_service_ptr->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll, kNoLimit),
      base::BindOnce(IsInManagedEnvironment)
          .Then(std::move(result_callback))
          // Keep the mojom connection alive until the callback is invoked.
          .Then(base::BindOnce(CloseMojomConnection,
                               std::move(network_service))));
}

}  // namespace policy
