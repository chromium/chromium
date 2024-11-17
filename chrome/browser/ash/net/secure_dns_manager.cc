// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include <map>
#include <string>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/net/ash_dns_over_https_config_source.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/country_codes/country_codes.h"
#include "net/dns/public/doh_provider_entry.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {
void MigratePrefIfNecessary(const std::string& pref_name,
                            PrefService* from_local_state,
                            PrefService* to_profile_prefs) {
  const base::Value* user_pref_value =
      to_profile_prefs->GetUserPrefValue(pref_name);
  // If there's already a user-set value in the profile prefs, do not override
  // it.
  if (user_pref_value) {
    return;
  }
  const base::Value* pref_value = from_local_state->GetUserPrefValue(pref_name);
  if (!pref_value) {
    return;
  }
  to_profile_prefs->Set(pref_name, *pref_value);

  // TODO(b/323547098): Clear the DoH user-set local state pref once the
  // migration code has ran for a couple of milestones.
  //  from_local_state->ClearPref(pref_name);
}

bool IsUserSetSecureDnsConfigInvalid(const PrefService* local_state) {
  const base::Value* secure_dns_mode =
      local_state->GetUserPrefValue(::prefs::kDnsOverHttpsMode);

  // Can be missing in tests.
  if (!secure_dns_mode) {
    return false;
  }
  if (*secure_dns_mode != base::Value(SecureDnsConfig::kModeSecure)) {
    return false;
  }

  const base::Value* secure_dns_template =
      local_state->GetUserPrefValue(::prefs::kDnsOverHttpsTemplates);
  if (!secure_dns_template || *secure_dns_template == base::Value()) {
    return true;
  }

  return false;
}

// Although DNS configurations are user specific, the initial implementation
// stored the secure DNS config to local_state, making it available to all users
// of the device. Part of restricting the user-set secure DNS preference to the
// user session is migrating the related preferences from local_state to profile
// prefs. See b/323547098 for details.
void MigrateDnsOverHttpsPrefs(PrefService* from_local_state,
                              PrefService* to_profile_prefs) {
  if (IsUserSetSecureDnsConfigInvalid(from_local_state)) {
    // Remove the invalid config. See b/335400734.
    from_local_state->ClearPref(::prefs::kDnsOverHttpsMode);
    return;
  }
  // Migrate local state to user pref.
  MigratePrefIfNecessary(::prefs::kDnsOverHttpsTemplates, from_local_state,
                         to_profile_prefs);
  MigratePrefIfNecessary(::prefs::kDnsOverHttpsMode, from_local_state,
                         to_profile_prefs);
}
}  // namespace
namespace ash {

SecureDnsManager::SecureDnsManager(PrefService* local_state,
                                   PrefService* profile_prefs,
                                   bool is_profile_managed)
    : local_state_(local_state),
      profile_prefs_(profile_prefs),
      is_profile_managed_(is_profile_managed) {
  if (!is_profile_managed) {
    CHECK(profile_prefs) << "Profile prefs cannot be empty for unmanaged users";
    MigrateDnsOverHttpsPrefs(local_state, profile_prefs);
  }
  doh_templates_uri_resolver_ =
      std::make_unique<dns_over_https::TemplatesUriResolverImpl>();

  LoadProviders();

  if (is_profile_managed) {
    MonitorLocalStatePrefs();
    OnLocalStatePrefsChanged();
    OnDoHIncludedDomainsPrefChanged();
    OnDoHExcludedDomainsPrefChanged();
  } else {
    MonitorUserPrefs();
    OnPrefChanged();
  }

  // The DNS-over-HTTPS config source is reset in the destructor of the
  // `SecureDnsManager`. This means the `SecureDnsManager` instance should
  // outlive the `AshDnsOverHttpsConfigSource` instance.
  g_browser_process->system_network_context_manager()
      ->GetStubResolverConfigReader()
      ->SetOverrideDnsOverHttpsConfigSource(
          std::make_unique<AshDnsOverHttpsConfigSource>(this, local_state));
}

void SecureDnsManager::SetPrimaryProfilePropertiesForTesting(
    PrefService* profile_prefs,
    bool is_profile_managed) {
  profile_prefs_registrar_.Reset();
  local_state_registrar_.Reset();
  is_profile_managed_ = is_profile_managed;

  if (is_profile_managed) {
    MonitorLocalStatePrefs();
    OnLocalStatePrefsChanged();
    OnDoHIncludedDomainsPrefChanged();
    OnDoHExcludedDomainsPrefChanged();
  } else {
    MonitorUserPrefs();
    OnPrefChanged();
  }
}

void SecureDnsManager::MonitorUserPrefs() {
  profile_prefs_->SetDefaultPrefValue(
      ::prefs::kDnsOverHttpsMode,
      local_state_->GetDefaultPrefValue(::prefs::kDnsOverHttpsMode)->Clone());

  profile_prefs_registrar_.Init(profile_prefs_);
  profile_prefs_registrar_.Add(
      ::prefs::kDnsOverHttpsMode,
      base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                          base::Unretained(this)));
  profile_prefs_registrar_.Add(
      ::prefs::kDnsOverHttpsTemplates,
      base::BindRepeating(&SecureDnsManager::OnPrefChanged,
                          base::Unretained(this)));
}
void SecureDnsManager::MonitorLocalStatePrefs() {
  local_state_registrar_.Init(local_state_);
  static constexpr std::array<const char*, 4> secure_dns_pref_names = {
      ::prefs::kDnsOverHttpsMode, ::prefs::kDnsOverHttpsTemplates,
      ::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
      ::prefs::kDnsOverHttpsSalt};
  for (auto* const pref_name : secure_dns_pref_names) {
    local_state_registrar_.Add(
        pref_name,
        base::BindRepeating(&SecureDnsManager::OnLocalStatePrefsChanged,
                            base::Unretained(this)));
  }
  local_state_registrar_.Add(
      prefs::kDnsOverHttpsIncludedDomains,
      base::BindRepeating(&SecureDnsManager::OnDoHIncludedDomainsPrefChanged,
                          base::Unretained(this)));
  local_state_registrar_.Add(
      prefs::kDnsOverHttpsExcludedDomains,
      base::BindRepeating(&SecureDnsManager::OnDoHExcludedDomainsPrefChanged,
                          base::Unretained(this)));
}

SecureDnsManager::~SecureDnsManager() {
  for (auto& observer : observers_) {
    observer.OnSecureDnsManagerShutdown();
  }

  g_browser_process->system_network_context_manager()
      ->GetStubResolverConfigReader()
      ->SetOverrideDnsOverHttpsConfigSource(nullptr);

  // `local_state_` outlives the SecureDnsManager instance. The value of
  // `::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS` should not outlive the
  // current instance of SecureDnsManager.
  local_state_->ClearPref(::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS);

  // Reset Shill's state in order for the secure DNS configuration to not leak
  // to the login screen.
  ResetShillState();
}

void SecureDnsManager::SetDoHTemplatesUriResolverForTesting(
    std::unique_ptr<dns_over_https::TemplatesUriResolver>
        doh_templates_uri_resolver) {
  CHECK_IS_TEST();
  doh_templates_uri_resolver_ = std::move(doh_templates_uri_resolver);
}

void SecureDnsManager::LoadProviders() {
  // Note: Check whether each provider is enabled *after* filtering based on
  // country code so that if we are doing experimentation via Finch for a
  // regional provider, the experiment groups will be less likely to include
  // users from other regions unnecessarily (since a client will be included in
  // the experiment if the provider feature flag is checked).
  const net::DohProviderEntry::List local_providers =
      chrome_browser_net::secure_dns::SelectEnabledProviders(
          chrome_browser_net::secure_dns::ProvidersForCountry(
              net::DohProviderEntry::GetList(),
              country_codes::GetCurrentCountryID()));

  for (const net::DohProviderEntry* provider : local_providers) {
    std::vector<std::string> ip_addrs;
    base::ranges::transform(provider->ip_addresses,
                            std::back_inserter(ip_addrs),
                            &net::IPAddress::ToString);
    local_doh_providers_[provider->doh_server_config] =
        base::JoinString(ip_addrs, ",");
  }
}

base::Value::Dict SecureDnsManager::GetProviders(
    const std::string& mode,
    const std::string& templates) const {
  base::Value::Dict doh_providers;

  if (mode == SecureDnsConfig::kModeOff) {
    return doh_providers;
  }

  // If there are templates then use them. In secure mode, the values, which
  // hold the IP addresses of the name servers, are left empty. In secure DNS
  // mode with fallback to plain-text nameservers, the values are stored as a
  // wildcard character denoting that it matches any IP addresses. In automatic
  // upgrade mode, the corresponding name servers will be populated using the
  // applicable providers.
  const std::string_view addr =
      mode == SecureDnsConfig::kModeSecure
          ? ""
          : shill::kDNSProxyDOHProvidersMatchAnyIPAddress;
  for (const auto& doh_template : base::SplitString(
           templates, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    doh_providers.Set(doh_template, addr);
  }
  if (mode == SecureDnsConfig::kModeSecure) {
    return doh_providers;
  }
  if (!doh_providers.empty()) {
    return doh_providers;
  }

  // No specified DoH providers, relay all DoH provider upgrade configuration
  // for dns-proxy to switch providers whenever the network or its settings
  // change.
  for (const auto& provider : local_doh_providers_) {
    doh_providers.Set(provider.first.server_template(), provider.second);
  }
  return doh_providers;
}

void SecureDnsManager::AddObserver(Observer* observer) {
  observer->OnModeChanged(cached_chrome_mode_);
  observer->OnTemplateUrisChanged(cached_chrome_template_uris_);
  observers_.AddObserver(observer);
}

void SecureDnsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SecureDnsManager::DefaultNetworkChanged(const NetworkState* network) {
  const std::string& mode = local_state_->GetString(::prefs::kDnsOverHttpsMode);
  if (mode == SecureDnsConfig::kModeOff) {
    return;
  }

  // Network updates are only relevant for determining the effective DoH
  // template URI if the admin has configured the
  // DnsOverHttpsTemplatesWithIdentifiers policy to include the IP addresses.
  std::string templates_with_identifiers =
      local_state_->GetString(::prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  if (!dns_over_https::TemplatesUriResolverImpl::
          IsDeviceIpAddressIncludedInUriTemplate(templates_with_identifiers)) {
    return;
  }
  UpdateTemplateUri();
}

void SecureDnsManager::OnPrefChanged() {
  CHECK(profile_prefs_);
  UpdateDoHConfig(profile_prefs_->GetString(::prefs::kDnsOverHttpsMode),
                  profile_prefs_->GetString(::prefs::kDnsOverHttpsTemplates));
}

void SecureDnsManager::OnLocalStatePrefsChanged() {
  UpdateTemplateUri();
  ToggleNetworkMonitoring();
}

void SecureDnsManager::ToggleNetworkMonitoring() {
  // If DoH with identifiers are active, verify if network changes need to be
  // observed for URI template placeholder replacement.
  std::string templates_with_identifiers =
      local_state_->GetString(::prefs::kDnsOverHttpsTemplatesWithIdentifiers);

  bool template_uri_includes_network_identifiers =
      doh_templates_uri_resolver_->GetDohWithIdentifiersActive() &&
      dns_over_https::TemplatesUriResolverImpl::
          IsDeviceIpAddressIncludedInUriTemplate(templates_with_identifiers);

  bool should_observe_default_network_changes =
      template_uri_includes_network_identifiers &&
      (local_state_->GetString(::prefs::kDnsOverHttpsMode) !=
       SecureDnsConfig::kModeOff);

  if (!should_observe_default_network_changes) {
    network_state_handler_observer_.Reset();
    return;
  }
  // Already observing default network changes.
  if (network_state_handler_observer_.IsObserving()) {
    return;
  }
  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());
}

void SecureDnsManager::OnDoHIncludedDomainsPrefChanged() {
  base::Value::List included_domains =
      local_state_->GetList(prefs::kDnsOverHttpsIncludedDomains).Clone();
  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDOHIncludedDomainsProperty,
      base::Value(std::move(included_domains)));
  UpdateCachedDomainConfigSet();
}

void SecureDnsManager::OnDoHExcludedDomainsPrefChanged() {
  base::Value::List excluded_domains =
      local_state_->GetList(prefs::kDnsOverHttpsExcludedDomains).Clone();
  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDOHExcludedDomainsProperty,
      base::Value(std::move(excluded_domains)));
  UpdateCachedDomainConfigSet();
}

void SecureDnsManager::UpdateCachedDomainConfigSet() {
  bool domain_config_set =
      !local_state_->GetList(prefs::kDnsOverHttpsIncludedDomains).empty() ||
      !local_state_->GetList(prefs::kDnsOverHttpsExcludedDomains).empty();
  if (domain_config_set == cached_domain_config_set_) {
    return;
  }
  cached_domain_config_set_ = domain_config_set;

  // If DoH domain config changed, force a DoH config update in order for the UI
  // to be updated.
  UpdateDoHConfig(cached_mode_, cached_template_uris_, /*force_update=*/true);
}

void SecureDnsManager::UpdateDoHConfig(const std::string& new_mode,
                                       const std::string& new_template_uris,
                                       bool force_update) {
  UpdateShillDoHConfig(new_mode, new_template_uris);

  // When DoH included or excluded domains policy is set. DoH must be disabled
  // for Chrome in order for the DNS traffic to reach ChromeOS DNS proxy. At the
  // same time, broadcast the changes to force the UI to be updated.
  std::string new_chrome_mode = new_mode;
  std::string new_chrome_template_uris = new_template_uris;
  if (cached_domain_config_set_) {
    new_chrome_mode = SecureDnsConfig::kModeOff;
    new_chrome_template_uris = std::string();
    force_update = true;
  }
  UpdateChromeDoHConfig(new_chrome_mode, new_chrome_template_uris,
                        force_update);
}

void SecureDnsManager::UpdateShillDoHConfig(
    const std::string& new_mode,
    const std::string& new_template_uris) {
  bool mode_changed = new_mode != cached_mode_;
  bool template_uris_changed = new_template_uris != cached_template_uris_;
  if (!mode_changed && !template_uris_changed) {
    // The secure DNS configuration has not changed
    return;
  }

  // Update cached DoH configs.
  cached_mode_ = new_mode;
  cached_template_uris_ = new_template_uris;

  // Set the DoH URI templae shill property which is synced with platform
  // daemons (shill, dns-proxy etc).
  NetworkHandler::Get()->network_configuration_handler()->SetManagerProperty(
      shill::kDNSProxyDOHProvidersProperty,
      base::Value(GetProviders(cached_mode_, cached_template_uris_)));
}

void SecureDnsManager::UpdateChromeDoHConfig(
    const std::string& new_mode,
    const std::string& new_template_uris,
    bool force_update) {
  bool mode_changed = new_mode != cached_chrome_mode_;
  bool template_uris_changed =
      new_template_uris != cached_chrome_template_uris_;
  if (!mode_changed && !template_uris_changed && !force_update) {
    // The secure DNS configuration has not changed
    return;
  }

  // Update cached DoH configs.
  cached_chrome_mode_ = new_mode;
  cached_chrome_template_uris_ = new_template_uris;

  // Broadcast DoH config updates for Chrome.
  for (auto& observer : observers_) {
    if (template_uris_changed || force_update) {
      observer.OnTemplateUrisChanged(cached_chrome_template_uris_);
    }
    if (mode_changed || force_update) {
      observer.OnModeChanged(cached_chrome_mode_);
    }
  }

  // Set the DoH URI template pref which is synced with Lacros and the
  // NetworkService.
  // TODO(acostinas, b/331903009): Storing the effective DoH providers in a
  // local_state pref on Chrome OS has downsides. Replace this pref with an
  // in-memory mechanism to sync effective DoH prefs.
  local_state_->SetString(::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
                          cached_chrome_template_uris_);
}

void SecureDnsManager::UpdateTemplateUri() {
  doh_templates_uri_resolver_->Update(local_state_);

  std::string new_templates =
      doh_templates_uri_resolver_->GetEffectiveTemplates();
  std::string new_mode = local_state_->GetString(::prefs::kDnsOverHttpsMode);

  // The DoH config is stored in the local_state only when controlled by policy.
  // If the local_state DoH pref is user-set, it should be ignored.
  if (!local_state_->IsManagedPreference(::prefs::kDnsOverHttpsMode)) {
    new_mode = SecureDnsConfig::kModeOff;
    new_templates = std::string();
  }

  bool prev_is_config_managed = cached_is_config_managed_;
  cached_is_config_managed_ =
      local_state_->FindPreference(::prefs::kDnsOverHttpsMode)->IsManaged();

  // Force DoH config update if the managedness of the config changed.
  UpdateDoHConfig(new_mode, new_templates,
                  cached_is_config_managed_ != prev_is_config_managed);

  // May be missing in tests.
  if (NetworkHandler::Get()->network_metadata_store()) {
    // TODO(b/323547098): Remove the pref management check. In older OS
    // versions, user DoH settings were in local_state, but DoH with identifiers
    // is enterprise-only feature.
    NetworkHandler::Get()
        ->network_metadata_store()
        ->SetSecureDnsTemplatesWithIdentifiersActive(
            doh_templates_uri_resolver_->GetDohWithIdentifiersActive() &&
            local_state_->IsManagedPreference(::prefs::kDnsOverHttpsMode));
  }
}

void SecureDnsManager::ResetShillState() {
  if (!NetworkHandler::IsInitialized()) {
    return;
  }
  auto* handler = NetworkHandler::Get()->network_configuration_handler();
  handler->SetManagerProperty(shill::kDOHIncludedDomainsProperty,
                              base::Value(base::Value::List()));
  handler->SetManagerProperty(shill::kDOHExcludedDomainsProperty,
                              base::Value(base::Value::List()));
  handler->SetManagerProperty(
      shill::kDNSProxyDOHProvidersProperty,
      base::Value(GetProviders(SecureDnsConfig::kModeOff, /*templates=*/"")));
}

// static
void SecureDnsManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(::prefs::kDnsOverHttpsMode, std::string());
  registry->RegisterStringPref(::prefs::kDnsOverHttpsTemplates, std::string());
}
// static
void SecureDnsManager::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(::prefs::kDnsOverHttpsSalt, std::string());
  registry->RegisterStringPref(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                               std::string());
}

std::optional<std::string>
SecureDnsManager::GetDohWithIdentifiersDisplayServers() const {
  if (doh_templates_uri_resolver_->GetDohWithIdentifiersActive()) {
    return doh_templates_uri_resolver_->GetDisplayTemplates();
  }
  return std::nullopt;
}

net::DnsOverHttpsConfig SecureDnsManager::GetOsDohConfig() const {
  net::DnsOverHttpsConfig doh_config;
  if (cached_mode_ != SecureDnsConfig::kModeOff) {
    doh_config = net::DnsOverHttpsConfig::FromStringLax(cached_template_uris_);
  }
  return doh_config;
}

net::SecureDnsMode SecureDnsManager::GetOsDohMode() const {
  return SecureDnsConfig::ParseMode(cached_mode_)
      .value_or(net::SecureDnsMode::kOff);
}

}  // namespace ash
