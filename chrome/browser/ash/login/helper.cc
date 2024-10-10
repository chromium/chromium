// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/helper.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_broker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

bool AreRiskyPoliciesUsed(policy::DeviceLocalAccountPolicyBroker* broker) {
  const policy::PolicyMap& policy_map = broker->core()->store()->policy_map();
  for (const auto& it : policy_map) {
    const policy::PolicyDetails* policy_details =
        policy::GetChromePolicyDetails(it.first);
    if (!policy_details) {
      continue;
    }
    for (policy::RiskTag risk_tag : policy_details->risk_tags) {
      if (risk_tag == policy::RISK_TAG_WEBSITE_SHARING) {
        VLOG(1) << "Considering managed session risky because " << it.first
                << " policy was enabled by admin.";
        return true;
      }
    }
  }
  return false;
}

bool IsProxyUsed(const PrefService* local_state_prefs) {
  std::unique_ptr<ProxyConfigDictionary> proxy_config =
      ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
          ProfileHelper::Get()->GetSigninProfile()->GetPrefs(),
          local_state_prefs);
  ProxyPrefs::ProxyMode mode;
  if (!proxy_config || !proxy_config->GetMode(&mode)) {
    return false;
  }
  return mode != ProxyPrefs::MODE_DIRECT;
}

bool PolicyHasWebTrustedAuthorityCertificate(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  return policy::UserNetworkConfigurationUpdaterAsh::
      PolicyHasWebTrustedAuthorityCertificate(
          broker->core()->store()->policy_map());
}

}  // namespace

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(gfx::Insets::VH(vertical_diff / 2, horizontal_diff / 2));
  }
  return bounds;
}

int GetCurrentUserImageSize() {
  // The biggest size that the profile picture is displayed at is currently
  // 220px, used for the big preview on OOBE and Change Picture options page.
  static const int kBaseUserImageSize = 220;
  float scale_factor = display::Display::GetForcedDeviceScaleFactor();
  if (scale_factor > 1.0f)
    return static_cast<int>(scale_factor * kBaseUserImageSize);
  const float max_scale = ui::GetScaleForMaxSupportedResourceScaleFactor();
  return kBaseUserImageSize * max_scale;
}

namespace login {

NetworkStateHelper::NetworkStateHelper() {}
NetworkStateHelper::~NetworkStateHelper() {}

std::u16string NetworkStateHelper::GetCurrentNetworkName() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network =
      nsh->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }

  network = nsh->ConnectingNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }
  return std::u16string();
}

bool NetworkStateHelper::IsConnected() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(NetworkTypePattern::Default()) != nullptr;
}

bool NetworkStateHelper::IsConnectedToEthernet() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(NetworkTypePattern::Ethernet()) != nullptr;
}

bool NetworkStateHelper::IsConnecting() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectingNetworkByType(NetworkTypePattern::Default()) != nullptr;
}

void NetworkStateHelper::OnCreateConfiguration(
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback,
    const std::string& service_path,
    const std::string& guid) const {
  // Connect to the network.
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, std::move(success_callback), std::move(error_callback),
      false /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
}

content::StoragePartition* GetSigninPartition() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  SigninPartitionManager* signin_partition_manager =
      SigninPartitionManager::Factory::GetForBrowserContext(signin_profile);
  if (!signin_partition_manager->IsInSigninSession())
    return nullptr;
  return signin_partition_manager->GetCurrentStoragePartition();
}

content::StoragePartition* GetLockScreenPartition() {
  Profile* lock_screen_profile = ProfileHelper::GetLockScreenProfile();
  // TODO(http://crbug/1348126): dependency on SigninPartitionManager should be
  // refactored after we clarify when and how do we clear data from the lock
  // screen profile.
  SigninPartitionManager* partition_manager =
      SigninPartitionManager::Factory::GetForBrowserContext(
          lock_screen_profile);
  if (!partition_manager->IsInSigninSession())
    return nullptr;
  return partition_manager->GetCurrentStoragePartition();
}

network::mojom::NetworkContext* GetSigninNetworkContext() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  // Special case for unit tests. There's no LoginDisplayHost thus no
  // webview instance. See http://crbug.com/477402
  if (!signin_partition && !LoginDisplayHost::default_host())
    return ProfileHelper::GetSigninProfile()->GetURLLoaderFactory();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetURLLoaderFactoryForBrowserProcess();
}

void SaveSyncPasswordDataToProfile(const UserContext& user_context,
                                   Profile* profile) {
  DCHECK(user_context.GetSyncPasswordData().has_value());
  password_manager::PasswordReuseManager* reuse_manager =
      PasswordReuseManagerFactory::GetForProfile(profile);

  if (reuse_manager) {
    reuse_manager->SaveSyncPasswordHash(
        user_context.GetSyncPasswordData().value(),
        password_manager::metrics_util::GaiaPasswordHashChange::
            SAVED_ON_CHROME_SIGNIN);
  }
}

base::TimeDelta TimeToOnlineSignIn(base::Time last_online_signin,
                                   base::TimeDelta offline_signin_limit) {
  const base::Time now = base::DefaultClock::GetInstance()->Now();
  // Time left to the next forced online signin.
  return offline_signin_limit - (now - last_online_signin);
}

bool IsFullManagementDisclosureNeeded(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  auto* local_state = g_browser_process->local_state();
  return AreRiskyPoliciesUsed(broker) ||
         local_state->GetBoolean(::prefs::kManagedSessionUseFullLoginWarning) ||
         PolicyHasWebTrustedAuthorityCertificate(broker) ||
         IsProxyUsed(local_state);
}

void SetAuthFactorsForUser(const AccountId& user,
                           const SessionAuthFactors& auth_factors,
                           bool is_pin_disabled_by_policy,
                           LoginScreenModel* login_screen) {
  cryptohome::AuthFactorsSet available_factors;
  cryptohome::PinLockAvailability pin_available_at = std::nullopt;

  if (auth_factors.FindSmartCardFactor()) {
    available_factors.Put(cryptohome::AuthFactorType::kSmartCard);
  } else {
    auto* password_factor = auth_factors.FindAnyPasswordFactor();
    if (password_factor) {
      available_factors.Put(cryptohome::AuthFactorType::kPassword);
    }
    auto* pin_factor = auth_factors.FindPinFactor();
    if (pin_factor && !pin_factor->GetPinStatus().IsLockedFactor()) {
      // If we end up with pin as the only auth factor and it is still disabled
      // by policy, we will show the pin.
      if (!is_pin_disabled_by_policy || !password_factor) {
        available_factors.Put(cryptohome::AuthFactorType::kPin);
      }
    }
    if (pin_factor && pin_factor->GetPinStatus().IsLockedFactor()) {
      pin_available_at = pin_factor->GetPinStatus().AvailableAt();
    }
  }
  login_screen->SetAuthFactorsForUser(user, available_factors,
                                      pin_available_at);
}

}  // namespace login
}  // namespace ash
