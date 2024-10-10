// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/ip_protection/ip_protection_core_host.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/permissions/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "crypto/crypto_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/cert/asn1_util.h"
#include "net/disk_cache/backend_experiment.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_util.h"
#include "net/net_buildflags.h"
#include "net/ssl/client_cert_store.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "net/cert/x509_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/net/client_cert_store_ash.h"
#include "chrome/browser/ash/net/client_cert_store_kcer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/ui/crypto_module_delegate_nss.h"
#include "net/ssl/client_cert_store_nss.h"
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(IS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "net/ssl/client_cert_store_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check_is_test.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "chrome/browser/lacros/cert/client_cert_store_lacros.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/client_certificates_service.h"
#include "components/enterprise/client_certificates/core/features.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/net/server_certificate_database.h"     // nogncheck
#include "chrome/browser/net/server_certificate_database.pb.h"  // nogncheck
#include "chrome/browser/net/server_certificate_database_service.h"  // nogncheck
#include "chrome/browser/net/server_certificate_database_service_factory.h"  // nogncheck
#endif

namespace {

bool* g_discard_domain_reliability_uploads_for_testing = nullptr;

const char kHttpCacheFinchExperimentGroups[] =
    "profile_network_context_service.http_cache_finch_experiment_groups";

std::vector<std::string> TranslateStringArray(const base::Value::List& list) {
  std::vector<std::string> strings;
  for (const base::Value& value : list) {
    DCHECK(value.is_string());
    strings.push_back(value.GetString());
  }
  return strings;
}

std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      net::HttpUtil::ExpandLanguageList(language_pref);
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

// Tests allowing ambient authentication with default credentials based on the
// profile type.
bool IsAmbientAuthAllowedForProfile(Profile* profile) {
  // Ambient authentication is always enabled for regular and system profiles.
  // System profiles (used in profile picker) may require authentication to
  // let user login.
  if (profile->IsRegularProfile() || profile->IsSystemProfile())
    return true;

  // Non-primary OTR profiles are not used to create browser windows and are
  // only technical means for a task that does not need to leave state after
  // it's completed.
  if (profile->IsOffTheRecord() && !profile->IsPrimaryOTRProfile())
    return true;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  DCHECK(local_state->FindPreference(
      prefs::kAmbientAuthenticationInPrivateModesEnabled));

  net::AmbientAuthAllowedProfileTypes type =
      static_cast<net::AmbientAuthAllowedProfileTypes>(local_state->GetInteger(
          prefs::kAmbientAuthenticationInPrivateModesEnabled));

  if (profile->IsGuestSession()) {
    return type == net::AmbientAuthAllowedProfileTypes::kGuestAndRegular ||
           type == net::AmbientAuthAllowedProfileTypes::kAll;
  } else if (profile->IsIncognitoProfile()) {
    return type == net::AmbientAuthAllowedProfileTypes::kIncognitoAndRegular ||
           type == net::AmbientAuthAllowedProfileTypes::kAll;
  }

  // Profile type not yet supported.
  NOTREACHED_IN_MIGRATION();

  return false;
}

void UpdateAntiAbuseSettings(Profile* profile) {
  ContentSetting content_setting =
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(ContentSettingsType::ANTI_ABUSE, nullptr);
  const bool block_trust_tokens = content_setting == CONTENT_SETTING_BLOCK;
  profile->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->SetBlockTrustTokens(
            block_trust_tokens);
      });
}

bool IsContentSettingsTypeEnabled(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::STORAGE_ACCESS:
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return true;
    default:
      return content_settings::CookieSettings::GetContentSettingsTypes()
          .contains(type);
  }
}

void UpdateCookieSettings(Profile* profile, ContentSettingsType type) {
  if (!IsContentSettingsTypeEnabled(type)) {
    return;
  }

  ContentSettingsForOneType settings;
  if (type == ContentSettingsType::FEDERATED_IDENTITY_SHARING) {
    // Note: FederatedIdentityPermissionContext also syncs the permissions
    // directly, in order to avoid a race condition. (Namely,
    // FederatedIdentityPermissionContext must guarantee that the permissions
    // have propagated before it calls its callback. However, the syncing that
    // occurs in this class is unsynchronized, so it would be racy to rely on
    // this update finishing before calling the context's callback.) This
    // unfortunately triggers a double-update here.
    if (FederatedIdentityPermissionContext* fedcm_context =
            FederatedIdentityPermissionContextFactory::GetForProfile(profile);
        fedcm_context) {
      settings = fedcm_context->GetSharingPermissionGrantsAsContentSettings();
    }
  } else {
    settings = HostContentSettingsMapFactory::GetForProfile(profile)
                   ->GetSettingsForOneType(type);
  }
  profile->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetCookieManagerForBrowserProcess()
            ->SetContentSettings(type, settings, base::NullCallback());
      });
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<net::ClientCertStore> GetWrappedCertStore(
    Profile* profile,
    std::unique_ptr<net::ClientCertStore> platform_store) {
  if (!profile || !client_certificates::features::
                      IsManagedClientCertificateForUserEnabled()) {
    return platform_store;
  }

  auto* provisioning_service =
      client_certificates::CertificateProvisioningServiceFactory::GetForProfile(
          profile);
  if (!provisioning_service) {
    return platform_store;
  }

  return client_certificates::ClientCertificatesService::Create(
      provisioning_service, std::move(platform_store));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

bool IsValidDNSConstraint(std::string_view possible_dns_constraint) {
  return base::IsStringASCII(possible_dns_constraint) &&
         possible_dns_constraint.length() <= 255;
}

// Parses the |possible_cidr_constraint|, populating |parsed_cidr| and |mask|,
// and then return true.
//
// If |possible_cidr_constraint| did not properly parse, returns false. The
// state of |parsed_cidr| and |mask| in this case is not guaranteed.
bool ParseCIDRConstraint(std::string_view possible_cidr_constraint,
                         net::IPAddress* parsed_cidr,
                         net::IPAddress* mask) {
  size_t prefix_length;
  if (!net::ParseCIDRBlock(possible_cidr_constraint, parsed_cidr,
                           &prefix_length)) {
    return false;
  }
  if (parsed_cidr->IsIPv4()) {
    if (!net::IPAddress::CreateIPv4Mask(mask, prefix_length)) {
      return false;
    }
  } else if (parsed_cidr->IsIPv6()) {
    if (!net::IPAddress::CreateIPv6Mask(mask, prefix_length)) {
      return false;
    }
  } else {
    // Somehow got an IP address that isn't ipv4 or ipv6?
    return false;
  }
  return true;
}

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
// Add a cert with constraints to the provided list.
// This will add a certificate from |cert_info| to the |cert_list| with
// any added constraints that are in |cert_info.cert_metadata|. It is okay for
// there to be no constraints in |cert_info.cert_metadata|.
//
// If any constraints in |cert_info.cert_metadata| are not valid, then the
// certificate will not be added to |cert_list| and this function will return
// false. Otherwise, the certificate will be added to |cert_list| and this
// function will return true.
bool MaybeAddCertWithConstraints(
    const net::ServerCertificateDatabase::CertInformation& cert_info,
    std::vector<cert_verifier::mojom::CertWithConstraintsPtr>* cert_list) {
  auto cert_with_constraints_mojo =
      cert_verifier::mojom::CertWithConstraints::New();
  cert_with_constraints_mojo->certificate = cert_info.der_cert;
  for (const auto& dns_constraint :
       cert_info.cert_metadata.constraints().dns_names()) {
    if (IsValidDNSConstraint(dns_constraint)) {
      cert_with_constraints_mojo->permitted_dns_names.push_back(dns_constraint);
    } else {
      return false;
    }
  }
  for (const auto& cidr_constraint :
       cert_info.cert_metadata.constraints().cidrs()) {
    net::IPAddress parsed_cidr;
    net::IPAddress mask;
    if (ParseCIDRConstraint(cidr_constraint, &parsed_cidr, &mask)) {
      cert_with_constraints_mojo->permitted_cidrs.push_back(
          cert_verifier::mojom::CIDR::New(/*ip=*/parsed_cidr,
                                          /*mask=*/mask));
    } else {
      return false;
    }
  }

  cert_list->push_back(std::move(cert_with_constraints_mojo));
  return true;
}
#endif

}  // namespace

ProfileNetworkContextService::ProfileNetworkContextService(Profile* profile)
    : profile_(profile), proxy_config_monitor_(profile) {
  TRACE_EVENT0("startup", "ProfileNetworkContextService::ctor");
  PrefService* profile_prefs = profile->GetPrefs();
  quic_allowed_.Init(prefs::kQuicAllowed, profile_prefs,
                     base::BindRepeating(
                         &ProfileNetworkContextService::DisableQuicIfNotAllowed,
                         base::Unretained(this)));
  pref_accept_language_.Init(
      language::prefs::kAcceptLanguages, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateAcceptLanguage,
                          base::Unretained(this)));
  enable_referrers_.Init(
      prefs::kEnableReferrers, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateReferrersEnabled,
                          base::Unretained(this)));
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  cookie_settings_observation_.Observe(cookie_settings_.get());

  DisableQuicIfNotAllowed();

  // Observe content settings so they can be synced to the network service.
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  pref_change_registrar_.Init(profile_prefs);

  // When any of the following CT preferences change, we schedule an update
  // to aggregate the actual update using a |ct_policy_update_timer_|.
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedSPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  // When any of the following Certificate preferences change, we schedule an
  // update to aggregate the actual update using a |cert_policy_update_timer_|.
  base::RepeatingClosure schedule_update_cert_policy = base::BindRepeating(
      &ProfileNetworkContextService::ScheduleUpdateCertificatePolicy,
      base::Unretained(this));
  pref_change_registrar_.Add(prefs::kCACertificates,
                             schedule_update_cert_policy);
  pref_change_registrar_.Add(prefs::kCACertificatesWithConstraints,
                             schedule_update_cert_policy);
  pref_change_registrar_.Add(prefs::kCADistrustedCertificates,
                             schedule_update_cert_policy);
  pref_change_registrar_.Add(prefs::kCAHintCertificates,
                             schedule_update_cert_policy);
#if !BUILDFLAG(IS_CHROMEOS)
  pref_change_registrar_.Add(prefs::kCAPlatformIntegrationEnabled,
                             schedule_update_cert_policy);
#endif

  pref_change_registrar_.Add(
      prefs::kGloballyScopeHTTPAuthCacheEnabled,
      base::BindRepeating(&ProfileNetworkContextService::
                              UpdateSplitAuthCacheByNetworkIsolationKey,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kCorsNonWildcardRequestHeadersSupport,
      base::BindRepeating(&ProfileNetworkContextService::
                              UpdateCorsNonWildcardRequestHeadersSupport,
                          base::Unretained(this)));

#if BUILDFLAG(ENABLE_REPORTING)
  if (base::FeatureList::IsEnabled(
          net::features::kReportingApiEnableEnterpriseCookieIssues)) {
    pref_change_registrar_.Add(
        prefs::kReportingEndpoints,
        base::BindRepeating(
            &ProfileNetworkContextService::UpdateEnterpriseReportingEndpoints,
            base::Unretained(this)));
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

ProfileNetworkContextService::~ProfileNetworkContextService() = default;

void ProfileNetworkContextService::ConfigureNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ConfigureNetworkContextParamsInternal(in_memory, relative_partition_path,
                                        network_context_params,
                                        cert_verifier_creation_params);

  if ((!in_memory && !profile_->IsOffTheRecord())) {
    // TODO(jam): delete this code 1 year after Network Service shipped to all
    // stable users, which would be after M83 branches.
    base::FilePath base_cache_path;
    chrome::GetUserCacheDirectory(GetPartitionPath(relative_partition_path),
                                  &base_cache_path);
    base::FilePath media_cache_path =
        base_cache_path.Append(chrome::kMediaCacheDirname);
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::GetDeletePathRecursivelyCallback(media_cache_path));
  }
}

void ProfileNetworkContextService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(embedder_support::kAlternateErrorPagesEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kQuicAllowed, true);
  registry->RegisterBooleanPref(prefs::kGloballyScopeHTTPAuthCacheEnabled,
                                false);
  registry->RegisterListPref(prefs::kHSTSPolicyBypassList);
  registry->RegisterListPref(prefs::kCACertificates);
  registry->RegisterListPref(prefs::kCACertificatesWithConstraints);
  registry->RegisterListPref(prefs::kCADistrustedCertificates);
  registry->RegisterListPref(prefs::kCAHintCertificates);
#if !BUILDFLAG(IS_CHROMEOS)
  // Include user added platform certs by default.
  registry->RegisterBooleanPref(prefs::kCAPlatformIntegrationEnabled, true);
#endif
}

// static
void ProfileNetworkContextService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kAmbientAuthenticationInPrivateModesEnabled,
      static_cast<int>(net::AmbientAuthAllowedProfileTypes::kRegularOnly));

  // For information about whether to reset the HTTP Cache or not, defaults
  // to the empty string, which does not prompt a reset.
  registry->RegisterStringPref(kHttpCacheFinchExperimentGroups, "");
}

void ProfileNetworkContextService::DisableQuicIfNotAllowed() {
  if (!quic_allowed_.IsManaged())
    return;

  // If QUIC is allowed, do nothing (re-enabling QUIC is not supported).
  if (quic_allowed_.GetValue())
    return;

  g_browser_process->system_network_context_manager()->DisableQuic();
}

void ProfileNetworkContextService::UpdateAcceptLanguage() {
  const std::string accept_language = ComputeAcceptLanguage();
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->SetAcceptLanguage(
            accept_language);
      });
}

void ProfileNetworkContextService::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetCookieManagerForBrowserProcess()
            ->BlockThirdPartyCookies(block_third_party_cookies);
      });
}

void ProfileNetworkContextService::OnMitigationsEnabledFor3pcdChanged(
    bool enable) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetCookieManagerForBrowserProcess()
            ->SetMitigationsEnabledFor3pcd(enable);
      });
}

void ProfileNetworkContextService::OnTrackingProtectionEnabledFor3pcdChanged(
    bool enable) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetCookieManagerForBrowserProcess()
            ->SetTrackingProtectionEnabledFor3pcd(enable);
      });
}

std::string ProfileNetworkContextService::ComputeAcceptLanguage() const {
  // TODO:(https://crbug.com/40224802) Return only single language without
  // expanding the language list if the DisableReduceAcceptLanguage deprecation
  // trial ends.

  if (profile_->IsOffTheRecord()) {
    // In incognito mode return only the first language.
    return ComputeAcceptLanguageFromPref(
        language::GetFirstLanguage(pref_accept_language_.GetValue()));
  }
  return ComputeAcceptLanguageFromPref(pref_accept_language_.GetValue());
}

void ProfileNetworkContextService::UpdateReferrersEnabled() {
  const bool enable_referrers = enable_referrers_.GetValue();
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->SetEnableReferrers(
            enable_referrers);
      });
}

network::mojom::CTPolicyPtr ProfileNetworkContextService::GetCTPolicy() {
  auto* prefs = profile_->GetPrefs();
  const base::Value::List& ct_excluded =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedHosts);
  const base::Value::List& ct_excluded_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedSPKIs);

  std::vector<std::string> excluded(TranslateStringArray(ct_excluded));
  std::vector<std::string> excluded_spkis(
      TranslateStringArray(ct_excluded_spkis));

  return network::mojom::CTPolicy::New(std::move(excluded),
                                       std::move(excluded_spkis));
}

void ProfileNetworkContextService::UpdateCTPolicyForContexts(
    const std::vector<network::mojom::NetworkContext*>& contexts) {
  for (auto* context : contexts) {
    context->SetCTPolicy(GetCTPolicy());
  }
}

void ProfileNetworkContextService::UpdateCTPolicy() {
  std::vector<network::mojom::NetworkContext*> contexts;
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        contexts.push_back(storage_partition->GetNetworkContext());
      });

  UpdateCTPolicyForContexts(contexts);
}

void ProfileNetworkContextService::ScheduleUpdateCTPolicy() {
  ct_policy_update_timer_.Start(FROM_HERE, base::Seconds(0), this,
                                &ProfileNetworkContextService::UpdateCTPolicy);
}

cert_verifier::mojom::AdditionalCertificatesPtr
ProfileNetworkContextService::GetCertificatePolicy(
    const base::FilePath& storage_partition_path) {
  auto* prefs = profile_->GetPrefs();
  auto additional_certificates =
      cert_verifier::mojom::AdditionalCertificates::New();

#if BUILDFLAG(IS_CHROMEOS)
  const policy::PolicyCertService* policy_cert_service =
      policy::PolicyCertServiceFactory::GetForProfile(profile_);
  if (policy_cert_service) {
    net::CertificateList all_certificates;
    net::CertificateList trust_anchors;
    policy_cert_service->GetPolicyCertificatesForStoragePartition(
        storage_partition_path, &all_certificates, &trust_anchors);

    for (const auto& cert : all_certificates) {
      base::span<const uint8_t> cert_bytes =
          net::x509_util::CryptoBufferAsSpan(cert->cert_buffer());
      additional_certificates->all_certificates.push_back(
          std::vector<uint8_t>(cert_bytes.begin(), cert_bytes.end()));
    }
    for (const auto& cert : trust_anchors) {
      base::span<const uint8_t> cert_bytes =
          net::x509_util::CryptoBufferAsSpan(cert->cert_buffer());
      additional_certificates->trust_anchors.push_back(
          std::vector<uint8_t>(cert_bytes.begin(), cert_bytes.end()));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  for (const base::Value& cert_b64 :
       prefs->GetList(prefs::kCAHintCertificates)) {
    std::optional<std::vector<uint8_t>> decoded_opt =
        base::Base64Decode(cert_b64.GetString());

    if (decoded_opt.has_value()) {
      additional_certificates->all_certificates.push_back(
          std::move(*decoded_opt));
    }
  }

  for (const base::Value& cert_b64 : prefs->GetList(prefs::kCACertificates)) {
    std::optional<std::vector<uint8_t>> decoded_opt =
        base::Base64Decode(cert_b64.GetString());

    if (decoded_opt.has_value()) {
      additional_certificates->trust_anchors_with_enforced_constraints
          .push_back(std::move(*decoded_opt));
    }
  }

  // Add trust anchors with constraints outside the cert
  for (const base::Value& cert_with_constraints :
       prefs->GetList(prefs::kCACertificatesWithConstraints)) {
    const base::Value::Dict* cert_with_constraints_dict =
        cert_with_constraints.GetIfDict();
    if (!cert_with_constraints_dict) {
      continue;
    }

    const std::string* cert_b64 =
        cert_with_constraints_dict->FindString("certificate");
    const base::Value::Dict* constraints_dict =
        cert_with_constraints_dict->FindDict("constraints");
    if (!constraints_dict) {
      continue;
    }
    const base::Value::List* permitted_cidrs =
        constraints_dict->FindList("permitted_cidrs");
    const base::Value::List* permitted_dns_names =
        constraints_dict->FindList("permitted_dns_names");

    // Need to have a cert, and at least one set of restrictions.
    if (!cert_b64) {
      continue;
    }

    if (!((permitted_cidrs && permitted_cidrs->size() > 0) ||
          (permitted_dns_names && permitted_dns_names->size() > 0))) {
      continue;
    }

    std::optional<std::vector<uint8_t>> decoded_cert_opt =
        base::Base64Decode(*cert_b64);
    if (!decoded_cert_opt.has_value()) {
      // Cert isn't valid b64, continue.
      continue;
    }

    bool invalid_constraint = false;
    auto cert_with_constraints_mojo =
        cert_verifier::mojom::CertWithConstraints::New();
    cert_with_constraints_mojo->certificate = std::move(*decoded_cert_opt);
    if (permitted_dns_names) {
      for (const base::Value& dns_name : *permitted_dns_names) {
        if (dns_name.is_string() &&
            IsValidDNSConstraint(dns_name.GetString())) {
          cert_with_constraints_mojo->permitted_dns_names.push_back(
              dns_name.GetString());
        } else {
          invalid_constraint = true;
          break;
        }
      }
    }
    if (invalid_constraint) {
      continue;
    }

    if (permitted_cidrs) {
      for (const base::Value& cidr : *permitted_cidrs) {
        if (!cidr.is_string()) {
          invalid_constraint = true;
          break;
        }
        net::IPAddress parsed_cidr;
        net::IPAddress mask;
        if (ParseCIDRConstraint(cidr.GetString(), &parsed_cidr, &mask)) {
          cert_with_constraints_mojo->permitted_cidrs.push_back(
              cert_verifier::mojom::CIDR::New(/*ip=*/parsed_cidr,
                                              /*mask=*/mask));

        } else {
          invalid_constraint = true;
          break;
        }
      }
    }
    if (invalid_constraint) {
      continue;
    }

    additional_certificates->trust_anchors_with_additional_constraints
        .push_back(std::move(cert_with_constraints_mojo));
  }

  for (const base::Value& cert_b64 :
       prefs->GetList(prefs::kCADistrustedCertificates)) {
    std::string decoded;
    if (!base::Base64Decode(cert_b64.GetString(), &decoded)) {
      continue;
    }
    std::string_view spki_piece;
    bool success = net::asn1::ExtractSPKIFromDERCert(decoded, &spki_piece);
    if (success) {
      additional_certificates->distrusted_spkis.push_back(
          base::ToVector(base::as_byte_span(spki_piece)));
    }
  }

#if !BUILDFLAG(IS_CHROMEOS)
  additional_certificates->include_system_trust_store =
      prefs->GetBoolean(prefs::kCAPlatformIntegrationEnabled);
#endif

  return additional_certificates;
}

void ProfileNetworkContextService::UpdateAdditionalCertificates() {
#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  if (base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2Write)) {
    net::ServerCertificateDatabaseService* cert_db_service =
        net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
            profile_);

    cert_db_service->GetAllCertificates(
        base::BindOnce(&ProfileNetworkContextService::
                           UpdateAdditionalCertificatesWithUserAddedCerts,
                       base::Unretained(this)));
  } else {
    profile_->ForEachLoadedStoragePartition(
        [&](content::StoragePartition* storage_partition) {
          storage_partition->GetCertVerifierServiceUpdater()
              ->UpdateAdditionalCertificates(
                  GetCertificatePolicy(storage_partition->GetPath()));
        });
  }
#else
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetCertVerifierServiceUpdater()
            ->UpdateAdditionalCertificates(
                GetCertificatePolicy(storage_partition->GetPath()));
      });
#endif
}

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
void ProfileNetworkContextService::
    UpdateAdditionalCertificatesWithUserAddedCerts(
        std::vector<net::ServerCertificateDatabase::CertInformation>
            cert_infos) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        cert_verifier::mojom::AdditionalCertificatesPtr additional_certs =
            GetCertificatePolicy(storage_partition->GetPath());

        for (const auto& cert_info : cert_infos) {
          std::optional<bssl::CertificateTrustType> trust =
              net::ServerCertificateDatabase::GetUserCertificateTrust(
                  cert_info);
          if (!trust) {
            continue;
          }
          switch (trust.value()) {
            case bssl::CertificateTrustType::UNSPECIFIED:
              additional_certs->all_certificates.push_back(cert_info.der_cert);
              break;

            case bssl::CertificateTrustType::DISTRUSTED: {
              std::string_view spki_piece;
              bool success = net::asn1::ExtractSPKIFromDERCert(
                  base::as_string_view(cert_info.der_cert), &spki_piece);
              if (success) {
                additional_certs->distrusted_spkis.push_back(
                    base::ToVector(base::as_byte_span(spki_piece)));
              }
              break;
            }

            case bssl::CertificateTrustType::TRUSTED_ANCHOR:
              if (!cert_info.cert_metadata.has_constraints() ||
                  (cert_info.cert_metadata.constraints().dns_names_size() ==
                       0 &&
                   cert_info.cert_metadata.constraints().cidrs_size() == 0)) {
                additional_certs->trust_anchors_with_enforced_constraints
                    .push_back(cert_info.der_cert);
              } else {
                MaybeAddCertWithConstraints(
                    cert_info,
                    &additional_certs
                         ->trust_anchors_with_additional_constraints);
              }
              break;

            case bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
              MaybeAddCertWithConstraints(
                  cert_info, &additional_certs->trust_anchors_and_leafs);
              break;
            case bssl::CertificateTrustType::TRUSTED_LEAF:
              MaybeAddCertWithConstraints(cert_info,
                                          &additional_certs->trust_leafs);
              break;
          }
        }
        storage_partition->GetCertVerifierServiceUpdater()
            ->UpdateAdditionalCertificates(std::move(additional_certs));
      });
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

void ProfileNetworkContextService::ScheduleUpdateCertificatePolicy() {
  cert_policy_update_timer_.Start(
      FROM_HERE, base::Seconds(0), this,
      &ProfileNetworkContextService::UpdateAdditionalCertificates);
}

ProfileNetworkContextService::CertificatePoliciesForView::
    CertificatePoliciesForView() = default;
ProfileNetworkContextService::CertificatePoliciesForView::
    ~CertificatePoliciesForView() = default;

ProfileNetworkContextService::CertificatePoliciesForView::
    CertificatePoliciesForView(CertificatePoliciesForView&&) = default;
ProfileNetworkContextService::CertificatePoliciesForView&
ProfileNetworkContextService::CertificatePoliciesForView::operator=(
    CertificatePoliciesForView&& other) = default;

ProfileNetworkContextService::CertificatePoliciesForView
ProfileNetworkContextService::GetCertificatePolicyForView() {
  CertificatePoliciesForView policies;
  policies.certificate_policies =
      GetCertificatePolicy(profile_->GetDefaultStoragePartition()->GetPath());

  auto* prefs = profile_->GetPrefs();
  for (const base::Value& cert_b64 :
       prefs->GetList(prefs::kCADistrustedCertificates)) {
    std::optional<std::vector<uint8_t>> decoded_opt =
        base::Base64Decode(cert_b64.GetString());

    if (decoded_opt.has_value()) {
      policies.full_distrusted_certs.push_back(std::move(*decoded_opt));
    }
  }

#if !BUILDFLAG(IS_CHROMEOS)
  policies.is_include_system_trust_store_managed =
      prefs->FindPreference(prefs::kCAPlatformIntegrationEnabled)->IsManaged();
#endif
  return policies;
}

bool ProfileNetworkContextService::ShouldSplitAuthCacheByNetworkIsolationKey()
    const {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kGloballyScopeHTTPAuthCacheEnabled))
    return false;
  return base::FeatureList::IsEnabled(
      network::features::kSplitAuthCacheByNetworkIsolationKey);
}

void ProfileNetworkContextService::UpdateSplitAuthCacheByNetworkIsolationKey() {
  const bool split_auth_cache_by_network_isolation_key =
      ShouldSplitAuthCacheByNetworkIsolationKey();

  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()
            ->SetSplitAuthCacheByNetworkAnonymizationKey(
                split_auth_cache_by_network_isolation_key);
      });
}

void ProfileNetworkContextService::
    UpdateCorsNonWildcardRequestHeadersSupport() {
  const bool value = profile_->GetPrefs()->GetBoolean(
      prefs::kCorsNonWildcardRequestHeadersSupport);

  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()
            ->SetCorsNonWildcardRequestHeadersSupport(value);
      });
}

#if BUILDFLAG(ENABLE_REPORTING)
base::flat_map<std::string, GURL>
ProfileNetworkContextService::GetEnterpriseReportingEndpoints() const {
  using FlatMap = base::flat_map<std::string, GURL>;
  // Create the underlying container first to allow sorting to
  // be done in a single pass.
  FlatMap::container_type pairs;
  const base::Value::Dict& pref_dict =
      profile_->GetPrefs()->GetDict(prefs::kReportingEndpoints);
  pairs.reserve(pref_dict.size());
  // The iterator for base::Value::Dict returns a temporary value when
  // dereferenced, so a const reference is not used below.
  for (const auto [endpoint_name, endpoint_url] : pref_dict) {
    GURL endpoint(endpoint_url.GetString());
    if (endpoint.is_valid() && endpoint.SchemeIsCryptographic()) {
      pairs.emplace_back(endpoint_name, std::move(endpoint));
    }
  }
  return FlatMap(std::move(pairs));
}

void ProfileNetworkContextService::UpdateEnterpriseReportingEndpoints() {
  base::flat_map<std::string, GURL> endpoints =
      GetEnterpriseReportingEndpoints();
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->SetEnterpriseReportingEndpoints(
            endpoints);
      });
}
#endif

// static
network::mojom::CookieManagerParamsPtr
ProfileNetworkContextService::CreateCookieManagerParams(
    Profile* profile,
    const content_settings::CookieSettings& cookie_settings) {
  auto out = network::mojom::CookieManagerParams::New();
  out->block_third_party_cookies =
      cookie_settings.ShouldBlockThirdPartyCookies();
  // This allows cookies to be sent on https requests from chrome:// pages,
  // ignoring SameSite attribute rules. For example, this is needed for browser
  // UI to interact with SameSite cookies on accounts.google.com, which is used
  // for displaying a list of available accounts on the NTP
  // (chrome://new-tab-page), etc.
  out->secure_origin_cookies_allowed_schemes.push_back(
      content::kChromeUIScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(chlily): To be consistent with the content_settings version of
  // CookieSettings, we should probably also add kExtensionScheme to the list of
  // matching_scheme_cookies_allowed_schemes.
  out->third_party_cookies_allowed_schemes.push_back(
      extensions::kExtensionScheme);
  out->third_party_cookies_allowed_schemes.push_back(
      content::kChromeDevToolsScheme);
#endif

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  for (auto type :
       content_settings::CookieSettings::GetContentSettingsTypes()) {
    if (!IsContentSettingsTypeEnabled(type)) {
      continue;
    }
    if (type == ContentSettingsType::FEDERATED_IDENTITY_SHARING) {
      if (FederatedIdentityPermissionContext* fedcm_context =
              FederatedIdentityPermissionContextFactory::GetForProfile(profile);
          fedcm_context) {
        out->content_settings[type] =
            fedcm_context->GetSharingPermissionGrantsAsContentSettings();
      } else {
        out->content_settings[type] = ContentSettingsForOneType();
      }
    } else {
      out->content_settings[type] =
          host_content_settings_map->GetSettingsForOneType(type);
    }
  }

  out->cookie_access_delegate_type =
      network::mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS;

  out->mitigations_enabled_for_3pcd =
      cookie_settings.MitigationsEnabledFor3pcd();

  out->tracking_protection_enabled_for_3pcd =
      TrackingProtectionSettingsFactory::GetForProfile(profile)
          ->IsTrackingProtection3pcdEnabled();

  return out;
}

void ProfileNetworkContextService::FlushCachedClientCertIfNeeded(
    const net::HostPortPair& host,
    const scoped_refptr<net::X509Certificate>& certificate) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->FlushCachedClientCertIfNeeded(
            host, certificate);
      });
}

void ProfileNetworkContextService::FlushMatchingCachedClientCert(
    const scoped_refptr<net::X509Certificate>& certificate) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->FlushMatchingCachedClientCert(
            certificate);
      });
}

void ProfileNetworkContextService::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

void ProfileNetworkContextService::SetDiscardDomainReliabilityUploadsForTesting(
    bool value) {
  g_discard_domain_reliability_uploads_for_testing = new bool(value);
}

std::unique_ptr<net::ClientCertStore>
ProfileNetworkContextService::CreateClientCertStore() {
  if (!client_cert_store_factory_.is_null())
    return client_cert_store_factory_.Run();

#if BUILDFLAG(IS_CHROMEOS)
  chromeos::CertificateProviderService* cert_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          profile_);
  std::unique_ptr<chromeos::CertificateProvider> certificate_provider;
  if (cert_provider_service) {
    certificate_provider = cert_provider_service->CreateCertificateProvider();
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool use_system_key_slot = false;
  // Enable client certificates for the Chrome OS sign-in frame, if this feature
  // is not disabled by a flag.
  // Note that while this applies to the whole sign-in profile / lock screen
  // profile, client certificates will only be selected for the StoragePartition
  // currently used in the sign-in frame (see SigninPartitionManager).
  if (ash::switches::IsSigninFrameClientCertsEnabled() &&
      (ash::ProfileHelper::IsSigninProfile(profile_) ||
       ash::ProfileHelper::IsLockScreenProfile(profile_))) {
    use_system_key_slot = true;
  }

  if (ash::features::ShouldUseKcerClientCertStore()) {
    return std::make_unique<ash::ClientCertStoreKcer>(
        std::move(certificate_provider),
        kcer::KcerFactoryAsh::GetKcer(profile_));
  } else {
    std::string username_hash;
    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user && !user->username_hash().empty()) {
      username_hash = user->username_hash();

      // Use the device-wide system key slot only if the user is affiliated on
      // the device.
      if (user->IsAffiliated()) {
        use_system_key_slot = true;
      }
    }

    return std::make_unique<ash::ClientCertStoreAsh>(
        std::move(certificate_provider), use_system_key_slot, username_hash,
        base::BindRepeating(&CreateCryptoModuleBlockingPasswordDelegate,
                            kCryptoModulePasswordClientAuth));
  }

#elif BUILDFLAG(USE_NSS_CERTS)
  std::unique_ptr<net::ClientCertStore> store =
      std::make_unique<net::ClientCertStoreNSS>(
          base::BindRepeating(&CreateCryptoModuleBlockingPasswordDelegate,
                              kCryptoModulePasswordClientAuth));
#if BUILDFLAG(IS_CHROMEOS_LACROS)

  if (!Profile::FromBrowserContext(
           GetBrowserContextRedirectedInIncognito(profile_))
           ->IsMainProfile()) {
    // TODO(crbug.com/40156976): At the moment client certs are only enabled for
    // the main profile and its incognito profile (similarly to how it worked in
    // Ash-Chrome). Return some cert store for secondary profiles in
    // Lacros-Chrome when certs are supported there.
    return nullptr;
  }

  CertDbInitializer* cert_db_initializer =
      CertDbInitializerFactory::GetForBrowserContext(profile_);
  store = std::make_unique<ClientCertStoreLacros>(
      std::move(certificate_provider), cert_db_initializer, std::move(store));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_LINUX)
  return GetWrappedCertStore(profile_, std::move(store));
#else
  return store;
#endif  // BUILDFLAG(IS_LINUX)
#elif BUILDFLAG(IS_WIN)
  return GetWrappedCertStore(profile_,
                             std::make_unique<net::ClientCertStoreWin>());
#elif BUILDFLAG(IS_MAC)
  return GetWrappedCertStore(profile_,
                             std::make_unique<net::ClientCertStoreMac>());
#elif BUILDFLAG(IS_ANDROID)
  // Android does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return nullptr;
#else
#error Unknown platform.
#endif
}

bool GetHttpCacheBackendResetParam(PrefService* local_state) {
  // Get the field trial groups.  If the server cannot be reached, then
  // this corresponds to "None" for each experiment.
  base::FieldTrial* field_trial = base::FeatureList::GetFieldTrial(
      net::features::kSplitCacheByNetworkIsolationKey);
  std::string current_field_trial_status =
      (field_trial ? field_trial->group_name() : "None");
  // This used to be used for keying on main frame only vs main frame +
  // innermost frame, but the feature was removed, and now it's always keyed on
  // both.
  current_field_trial_status += " None";
  // This used to be for keying on scheme + eTLD+1 vs origin, but the trial was
  // removed, and now it's always keyed on eTLD+1. Still keeping a third "None"
  // to avoid resetting the disk cache.
  current_field_trial_status += " None ";

  field_trial = base::FeatureList::GetFieldTrial(
      net::features::kSplitCacheByIncludeCredentials);
  current_field_trial_status +=
      (field_trial ? field_trial->group_name() : "None");

  // For the HTTP Cache keying experiments, if a flag indicates that the user is
  // in an experiment group, modify `current_field_trial_status` to ensure that
  // the cache gets cleared. If the user is not a part of the experiment, don't
  // make any changes so as not to invalidate the existing cache.
  if (base::FeatureList::IsEnabled(
          net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean)) {
    current_field_trial_status += " 20240814-CrossSiteNavBool";
  } else if (base::FeatureList::IsEnabled(
                 net::features::kSplitCacheByMainFrameNavigationInitiator)) {
    current_field_trial_status += " 20240814-MainFrameNavigationInitiator";
  } else if (base::FeatureList::IsEnabled(
                 net::features::kSplitCacheByNavigationInitiator)) {
    current_field_trial_status += " 20240814-NavigationInitiator";
  } else if (base::FeatureList::IsEnabled(
                 net::features::kHttpCacheKeyingExperimentControlGroup2024)) {
    current_field_trial_status += " 20240814-ExperimentControlGroup";
  }

  if (disk_cache::InBackendExperiment()) {
    if (disk_cache::InSimpleBackendExperimentGroup()) {
      current_field_trial_status += " 20241007-DiskCache-Simple";
    } else {
      current_field_trial_status += " 20241007-DiskCache-Blockfile";
    }
  }

  std::string previous_field_trial_status =
      local_state->GetString(kHttpCacheFinchExperimentGroups);
  local_state->SetString(kHttpCacheFinchExperimentGroups,
                         current_field_trial_status);

  return !previous_field_trial_status.empty() &&
         current_field_trial_status != previous_field_trial_status;
}

void ProfileNetworkContextService::ConfigureNetworkContextParamsInternal(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  TRACE_EVENT0(
      "startup",
      "ProfileNetworkContextService::ConfigureNetworkContextParamsInternal");
  if (profile_->IsOffTheRecord())
    in_memory = true;
  base::FilePath path(GetPartitionPath(relative_partition_path));

  g_browser_process->system_network_context_manager()
      ->ConfigureDefaultNetworkContextParams(network_context_params);

  network_context_params->enable_zstd =
      base::FeatureList::IsEnabled(net::features::kZstdContentEncoding) &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kZstdContentEncodingEnabled);
  network_context_params->accept_language = ComputeAcceptLanguage();
  network_context_params->enable_referrers = enable_referrers_.GetValue();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(embedder_support::kShortReportingDelay)) {
    network_context_params->reporting_delivery_interval =
        base::Milliseconds(100);
  }

  // Always enable the HTTP cache.
  network_context_params->http_cache_enabled = true;

  network_context_params->http_auth_static_network_context_params =
      network::mojom::HttpAuthStaticNetworkContextParams::New();

  if (IsAmbientAuthAllowedForProfile(profile_)) {
    network_context_params->http_auth_static_network_context_params
        ->allow_default_credentials =
        net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS;
  } else {
    network_context_params->http_auth_static_network_context_params
        ->allow_default_credentials =
        net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS;
  }

  network_context_params->cookie_manager_params =
      CreateCookieManagerParams(profile_, *cookie_settings_);

  // Configure on-disk storage for non-OTR profiles. OTR profiles just use
  // default behavior (in memory storage, default sizes).
  if (!in_memory) {
    PrefService* local_state = g_browser_process->local_state();
    // Configure the HTTP cache path and size.
    base::FilePath base_cache_path;
    chrome::GetUserCacheDirectory(path, &base_cache_path);
    base::FilePath disk_cache_dir =
        local_state->GetFilePath(prefs::kDiskCacheDir);
    if (!disk_cache_dir.empty())
      base_cache_path = disk_cache_dir.Append(base_cache_path.BaseName());
    const int disk_cache_size = local_state->GetInteger(prefs::kDiskCacheSize);
    network_context_params->http_cache_max_size = disk_cache_size;
    network_context_params->shared_dictionary_cache_max_size = disk_cache_size;

    network_context_params->file_paths =
        ::network::mojom::NetworkContextFilePaths::New();

    network_context_params->file_paths->http_cache_directory =
        base_cache_path.Append(chrome::kCacheDirname);
    network_context_params->file_paths->data_directory =
        path.Append(chrome::kNetworkDataDirname);
    network_context_params->file_paths->unsandboxed_data_path = path;
    network_context_params->file_paths->trigger_migration =
        base::FeatureList::IsEnabled(features::kTriggerNetworkDataMigration);

    // Currently this just contains HttpServerProperties, but that will likely
    // change.
    network_context_params->file_paths->http_server_properties_file_name =
        base::FilePath(chrome::kNetworkPersistentStateFilename);
    network_context_params->file_paths->cookie_database_name =
        base::FilePath(chrome::kCookieFilename);

#if BUILDFLAG(IS_WIN)
    // If this feature is enabled, then the cookie database used by this profile
    // will be locked for exclusive access by sqlite3 implementation in the
    // network service.
    network_context_params->enable_locking_cookie_database =
        base::FeatureList::IsEnabled(features::kLockProfileCookieDatabase);
#endif  // BUILDFLAG(IS_WIN)

    g_browser_process->system_network_context_manager()
        ->AddCookieEncryptionManagerToNetworkContextParams(
            network_context_params);

    network_context_params->file_paths->trust_token_database_name =
        base::FilePath(chrome::kTrustTokenFilename);

#if BUILDFLAG(ENABLE_REPORTING)
    network_context_params->file_paths->reporting_and_nel_store_database_name =
        base::FilePath(chrome::kReportingAndNelStoreFilename);

    if (base::FeatureList::IsEnabled(
            net::features::kReportingApiEnableEnterpriseCookieIssues)) {
      network_context_params->enterprise_reporting_endpoints =
          GetEnterpriseReportingEndpoints();
    }
#endif  // BUILDFLAG(ENABLE_REPORTING)

    if (relative_partition_path.empty()) {  // This is the main partition.
      network_context_params->restore_old_session_cookies =
          profile_->ShouldRestoreOldSessionCookies();
      network_context_params->persist_session_cookies =
          profile_->ShouldPersistSessionCookies();
    } else {
      // Copy behavior of ProfileImplIOData::InitializeAppRequestContext.
      network_context_params->restore_old_session_cookies = false;
      network_context_params->persist_session_cookies = false;
    }

    network_context_params->file_paths->transport_security_persister_file_name =
        base::FilePath(chrome::kTransportSecurityPersisterFilename);
    network_context_params->file_paths->sct_auditing_pending_reports_file_name =
        base::FilePath(chrome::kSCTAuditingPendingReportsFileName);
  }
  const base::Value::List& hsts_policy_bypass_list =
      profile_->GetPrefs()->GetList(prefs::kHSTSPolicyBypassList);
  for (const auto& value : hsts_policy_bypass_list) {
    const std::string* string_value = value.GetIfString();
    if (!string_value)
      continue;
    network_context_params->hsts_policy_bypass_list.push_back(*string_value);
  }

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params);

  network_context_params->enable_certificate_reporting = true;

  SCTReportingService* sct_reporting_service =
      SCTReportingServiceFactory::GetForBrowserContext(profile_);
  if (sct_reporting_service) {
    network_context_params->sct_auditing_mode =
        sct_reporting_service->GetReportingMode();
  } else {
    network_context_params->sct_auditing_mode =
        network::mojom::SCTAuditingMode::kDisabled;
  }

  network_context_params->ct_policy = GetCTPolicy();

  if (domain_reliability::ShouldCreateService()) {
    network_context_params->enable_domain_reliability = true;
    network_context_params->domain_reliability_upload_reporter =
        domain_reliability::kUploadReporterString;
    network_context_params->discard_domain_reliablity_uploads =
        g_discard_domain_reliability_uploads_for_testing
            ? *g_discard_domain_reliability_uploads_for_testing
            : !g_browser_process->local_state()->GetBoolean(
                  metrics::prefs::kMetricsReportingEnabled);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Configure cert verifier to use the same software NSS database as Chrome is
  // currently using (secondary profiles don't have their own databases at the
  // moment).
  cert_verifier_creation_params->nss_full_path.reset();
  if (profile_->IsMainProfile()) {
    const crosapi::mojom::DefaultPathsPtr& default_paths =
        chromeos::BrowserParamsProxy::Get()->DefaultPaths();
    // `default_paths` can be nullptr in tests.
    if (!default_paths) {
      CHECK_IS_TEST();
    }
    // Populating `nss_full_path` will make cert verifier load
    // and use the corresponding NSS public slot. Kiosk sessions don't have
    // the UI that could result in interactions with the public slot. Kiosk
    // users are also not owner users and can't have the owner key in the
    // public slot. Leaving it empty will make cert verifier ignore the
    // public slot. This is done mainly because Chrome sometimes fails to
    // load the public slot and has to crash because of that.
    if (default_paths && default_paths->user_nss_database.has_value() &&
        !chromeos::IsKioskSession()) {
      cert_verifier_creation_params->nss_full_path =
          default_paths->user_nss_database.value();
    }
  }

  policy::PolicyCertServiceFactory::CreateAndStartObservingForProfile(profile_);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool profile_supports_policy_certs = false;
  if (ash::ProfileHelper::IsSigninProfile(profile_) ||
      ash::ProfileHelper::IsLockScreenProfile(profile_)) {
    profile_supports_policy_certs = true;
  }
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    // No need to initialize NSS for users with empty username hash:
    // Getters for a user's NSS slots always return NULL slot if the user's
    // username hash is empty, even when the NSS is not initialized for the
    // user.
    if (user && !user->username_hash().empty()) {
      // Populating `username_hash` and `nss_path` will make cert verifier load
      // and use the corresponding NSS public slot. Kiosk sessions don't have
      // the UI that could result in interactions with the public slot. Kiosk
      // users are also not owner users and can't have the owner key in the
      // public slot. Leaving them empty will make cert verifier ignore the
      // public slot. This is done mainly because Chrome sometimes fails to
      // load the public slot and has to crash because of that.
      if (!chromeos::IsKioskSession()) {
        cert_verifier_creation_params->username_hash = user->username_hash();
        cert_verifier_creation_params->nss_path = profile_->GetPath();
      }
      profile_supports_policy_certs = true;
    }
  }
  if (profile_supports_policy_certs) {
    policy::PolicyCertServiceFactory::CreateAndStartObservingForProfile(
        profile_);
  }
#endif

  // TODO(crbug.com/40928765): check to see if IsManaged() ensures the pref
  // isn't set in user profiles, or if that does something else. If that's true,
  // add an isManaged() check here.

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  if (base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2Write)) {
    cert_verifier_creation_params->wait_for_update = true;
    UpdateAdditionalCertificates();
  } else {
    cert_verifier_creation_params->initial_additional_certificates =
        GetCertificatePolicy(GetPartitionPath(relative_partition_path));
  }
#else
  cert_verifier_creation_params->initial_additional_certificates =
      GetCertificatePolicy(GetPartitionPath(relative_partition_path));
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_CHROMEOS)
  // Disable idle sockets close on memory pressure if configured by finch or
  // about://flags.
  if (base::FeatureList::IsEnabled(
          chromeos::features::kDisableIdleSocketsCloseOnMemoryPressure)) {
    network_context_params->disable_idle_sockets_close_on_memory_pressure =
        true;
  }
#endif

  network_context_params->reset_http_cache_backend =
      GetHttpCacheBackendResetParam(g_browser_process->local_state());

  network_context_params->split_auth_cache_by_network_anonymization_key =
      ShouldSplitAuthCacheByNetworkIsolationKey();

  // All consumers of the main NetworkContext must provide
  // NetworkAnonymizationKeys / IsolationInfos, so storage can be isolated on a
  // per-site basis.
  network_context_params->require_network_anonymization_key = true;

  ContentSetting anti_abuse_content_setting =
      HostContentSettingsMapFactory::GetForProfile(profile_)
          ->GetDefaultContentSetting(ContentSettingsType::ANTI_ABUSE, nullptr);
  network_context_params->block_trust_tokens =
      anti_abuse_content_setting == CONTENT_SETTING_BLOCK;

  network_context_params->first_party_sets_access_delegate_params =
      network::mojom::FirstPartySetsAccessDelegateParams::New();
  network_context_params->first_party_sets_access_delegate_params->enabled =
      PrivacySandboxSettingsFactory::GetForProfile(profile_)
          ->AreRelatedWebsiteSetsEnabled();

  mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
      fps_access_delegate_remote;
  network_context_params->first_party_sets_access_delegate_receiver =
      fps_access_delegate_remote.BindNewPipeAndPassReceiver();

  first_party_sets::FirstPartySetsPolicyService* fps_service =
      first_party_sets::FirstPartySetsPolicyServiceFactory::
          GetForBrowserContext(profile_);
  DCHECK(fps_service);
  fps_service->AddRemoteAccessDelegate(std::move(fps_access_delegate_remote));

  network_context_params->acam_preflight_spec_conformant =
      profile_->GetPrefs()->GetBoolean(
          prefs::kAccessControlAllowMethodsInCORSPreflightSpecConformant);

  IpProtectionCoreHost* ipp_core_host = IpProtectionCoreHost::Get(profile_);
  if (ipp_core_host) {
    ipp_core_host->AddNetworkService(
        network_context_params->ip_protection_config_getter
            .InitWithNewPipeAndPassReceiver(),
        network_context_params->ip_protection_control
            .InitWithNewPipeAndPassRemote());
    network_context_params->enable_ip_protection =
        ipp_core_host->IsIpProtectionEnabled();
  }

  network_context_params->device_bound_sessions_enabled =
      base::FeatureList::IsEnabled(net::features::kDeviceBoundSessions);
}

base::FilePath ProfileNetworkContextService::GetPartitionPath(
    const base::FilePath& relative_partition_path) {
  base::FilePath path = profile_->GetPath();
  if (!relative_partition_path.empty())
    path = path.Append(relative_partition_path);
  return path;
}

void ProfileNetworkContextService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  switch (content_type) {
    case ContentSettingsType::ANTI_ABUSE:
      UpdateAntiAbuseSettings(profile_);
      break;
    case ContentSettingsType::DEFAULT:
      UpdateAntiAbuseSettings(profile_);
      for (auto type :
           content_settings::CookieSettings::GetContentSettingsTypes()) {
        UpdateCookieSettings(profile_, type);
      }
      break;
    default:
      if (content_settings::CookieSettings::GetContentSettingsTypes().contains(
              content_type)) {
        UpdateCookieSettings(profile_, content_type);
        return;
      }
      return;
  }
}
