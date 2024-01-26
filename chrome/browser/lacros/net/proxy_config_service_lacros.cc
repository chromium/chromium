// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/proxy_config_service_lacros.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/lacros/net/lacros_extension_proxy_tracker.h"
#include "chrome/browser/lacros/net/network_settings_translation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace {

net::ProxyConfigWithAnnotation GetConfigOrDirect(
    const std::optional<net::ProxyConfigWithAnnotation>& optional_config,
    Profile* profile,
    bool proxy_controlled_by_extension) {
  DCHECK(profile);

  auto CreateDirect = net::ProxyConfigWithAnnotation::CreateDirect;

  if (!optional_config)
    return CreateDirect();

  // The primary profile always uses the OS proxy.
  if (profile->IsMainProfile())
    return optional_config.value();

  // Incognito profile doesn't apply proxies set by an extension in the main
  // profile.
  if (profile->IsIncognitoProfile()) {
    if (proxy_controlled_by_extension)
      return CreateDirect();
    return optional_config.value();
  }

  // Secondary Lacros profiles can opt to use the OS proxy.
  return profile->GetPrefs()->GetBoolean(prefs::kUseAshProxy)
             ? optional_config.value()
             : CreateDirect();
}
}  // namespace

namespace chromeos {

ProxyConfigServiceLacros::ProxyConfigServiceLacros(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
  auto* lacros_service = chromeos::LacrosService::Get();
  // crosapi is disabled in browser_tests.
  if (!lacros_service->IsAvailable<crosapi::mojom::NetworkSettingsService>()) {
    LOG(ERROR) << "The NetworkSettingsService service is not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
      ->AddNetworkSettingsObserver(receiver_.BindNewPipeAndPassRemote());

  // `kUseAshProxy` is a user exposed setting whether to use the ash proxy (from
  // the system) or whether to use profile specific proxy settings. This option
  // is only given for secondary profiles. For the primary profile, the user has
  // to use the system proxy settings, the value of kUseAshProxy is always true,
  // and the setting is not exposed to the user.
  // TODO(acostinas, b:192915915) Enable secondary profiles to configure
  // `kUseAshProxy` from chrome://settings.
  if (profile->IsMainProfile()) {
    lacros_extension_proxy_tracker_ =
        std::make_unique<lacros::net::LacrosExtensionProxyTracker>(profile);
  }

  profile_pref_change_registrar_.Init(profile->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kUseAshProxy,
      base::BindRepeating(&ProxyConfigServiceLacros::OnUseAshProxyPrefChanged,
                          base::Unretained(this)));
}

ProxyConfigServiceLacros::~ProxyConfigServiceLacros() = default;

void ProxyConfigServiceLacros::OnProxyChanged(
    crosapi::mojom::ProxyConfigPtr proxy_config) {
  proxy_controlled_by_extension_ = !proxy_config->extension.is_null();
  cached_config_ = CrosapiProxyToNetProxy(std::move(proxy_config));
  NotifyObservers();
}

void ProxyConfigServiceLacros::
    OnAlwaysOnVpnPreConnectUrlAllowlistEnforcedChanged(bool enforced) {}

void ProxyConfigServiceLacros::OnUseAshProxyPrefChanged() {
  NotifyObservers();
}

void ProxyConfigServiceLacros::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnProxyConfigChanged(
        GetConfigOrDirect(cached_config_, profile_,
                          proxy_controlled_by_extension_),
        ConfigAvailability::CONFIG_VALID);
  }
}

void ProxyConfigServiceLacros::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyConfigServiceLacros::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void ProxyConfigServiceLacros::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUseAshProxy, false);
}

net::ProxyConfigService::ConfigAvailability
ProxyConfigServiceLacros::GetLatestProxyConfig(
    net::ProxyConfigWithAnnotation* config) {
  // Returns the last proxy configuration that the Ash
  // NetworkSettingsService notified us, or the direct proxy if `cached_config_`
  // is not set.
  *config = GetConfigOrDirect(cached_config_, profile_,
                              proxy_controlled_by_extension_);
  return ConfigAvailability::CONFIG_VALID;
}

}  // namespace chromeos
