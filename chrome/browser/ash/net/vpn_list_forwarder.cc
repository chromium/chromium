// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/vpn_list_forwarder.h"

#include "ash/public/cpp/network_config_service.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/user_manager/user.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

using chromeos::network_config::mojom::VpnProvider;
using chromeos::network_config::mojom::VpnProviderPtr;
using chromeos::network_config::mojom::VpnType;

namespace mojo {

template <>
struct TypeConverter<VpnProviderPtr,
                     app_list::ArcVpnProviderManager::ArcVpnProvider*> {
  static VpnProviderPtr Convert(
      const app_list::ArcVpnProviderManager::ArcVpnProvider* input) {
    auto result = VpnProvider::New();
    result->type = VpnType::kArc;
    result->provider_id = input->package_name;
    result->provider_name = input->app_name;
    result->app_id = input->app_id;
    result->last_launch_time = input->last_launch_time;
    return result;
  }
};

template <>
struct TypeConverter<VpnProviderPtr, const extensions::Extension*> {
  static VpnProviderPtr Convert(const extensions::Extension* input) {
    auto result = VpnProvider::New();
    result->type = VpnType::kExtension;
    result->provider_id = input->id();
    result->provider_name = input->name();
    // For Extensions, the app id is the same as the provider id.
    result->app_id = input->id();
    return result;
  }
};

}  // namespace mojo

namespace {

bool IsVPNProvider(const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kVpnProvider);
}

Profile* GetProfileForPrimaryUser() {
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;

  return ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
}

}  // namespace

VpnListForwarder::VpnListForwarder() {
  if (user_manager::UserManager::Get()->GetPrimaryUser()) {
    // If a user is logged in, start observing the primary user's extension
    // registry immediately.
    AttachToPrimaryUserProfile();
  } else {
    // If no user is logged in, wait until the first user logs in (thus becoming
    // the primary user).
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
  }

  cros_network_config_ = std::make_unique<
      mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>>();
  ash::GetNetworkConfigService(
      cros_network_config_->BindNewPipeAndPassReceiver());
}

VpnListForwarder::~VpnListForwarder() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
  if (arc_vpn_provider_manager_)
    arc_vpn_provider_manager_->RemoveObserver(this);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->vpn_extension_observer_ash()
      ->SetDelegate(nullptr);
}

void VpnListForwarder::OnArcVpnProvidersRefreshed(
    const std::vector<
        std::unique_ptr<app_list::ArcVpnProviderManager::ArcVpnProvider>>&
        arc_vpn_providers) {
  for (const auto& arc_vpn_provider : arc_vpn_providers) {
    vpn_providers_[arc_vpn_provider->package_name] =
        VpnProvider::From(arc_vpn_provider.get());
  }
  SetVpnProviders();
}

void VpnListForwarder::OnArcVpnProviderUpdated(
    app_list::ArcVpnProviderManager::ArcVpnProvider* arc_vpn_provider) {
  vpn_providers_[arc_vpn_provider->package_name] =
      VpnProvider::From(arc_vpn_provider);
  SetVpnProviders();
}

void VpnListForwarder::OnArcVpnProviderRemoved(
    const std::string& package_name) {
  vpn_providers_.erase(package_name);
  SetVpnProviders();
}

void VpnListForwarder::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!IsVPNProvider(extension))
    return;
  vpn_providers_[extension->id()] = VpnProvider::From(extension);
  SetVpnProviders();
}

void VpnListForwarder::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!IsVPNProvider(extension))
    return;
  vpn_providers_.erase(extension->id());
  SetVpnProviders();
}

void VpnListForwarder::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK(extension_registry_);
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
}

void VpnListForwarder::OnLacrosVpnExtensionLoaded(
    const std::string& extension_id,
    const std::string& extension_name) {
  auto vpn_provider = VpnProvider::New(
      /*type=*/VpnType::kExtension,
      /*provider_id=*/extension_id,
      /*provider_name=*/extension_name,
      /*app_id=*/extension_id,
      /*last_launch_time=*/base::Time());
  lacros_vpn_providers_[extension_id] = std::move(vpn_provider);
  SetVpnProviders();
}

void VpnListForwarder::OnLacrosVpnExtensionUnloaded(
    const std::string& extension_id) {
  lacros_vpn_providers_.erase(extension_id);
  SetVpnProviders();
}

void VpnListForwarder::OnLacrosVpnExtensionObserverDisconnected() {
  lacros_vpn_providers_.clear();
  SetVpnProviders();
}

void VpnListForwarder::ActiveUserChanged(user_manager::User* active_user) {
  DCHECK_EQ(user_manager::UserManager::Get()->GetPrimaryUser(), active_user);
  active_user->AddProfileCreatedObserver(
      base::BindOnce(&VpnListForwarder::AttachToPrimaryUserProfile,
                     weak_factory_.GetWeakPtr()));
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

void VpnListForwarder::SetVpnProviders() {
  std::vector<VpnProviderPtr> config_providers;
  config_providers.reserve(vpn_providers_.size() +
                           lacros_vpn_providers_.size());

  for (const auto& [id, vpn_provider] : vpn_providers_) {
    config_providers.push_back(vpn_provider->Clone());
  }
  for (const auto& [id, lacros_vpn_provider] : lacros_vpn_providers_) {
    config_providers.push_back(lacros_vpn_provider->Clone());
  }

  (*cros_network_config_)->SetVpnProviders(std::move(config_providers));
}

void VpnListForwarder::AttachToPrimaryUserProfile() {
  AttachToPrimaryUserExtensionRegistry();
  AttachToPrimaryUserArcVpnProviderManager();
  AttachToVpnExtensionObserverAsh();
}

void VpnListForwarder::AttachToPrimaryUserExtensionRegistry() {
  DCHECK(!extension_registry_);
  extension_registry_ =
      extensions::ExtensionRegistry::Get(GetProfileForPrimaryUser());
  extension_registry_->AddObserver(this);

  for (const auto& extension : extension_registry_->enabled_extensions()) {
    if (!IsVPNProvider(extension.get()))
      continue;
    vpn_providers_[extension->id()] = VpnProvider::From(extension.get());
  }
  SetVpnProviders();
}

void VpnListForwarder::AttachToPrimaryUserArcVpnProviderManager() {
  arc_vpn_provider_manager_ =
      app_list::ArcVpnProviderManager::Get(GetProfileForPrimaryUser());

  if (arc_vpn_provider_manager_)
    arc_vpn_provider_manager_->AddObserver(this);
}

void VpnListForwarder::AttachToVpnExtensionObserverAsh() {
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->vpn_extension_observer_ash()
      ->SetDelegate(this);
}
