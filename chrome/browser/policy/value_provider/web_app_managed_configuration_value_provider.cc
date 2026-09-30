// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/web_app_managed_configuration_value_provider.h"

#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

std::string_view PolicyScopeToString(policy::PolicyScope scope) {
  switch (scope) {
    case policy::POLICY_SCOPE_USER:
      return "user";
    case policy::POLICY_SCOPE_MACHINE:
      return "machine";
  }
}

std::string_view PolicyLevelToString(policy::PolicyLevel level) {
  switch (level) {
    case policy::POLICY_LEVEL_RECOMMENDED:
      return "recommended";
    case policy::POLICY_LEVEL_MANDATORY:
      return "mandatory";
  }
}

struct PolicyMetadata {
  std::string source;
  std::string scope;
  std::string level;
};

PolicyMetadata GetManagedConfigurationPerOriginMetadata(Profile* profile) {
  PolicyMetadata metadata;
  const policy::PolicyMap& chrome_policies =
      GetPolicyService(profile)->GetPolicies(
          policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  const policy::PolicyMap::Entry* entry =
      chrome_policies.Get(policy::key::kManagedConfigurationPerOrigin);
  if (!entry) {
    return metadata;
  }

  metadata.source = policy::kPolicySources[entry->source].name;
  metadata.scope = PolicyScopeToString(entry->scope);
  metadata.level = PolicyLevelToString(entry->level);
  return metadata;
}

base::DictValue CreateManagedConfigurationEntry(
    std::string_view name,
    base::Value value,
    const PolicyMetadata& metadata) {
  base::DictValue config_entry =
      base::DictValue().Set("name", name).Set("value", std::move(value));
  if (!metadata.source.empty()) {
    config_entry.Set("source", metadata.source);
  }
  if (!metadata.scope.empty()) {
    config_entry.Set("scope", metadata.scope);
  }
  if (!metadata.level.empty()) {
    config_entry.Set("level", metadata.level);
  }
  return config_entry;
}

}  // namespace

std::unique_ptr<WebAppManagedConfigurationValueProvider>
WebAppManagedConfigurationValueProvider::Create(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile);
  auto* managed_config_api =
      ManagedConfigurationAPIFactory::GetForProfile(profile);
  if (!web_app_provider || !managed_config_api) {
    return nullptr;
  }
  return base::WrapUnique<WebAppManagedConfigurationValueProvider>(
      new WebAppManagedConfigurationValueProvider(profile, *web_app_provider,
                                                  *managed_config_api));
}

WebAppManagedConfigurationValueProvider::
    WebAppManagedConfigurationValueProvider(
        Profile* profile,
        web_app::WebAppProvider& web_app_provider,
        ManagedConfigurationAPI& managed_configuration_api)
    : profile_(profile),
      web_app_provider_(web_app_provider),
      managed_configuration_api_(managed_configuration_api) {
  install_manager_observation_.Observe(&web_app_provider.install_manager());
  managed_config_observation_.Observe(&managed_configuration_api);

  UpdateManagedConfigurations();
}

WebAppManagedConfigurationValueProvider::
    ~WebAppManagedConfigurationValueProvider() = default;

base::DictValue WebAppManagedConfigurationValueProvider::GetValues() {
  return values_.Clone();
}

base::DictValue WebAppManagedConfigurationValueProvider::GetNames() {
  return names_.Clone();
}

void WebAppManagedConfigurationValueProvider::Refresh() {
  UpdateManagedConfigurations();
}

void WebAppManagedConfigurationValueProvider::OnWebAppInstalled(
    const webapps::AppId& app_id) {
  UpdateManagedConfigurations();
}

void WebAppManagedConfigurationValueProvider::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  UpdateManagedConfigurations();
}

void WebAppManagedConfigurationValueProvider::
    OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void WebAppManagedConfigurationValueProvider::OnAnyManagedConfigurationChanged(
    const url::Origin& origin) {
  UpdateManagedConfigurations();
}

void WebAppManagedConfigurationValueProvider::UpdateManagedConfigurations() {
  if (!web_app_provider_->is_registry_ready()) {
    web_app_provider_->on_registry_ready().Post(
        FROM_HERE, base::BindOnce(&WebAppManagedConfigurationValueProvider::
                                      UpdateManagedConfigurations,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  const std::set<url::Origin>& managed_origins =
      managed_configuration_api_->GetManagedOrigins();

  if (managed_origins.empty()) {
    if (!values_.empty() || !names_.empty()) {
      values_.clear();
      names_.clear();
      NotifyValueChange();
    }
    return;
  }

  auto barrier_callback = base::BarrierCallback<WebAppConfigResult>(
      managed_origins.size(),
      base::BindOnce(&WebAppManagedConfigurationValueProvider::
                         OnAllOriginConfigurationsLoaded,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const auto& origin : managed_origins) {
    managed_configuration_api_->GetOriginPolicyConfiguration(
        origin,
        base::BindOnce(
            [](url::Origin origin,
               base::RepeatingCallback<void(WebAppConfigResult)> barrier,
               std::optional<base::DictValue> maybe_dict) {
              barrier.Run(
                  WebAppConfigResult{std::move(origin), std::move(maybe_dict)});
            },
            origin, barrier_callback));
  }
}

void WebAppManagedConfigurationValueProvider::OnAllOriginConfigurationsLoaded(
    std::vector<WebAppConfigResult> results) {
  // Managed configuration entries inherit their policy metadata (source, scope,
  // level) from the ManagedConfigurationPerOrigin policy.
  PolicyMetadata metadata = GetManagedConfigurationPerOriginMetadata(profile_);

  base::DictValue new_values;
  base::DictValue new_names;

  const auto& registrar = web_app_provider_->registrar_unsafe();
  for (auto& [origin, maybe_dict] : results) {
    const std::string serialized_origin = origin.Serialize();

    base::DictValue config_entries_dict;
    base::ListValue config_keys_list;

    if (maybe_dict) {
      for (auto [config_key, config_value] : *maybe_dict) {
        config_entries_dict.Set(
            config_key, CreateManagedConfigurationEntry(
                            config_key, std::move(config_value), metadata));
        config_keys_list.Append(config_key);
      }
    }

    auto add_section = [&](const std::string& section_key,
                           const std::string& app_name) {
      new_values.Set(section_key, base::DictValue()
                                      .Set(policy::kNameKey, app_name)
                                      .Set(policy::kIdKey, serialized_origin)
                                      .Set("isExtension", false)
                                      .Set("isWebApp", true)
                                      .Set(policy::kPoliciesKey,
                                           config_entries_dict.Clone()));

      new_names.Set(section_key, base::DictValue()
                                     .Set(policy::kNameKey, app_name)
                                     .Set(policy::kPolicyNamesKey,
                                          config_keys_list.Clone()));
    };

    std::vector<webapps::AppId> app_ids = registrar.FindAllAppsNestedInUrl(
        origin.GetURL(), web_app::WebAppFilter::InstalledInChrome());
    if (app_ids.empty()) {
      add_section(serialized_origin, std::string());
    } else {
      for (const webapps::AppId& app_id : app_ids) {
        add_section(registrar.GetAppScope(app_id).spec(),
                    registrar.GetAppShortName(app_id));
      }
    }
  }

  values_ = std::move(new_values);
  names_ = std::move(new_names);
  NotifyValueChange();
}
