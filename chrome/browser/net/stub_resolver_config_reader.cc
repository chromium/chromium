// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/util.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/enterprise/util/android_enterprise_info.h"
#endif

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/parental_controls.h"
#endif

namespace {

// Detailed descriptions of the secure DNS mode. These values are logged to UMA.
// Entries should not be renumbered and numeric values should never be reused.
// Please keep in sync with "SecureDnsModeDetails" in
// src/tools/metrics/histograms/enums.xml.
enum class SecureDnsModeDetailsForHistogram {
  // The mode is controlled by the user and is set to 'off'.
  kOffByUser = 0,
  // The mode is controlled via enterprise policy and is set to 'off'.
  kOffByEnterprisePolicy = 1,
  // Chrome detected a managed environment and forced the mode to 'off'.
  kOffByDetectedManagedEnvironment = 2,
  // Chrome detected parental controls and forced the mode to 'off'.
  kOffByDetectedParentalControls = 3,
  // The mode is controlled by the user and is set to 'automatic' (the default
  // mode).
  kAutomaticByUser = 4,
  // The mode is controlled via enterprise policy and is set to 'automatic'.
  kAutomaticByEnterprisePolicy = 5,
  // The mode is controlled by the user and is set to 'secure'.
  kSecureByUser = 6,
  // The mode is controlled via enterprise policy and is set to 'secure'.
  kSecureByEnterprisePolicy = 7,
  kMaxValue = kSecureByEnterprisePolicy,
};

#if defined(OS_WIN)
bool ShouldDisableDohForWindowsParentalControls() {
  const WinParentalControls& parental_controls = GetWinParentalControls();
  if (parental_controls.web_filter)
    return true;

  // Some versions before Windows 8 may not fully support |web_filter|, so
  // conservatively disable doh for any recognized parental controls.
  if (parental_controls.any_restrictions &&
    base::win::GetVersion() < base::win::Version::WIN8) {
    return true;
  }

  return false;
}
#endif  // defined(OS_WIN)

// Check the AsyncDns field trial and return true if it should be enabled. On
// Android this includes checking the Android version in the field trial.
bool ShouldEnableAsyncDns() {
  bool feature_can_be_enabled = true;
#if defined(OS_ANDROID)
  int min_sdk =
      base::GetFieldTrialParamByFeatureAsInt(features::kAsyncDns, "min_sdk", 0);
  if (base::android::BuildInfo::GetInstance()->sdk_int() < min_sdk)
    feature_can_be_enabled = false;
#endif
  return feature_can_be_enabled &&
         base::FeatureList::IsEnabled(features::kAsyncDns);
}

}  // namespace

// static
constexpr base::TimeDelta StubResolverConfigReader::kParentalControlsCheckDelay;

StubResolverConfigReader::StubResolverConfigReader(PrefService* local_state,
                                                   bool set_up_pref_defaults)
    : local_state_(local_state) {
  base::RepeatingClosure pref_callback =
      base::BindRepeating(&StubResolverConfigReader::UpdateNetworkService,
                          base::Unretained(this), false /* record_metrics */);

  pref_change_registrar_.Init(local_state_);

  // Update the DnsClient and DoH default preferences based on the corresponding
  // features before registering change callbacks for these preferences.
  // Changing prefs or defaults after registering change callbacks could result
  // in reentrancy and mess up registration between this code and NetworkService
  // creation.
  if (set_up_pref_defaults) {
    local_state_->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                      base::Value(ShouldEnableAsyncDns()));
    net::SecureDnsMode default_secure_dns_mode = net::SecureDnsMode::kOff;
    std::string default_doh_templates;
    if (base::FeatureList::IsEnabled(features::kDnsOverHttps)) {
      if (features::kDnsOverHttpsFallbackParam.Get()) {
        default_secure_dns_mode = net::SecureDnsMode::kAutomatic;
      } else {
        default_secure_dns_mode = net::SecureDnsMode::kSecure;
      }
      default_doh_templates = features::kDnsOverHttpsTemplatesParam.Get();
    }
    local_state_->SetDefaultPrefValue(
        prefs::kDnsOverHttpsMode,
        base::Value(SecureDnsConfig::ModeToString(default_secure_dns_mode)));
    local_state_->SetDefaultPrefValue(prefs::kDnsOverHttpsTemplates,
                                      base::Value(default_doh_templates));

    // If the user has explicitly enabled or disabled the DoH experiment in
    // chrome://flags and the DoH UI setting is not visible, store that choice
    // in the user prefs so that it can be persisted after the experiment ends.
    // Also make sure to remove the stored prefs value if the user has changed
    // their chrome://flags selection to the default.
    if (!features::kDnsOverHttpsShowUiParam.Get()) {
      flags_ui::PrefServiceFlagsStorage flags_storage(local_state_);
      std::set<std::string> entries = flags_storage.GetFlags();
      if (entries.count("dns-over-https@1")) {
        // The user has "Enabled" selected.
        local_state_->SetString(prefs::kDnsOverHttpsMode,
                                SecureDnsConfig::kModeAutomatic);
      } else if (entries.count("dns-over-https@2")) {
        // The user has "Disabled" selected.
        local_state_->SetString(prefs::kDnsOverHttpsMode,
                                SecureDnsConfig::kModeOff);
      } else {
        // The user has "Default" selected.
        local_state_->ClearPref(prefs::kDnsOverHttpsMode);
      }
    }
  }

  pref_change_registrar_.Add(prefs::kBuiltInDnsClientEnabled, pref_callback);
  pref_change_registrar_.Add(prefs::kDnsOverHttpsMode, pref_callback);
  pref_change_registrar_.Add(prefs::kDnsOverHttpsTemplates, pref_callback);

  parental_controls_delay_timer_.Start(
      FROM_HERE, kParentalControlsCheckDelay,
      base::BindOnce(&StubResolverConfigReader::OnParentalControlsDelayTimer,
                     base::Unretained(this)));

#if defined(OS_ANDROID)
  chrome::enterprise_util::AndroidEnterpriseInfo::GetInstance()
      ->GetAndroidEnterpriseInfoState(base::BindOnce(
          &StubResolverConfigReader::OnAndroidOwnedStateCheckComplete,
          weak_factory_.GetWeakPtr()));
#endif
}

StubResolverConfigReader::~StubResolverConfigReader() = default;

// static
void StubResolverConfigReader::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register the DnsClient and DoH preferences. The feature list has not been
  // initialized yet, so setting the preference defaults here to reflect the
  // corresponding features will only cause the preference defaults to reflect
  // the feature defaults (feature values set via the command line will not be
  // captured). Thus, the preference defaults are updated in the constructor
  // for SystemNetworkContextManager, at which point the feature list is ready.
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled, false);
  registry->RegisterStringPref(prefs::kDnsOverHttpsMode, std::string());
  registry->RegisterStringPref(prefs::kDnsOverHttpsTemplates, std::string());
}

SecureDnsConfig StubResolverConfigReader::GetSecureDnsConfiguration(
    bool force_check_parental_controls_for_automatic_mode) {
  return GetAndUpdateConfiguration(
      force_check_parental_controls_for_automatic_mode,
      false /* record_metrics */, false /* update_network_service */);
}

void StubResolverConfigReader::UpdateNetworkService(bool record_metrics) {
  GetAndUpdateConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */,
      record_metrics, true /* update_network_service */);
}

bool StubResolverConfigReader::ShouldDisableDohForManaged() {
// This function ignores cloud policies which are loaded on a per-profile basis.
#if defined(OS_ANDROID)
  // Check for MDM/management/owner apps. android_has_owner_ is true if either a
  // device or policy owner app is discovered by
  // GetAndroidEnterpriseInfoState(). If android_has_owner_ is nullopt, take a
  // value of false so that we don't disable DoH during the async check.

  // Because Android policies can only be loaded with owner apps this is
  // sufficient to check for the prescences of policies as well.
  if (android_has_owner_.value_or(false))
    return true;
#elif defined(OS_WIN)
  if (base::IsMachineExternallyManaged())
    return true;
#endif
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (g_browser_process->browser_policy_connector()->HasMachineLevelPolicies())
    return true;
#endif
  return false;
}

bool StubResolverConfigReader::ShouldDisableDohForParentalControls() {
#if defined(OS_WIN)
  return ShouldDisableDohForWindowsParentalControls();
#endif

  return false;
}

void StubResolverConfigReader::OnParentalControlsDelayTimer() {
  DCHECK(!parental_controls_delay_timer_.IsRunning());

  // No need to act if parental controls were checked early.
  if (parental_controls_checked_)
    return;
  parental_controls_checked_ = true;

  // If parental controls are enabled, force a config change so secure DNS can
  // be disabled.
  if (ShouldDisableDohForParentalControls())
    UpdateNetworkService(false /* record_metrics */);
}

bool StubResolverConfigReader::GetInsecureStubResolverEnabled() {
  return local_state_->GetBoolean(prefs::kBuiltInDnsClientEnabled);
}

SecureDnsConfig StubResolverConfigReader::GetAndUpdateConfiguration(
    bool force_check_parental_controls_for_automatic_mode,
    bool record_metrics,
    bool update_network_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::SecureDnsMode secure_dns_mode;
  SecureDnsModeDetailsForHistogram mode_details;
  SecureDnsConfig::ManagementMode forced_management_mode =
      SecureDnsConfig::ManagementMode::kNoOverride;
  bool is_managed =
      local_state_->FindPreference(prefs::kDnsOverHttpsMode)->IsManaged();
  if (!is_managed && ShouldDisableDohForManaged()) {
    secure_dns_mode = net::SecureDnsMode::kOff;
    forced_management_mode = SecureDnsConfig::ManagementMode::kDisabledManaged;
  } else {
    secure_dns_mode = SecureDnsConfig::ParseMode(
                          local_state_->GetString(prefs::kDnsOverHttpsMode))
                          .value_or(net::SecureDnsMode::kOff);
  }

  bool check_parental_controls = false;
  if (secure_dns_mode == net::SecureDnsMode::kSecure) {
    mode_details =
        is_managed ? SecureDnsModeDetailsForHistogram::kSecureByEnterprisePolicy
                   : SecureDnsModeDetailsForHistogram::kSecureByUser;

    // SECURE mode must always check for parental controls immediately (unless
    // enabled through policy, which takes precedence over parental controls)
    // because the mode allows sending DoH requests immediately.
    check_parental_controls = !is_managed;
  } else if (secure_dns_mode == net::SecureDnsMode::kAutomatic) {
    mode_details =
        is_managed
            ? SecureDnsModeDetailsForHistogram::kAutomaticByEnterprisePolicy
            : SecureDnsModeDetailsForHistogram::kAutomaticByUser;

    // To avoid impacting startup performance, AUTOMATIC mode should defer
    // checking parental for a short period. This delay should have no practical
    // effect on DoH queries because DoH enabling probes do not start until a
    // longer period after startup.
    bool allow_check_parental_controls =
        force_check_parental_controls_for_automatic_mode ||
        parental_controls_checked_;
    check_parental_controls = !is_managed && allow_check_parental_controls;
  } else {
    switch (forced_management_mode) {
      case SecureDnsConfig::ManagementMode::kNoOverride:
        mode_details =
            is_managed
                ? SecureDnsModeDetailsForHistogram::kOffByEnterprisePolicy
                : SecureDnsModeDetailsForHistogram::kOffByUser;
        break;
      case SecureDnsConfig::ManagementMode::kDisabledManaged:
        mode_details =
            SecureDnsModeDetailsForHistogram::kOffByDetectedManagedEnvironment;
        break;
      case SecureDnsConfig::ManagementMode::kDisabledParentalControls:
        NOTREACHED();
        break;
      default:
        NOTREACHED();
    }

    // No need to check for parental controls if DoH is already disabled.
    check_parental_controls = false;
  }

  // Check parental controls last because it can be expensive and should only be
  // checked if necessary for the otherwise-determined mode.
  if (check_parental_controls) {
    if (ShouldDisableDohForParentalControls()) {
      forced_management_mode =
          SecureDnsConfig::ManagementMode::kDisabledParentalControls;
      secure_dns_mode = net::SecureDnsMode::kOff;
      mode_details =
          SecureDnsModeDetailsForHistogram::kOffByDetectedParentalControls;

      // If parental controls had not previously been checked, need to update
      // network service.
      if (!parental_controls_checked_)
        update_network_service = true;
    }

    parental_controls_checked_ = true;
  }

  if (record_metrics) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsConfig.SecureDnsMode", mode_details);
  }

  std::string doh_templates =
      local_state_->GetString(prefs::kDnsOverHttpsTemplates);
  std::string server_method;
  std::vector<net::DnsOverHttpsServerConfig> dns_over_https_servers;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      servers_mojo;
  if (!doh_templates.empty() && secure_dns_mode != net::SecureDnsMode::kOff) {
    for (base::StringPiece server_template :
         chrome_browser_net::secure_dns::SplitGroup(doh_templates)) {
      if (!net::dns_util::IsValidDohTemplate(server_template, &server_method)) {
        continue;
      }

      bool use_post = server_method == "POST";
      dns_over_https_servers.emplace_back(std::string(server_template),
                                          use_post);

      if (!servers_mojo.has_value()) {
        servers_mojo = base::make_optional<
            std::vector<network::mojom::DnsOverHttpsServerPtr>>();
      }

      network::mojom::DnsOverHttpsServerPtr server_mojo =
          network::mojom::DnsOverHttpsServer::New();
      server_mojo->server_template = std::string(server_template);
      server_mojo->use_post = use_post;
      servers_mojo->emplace_back(std::move(server_mojo));
    }
  }

  if (update_network_service) {
    content::GetNetworkService()->ConfigureStubHostResolver(
        GetInsecureStubResolverEnabled(), secure_dns_mode,
        std::move(servers_mojo));
  }

  return SecureDnsConfig(secure_dns_mode, std::move(dns_over_https_servers),
                         forced_management_mode);
}

#if defined(OS_ANDROID)
void StubResolverConfigReader::OnAndroidOwnedStateCheckComplete(
    bool has_profile_owner,
    bool has_device_owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  android_has_owner_ = has_profile_owner || has_device_owner;
  // update the network service if the actual result is "true" to save time.
  if (android_has_owner_.value())
    UpdateNetworkService(false /* record_metrics */);
}
#endif
