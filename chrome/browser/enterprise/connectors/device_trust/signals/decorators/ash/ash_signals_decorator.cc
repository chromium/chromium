// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/ash/ash_signals_decorator.h"

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace enterprise_connectors {

namespace {

using policy::BrowserPolicyConnectorAsh;

constexpr char kLatencyHistogramVariant[] = "Ash";

device_signals::Trigger DetermineTrigger(Profile* profile) {
  if (!profile) {
    return device_signals::Trigger::kUnspecified;
  }
  if (ash::IsUserBrowserContext(profile)) {
    return device_signals::Trigger::kBrowserNavigation;
  }
  if (ash::IsSigninBrowserContext(profile)) {
    return device_signals::Trigger::kLoginScreen;
  }

  return device_signals::Trigger::kUnspecified;
}

// Checks for the given profile if the user is affiliated or belongs to the
// sign-in profile.
bool IsSigninProfileOrBelongsToAffiliatedUser(Profile* profile) {
  if (ash::IsSigninBrowserContext(profile)) {
    return true;
  }

  if (profile->IsOffTheRecord()) {
    return false;
  }

  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (!user) {
    return false;
  }
  return user->IsAffiliated();
}

void GetNetworkDeviceStates(Profile* profile,
                            ash::NetworkStateHandler::DeviceStateList* list) {
  if (!IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    return;
  }

  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();

  network_state_handler->GetDeviceList(list);
}

std::optional<std::string> GetMacAddress(Profile* profile) {
  if (!IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    return std::nullopt;
  }

  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  if (!network) {
    return std::nullopt;
  }
  const ash::DeviceState* device =
      network_state_handler->GetDeviceState(network->device_path());
  if (!device) {
    return std::nullopt;
  }
  return ash::network_util::FormattedMacAddress(device->mac_address());
}

}  // namespace

AshSignalsDecorator::AshSignalsDecorator(
    policy::BrowserPolicyConnectorAsh* browser_policy_connector,
    Profile* profile)
    : browser_policy_connector_(browser_policy_connector),
      profile_(profile),
      attributes_(std::make_unique<policy::DeviceAttributesImpl>()) {
  DCHECK(browser_policy_connector_);
  DCHECK(profile_);
}

AshSignalsDecorator::~AshSignalsDecorator() = default;

void AshSignalsDecorator::Decorate(base::Value::Dict& signals,
                                   base::OnceClosure done_closure) {
  auto start_time = base::TimeTicks::Now();

  signals.Set(device_signals::names::kDeviceEnrollmentDomain,
              browser_policy_connector_->GetEnterpriseDomainManager());
  signals.Set(device_signals::names::kAllowScreenLock,
              profile_->GetPrefs()->GetBoolean(ash::prefs::kAllowScreenLock));
  signals.Set(device_signals::names::kSerialNumber,
              attributes_->GetDeviceSerialNumber());
  signals.Set(device_signals::names::kDeviceHostName,
              ash::NetworkHandler::Get()->network_state_handler()->hostname());
  signals.Set(device_signals::names::kTrigger,
              static_cast<int32_t>(DetermineTrigger(profile_)));
  // On ChromeOS the disk is always encrypted. See (b/249756773) for more
  // information.
  signals.Set(device_signals::names::kDiskEncrypted,
              static_cast<int32_t>(device_signals::SettingValue::ENABLED));

  // Also, there is no way to remove the need for a password when logging into a
  // device, including when the screen is locked. A password or pin is always
  // required.
  signals.Set(device_signals::names::kScreenLockSecured,
              static_cast<int32_t>(device_signals::SettingValue::ENABLED));

  base::Value::List imei_list;
  base::Value::List meid_list;
  ash::NetworkStateHandler::DeviceStateList device_list;
  GetNetworkDeviceStates(profile_, &device_list);
  for (auto* device_state : device_list) {
    if (!device_state)
      continue;

    if (!device_state->imei().empty())
      imei_list.Append(device_state->imei());

    if (!device_state->meid().empty())
      meid_list.Append(device_state->meid());
  }
  signals.Set(device_signals::names::kImei, std::move(imei_list));
  signals.Set(device_signals::names::kMeid, std::move(meid_list));

  std::vector<std::string> mac_addresses;
  std::optional<std::string> mac_address = GetMacAddress(
      g_browser_process->profile_manager()->GetPrimaryUserProfile());
  // `get_network_details()->mac_address` returns a std::string. On other
  // platforms (Windows, Linux and Mac) there can be multiple mac
  // addresses.
  if (mac_address) {
    mac_addresses.push_back(mac_address.value());
  }

  // The mac addresses signal must always have a value, which can be an empty
  // array.
  signals.Set(device_signals::names::kMacAddresses, ToListValue(mac_addresses));

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);
  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
