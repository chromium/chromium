// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_WEB_APP_MANAGED_CONFIGURATION_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_WEB_APP_MANAGED_CONFIGURATION_VALUE_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "url/origin.h"

class Profile;

namespace web_app {
class WebAppProvider;
}  // namespace web_app

class WebAppManagedConfigurationValueProvider
    : public policy::PolicyValueProvider,
      public web_app::WebAppInstallManagerObserver,
      public ManagedConfigurationAPI::AnyOriginObserver {
 public:
  static std::unique_ptr<WebAppManagedConfigurationValueProvider> Create(
      Profile* profile);

  ~WebAppManagedConfigurationValueProvider() override;

  // policy::PolicyValueProvider:
  base::DictValue GetValues() override;
  base::DictValue GetNames() override;
  void Refresh() override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

  // ManagedConfigurationAPI::AnyOriginObserver:
  void OnAnyManagedConfigurationChanged(const url::Origin& origin) override;

 private:
  WebAppManagedConfigurationValueProvider(
      Profile* profile,
      web_app::WebAppProvider& web_app_provider,
      ManagedConfigurationAPI& managed_configuration_api);

  struct WebAppConfigResult {
    url::Origin origin;
    std::optional<base::DictValue> maybe_dict;
  };

  void UpdateManagedConfigurations();
  void OnAllOriginConfigurationsLoaded(std::vector<WebAppConfigResult> results);

  raw_ptr<Profile> profile_;
  raw_ref<web_app::WebAppProvider> web_app_provider_;
  raw_ref<ManagedConfigurationAPI> managed_configuration_api_;

  base::DictValue values_;
  base::DictValue names_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::ScopedObservation<ManagedConfigurationAPI,
                          ManagedConfigurationAPI::AnyOriginObserver>
      managed_config_observation_{this};

  base::WeakPtrFactory<WebAppManagedConfigurationValueProvider>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_POLICY_VALUE_PROVIDER_WEB_APP_MANAGED_CONFIGURATION_VALUE_PROVIDER_H_
