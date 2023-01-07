// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/lacros_extension_proxy_tracker.h"

#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/lacros/net/network_settings_translation.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace lacros {
namespace net {

LacrosExtensionProxyTracker::LacrosExtensionProxyTracker(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_->IsMainProfile());
  if (!LacrosExtensionProxyTracker::AshVersionSupportsExtensionSetProxies())
    return;

  proxy_prefs_registrar_.Init(profile_->GetPrefs());
  proxy_prefs_registrar_.Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&LacrosExtensionProxyTracker::OnProxyPrefChanged,
                          base::Unretained(this)));
  OnProxyPrefChanged(proxy_config::prefs::kProxy);
}

LacrosExtensionProxyTracker::~LacrosExtensionProxyTracker() = default;

void LacrosExtensionProxyTracker::OnProxyPrefChanged(
    const std::string& pref_name) {
  DCHECK(profile_->IsMainProfile());
  DCHECK(pref_name == proxy_config::prefs::kProxy);
  ::net::ProxyConfigWithAnnotation config;
  ProxyPrefs::ConfigState config_state =
      PrefProxyConfigTrackerImpl::ReadPrefConfig(profile_->GetPrefs(), &config);

  auto* lacros_service = chromeos::LacrosService::Get();

  if (config_state != ProxyPrefs::ConfigState::CONFIG_EXTENSION) {
    if (extension_proxy_active_) {
      extension_proxy_active_ = false;
      lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
          ->ClearExtensionProxy();
    }
    return;
  }
  extension_proxy_active_ = true;
  const extensions::Extension* extension =
      extensions::GetExtensionOverridingProxy(profile_);

  // This can happen in a new Chrome session, if the extension proxy was set
  // in a previous Chrome session. In this case, it's safe to exit here since
  // Ash-Chrome already has the information of the extension which set the
  // proxy. The reason is that the extension store where the pref is stored is
  // loaded before the extension which is controlling the pref.
  if (!extension) {
    return;
  }

  auto proxy_config = chromeos::NetProxyToCrosapiProxy(config);
  proxy_config->extension = crosapi::mojom::ExtensionControllingProxy::New();
  proxy_config->extension->name = extension->name();
  proxy_config->extension->id = extension->id();
  proxy_config->extension->can_be_disabled =
      !extensions::ExtensionSystem::Get(profile_)
           ->management_policy()
           ->MustRemainEnabled(extension, nullptr);

  lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
      ->SetExtensionProxy(std::move(proxy_config));
}

// static
bool LacrosExtensionProxyTracker::AshVersionSupportsExtensionSetProxies() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::NetworkSettingsService>()) {
    LOG(ERROR) << "Ash NetworkSettingsService not available";
    return false;
  }

  int network_settings_version = service->GetInterfaceVersion(
      crosapi::mojom::NetworkSettingsService::Uuid_);
  if (network_settings_version <
      int{crosapi::mojom::NetworkSettingsService::MethodMinVersions::
              kSetExtensionProxyMinVersion}) {
    LOG(WARNING) << "Ash NetworkSettingsService version "
                 << network_settings_version
                 << " does not support extension set proxies.";
    return false;
  }

  return true;
}

}  // namespace net
}  // namespace lacros
