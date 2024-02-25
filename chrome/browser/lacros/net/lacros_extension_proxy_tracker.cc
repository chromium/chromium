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
namespace {
crosapi::mojom::NetworkSettingsService* GetNetworkSettingsApi() {
  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
      .get();
}
}  // namespace
namespace net {

LacrosExtensionProxyTracker::LacrosExtensionProxyTracker(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_->IsMainProfile());
  proxy_prefs_registrar_.Init(profile_->GetPrefs());
  proxy_prefs_registrar_.Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&LacrosExtensionProxyTracker::OnProxyPrefChanged,
                          base::Unretained(this)));
  OnProxyPrefChanged(proxy_config::prefs::kProxy);
  extension_observation_.Observe(extensions::ExtensionRegistry::Get(profile));
}

LacrosExtensionProxyTracker::~LacrosExtensionProxyTracker() = default;

void LacrosExtensionProxyTracker::OnProxyPrefChanged(
    const std::string& pref_name) {
  DCHECK(profile_->IsMainProfile());
  DCHECK(pref_name == proxy_config::prefs::kProxy);
  ::net::ProxyConfigWithAnnotation config;
  ProxyPrefs::ConfigState config_state =
      PrefProxyConfigTrackerImpl::ReadPrefConfig(profile_->GetPrefs(), &config);

  if (config_state != ProxyPrefs::ConfigState::CONFIG_EXTENSION) {
    if (extension_proxy_active_) {
      extension_proxy_active_ = false;
      if (AshVersionSupportsExtensionMetadata()) {
        GetNetworkSettingsApi()->ClearExtensionControllingProxyMetadata();
      } else {
        // TODO(acostinas,b/267719988) Remove this deprecated call in 118.
        GetNetworkSettingsApi()->ClearExtensionProxy();
      }
    }
    return;
  }
  extension_proxy_active_ = true;
  const extensions::Extension* extension =
      extensions::GetExtensionOverridingProxy(profile_);

  // The the extension-set proxy pref update is received before the extension is
  // enabled. This happens when the extension store where the pref is stored is
  // loaded before the extension, in one of the following cases:
  // - A new Chrome session is started, where the extension has set the proxy
  // pref in a previous session;
  // - Ash to Lacros migration;
  // - Disabling and then enabling an extension which is controlling the proxy.
  // It's safe to ignore the update here because this case is handled by
  // listening to the ExtensionRegistry events and re-sending the proxy config
  // to Ash when the extension which is controlling the proxy is loaded.
  if (!extension) {
    return;
  }

  auto extension_info = crosapi::mojom::ExtensionControllingProxy::New();
  extension_info->name = extension->name();
  extension_info->id = extension->id();
  extension_info->can_be_disabled =
      !extensions::ExtensionSystem::Get(profile_)
           ->management_policy()
           ->MustRemainEnabled(extension, nullptr);

  if (AshVersionSupportsExtensionMetadata()) {
    GetNetworkSettingsApi()->SetExtensionControllingProxyMetadata(
        std::move(extension_info));
    return;
  }
  auto proxy_config = chromeos::NetProxyToCrosapiProxy(config);
  proxy_config->extension = std::move(extension_info);
  // TODO(acostinas, b/267719988) Remove this deprecated call in 118.
  GetNetworkSettingsApi()->SetExtensionProxy(std::move(proxy_config));
}

void LacrosExtensionProxyTracker::OnExtensionReady(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!extension_proxy_active_) {
    return;
  }
  if (profile_ != Profile::FromBrowserContext(browser_context)) {
    return;
  }
  if (extensions::GetExtensionOverridingProxy(profile_) != extension) {
    return;
  }

  OnProxyPrefChanged(proxy_config::prefs::kProxy);
}

// static
bool LacrosExtensionProxyTracker::AshVersionSupportsExtensionMetadata() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::NetworkSettingsService>()) {
    LOG(ERROR) << "Ash NetworkSettingsService not available";
    return false;
  }

  int network_settings_version =
      service->GetInterfaceVersion<crosapi::mojom::NetworkSettingsService>();
  if (network_settings_version <
      int{crosapi::mojom::NetworkSettingsService::MethodMinVersions::
              kSetExtensionControllingProxyMetadataMinVersion}) {
    LOG(WARNING) << "Ash NetworkSettingsService version "
                 << network_settings_version
                 << " does not support extension set proxies.";
    return false;
  }

  return true;
}

}  // namespace net
}  // namespace lacros
