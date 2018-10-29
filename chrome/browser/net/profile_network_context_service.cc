// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "net/http/http_util.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/features.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace {

std::vector<std::string> TranslateStringArray(const base::ListValue* list) {
  std::vector<std::string> strings;
  for (const base::Value& value : *list) {
    DCHECK(value.is_string());
    strings.push_back(value.GetString());
  }
  return strings;
}

std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      base::FeatureList::IsEnabled(features::kUseNewAcceptLanguageHeader)
          ? net::HttpUtil::ExpandLanguageList(language_pref)
          : language_pref;
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

}  // namespace

ProfileNetworkContextService::ProfileNetworkContextService(Profile* profile)
    : profile_(profile), proxy_config_monitor_(profile) {
  PrefService* profile_prefs = profile->GetPrefs();
  quic_allowed_.Init(
      prefs::kQuicAllowed, profile_prefs,
      base::Bind(&ProfileNetworkContextService::DisableQuicIfNotAllowed,
                 base::Unretained(this)));
  pref_accept_language_.Init(
      prefs::kAcceptLanguages, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateAcceptLanguage,
                          base::Unretained(this)));
  enable_referrers_.Init(
      prefs::kEnableReferrers, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateReferrersEnabled,
                          base::Unretained(this)));
  block_third_party_cookies_.Init(
      prefs::kBlockThirdPartyCookies, profile_prefs,
      base::BindRepeating(
          &ProfileNetworkContextService::UpdateBlockThirdPartyCookies,
          base::Unretained(this)));
  DisableQuicIfNotAllowed();

  // Observe content settings so they can be synced to the network service.
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  pref_change_registrar_.Init(profile_prefs);

  // When any of the following CT preferences change, we schedule an update
  // to aggregate the actual update using a |ct_policy_update_timer_|.
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTRequiredHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedSPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedLegacySPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
}

ProfileNetworkContextService::~ProfileNetworkContextService() {}

network::mojom::NetworkContextPtr
ProfileNetworkContextService::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  network::mojom::NetworkContextPtr network_context;
  PartitionInfo partition_info(in_memory, relative_partition_path);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    content::GetNetworkService()->CreateNetworkContext(
        MakeRequest(&network_context),
        CreateNetworkContextParams(in_memory, relative_partition_path));
  } else {
    // The corresponding |profile_io_data_network_contexts_| may already be
    // initialized if SetUpProfileIODataNetworkContext was called first.
    auto iter = profile_io_data_network_contexts_.find(partition_info);
    if (iter == profile_io_data_network_contexts_.end()) {
      // If this is not the main network context, then this method is expected
      // to be called after the URLRequestContext is configured.
      DCHECK(relative_partition_path.empty());
      // If the NetworkContext has not been requested yet, go ahead and create a
      // request for it.
      profile_io_data_context_requests_[partition_info] =
          mojo::MakeRequest(&network_context);
    } else {
      network_context = std::move(iter->second);
      // This is not strictly necessary, since the network service can't crash,
      // and NetworkContexts can't be destroyed without destroying the profile.
      profile_io_data_network_contexts_.erase(iter);
    }
  }

  std::vector<network::mojom::NetworkContext*> contexts{network_context.get()};
  UpdateCTPolicyForContexts(contexts);

  return network_context;
}

void ProfileNetworkContextService::SetUpProfileIODataNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextRequest* network_context_request,
    network::mojom::NetworkContextParamsPtr* network_context_params) {
  DCHECK(network_context_request);
  DCHECK(network_context_params);

  PartitionInfo partition_info(in_memory, relative_partition_path);

  // This may be called either before or after CreateNetworkContext().
  auto iter = profile_io_data_context_requests_.find(partition_info);
  if (iter == profile_io_data_context_requests_.end()) {
    DCHECK(profile_io_data_network_contexts_.find(partition_info) ==
           profile_io_data_network_contexts_.end());
    *network_context_request =
        mojo::MakeRequest(&profile_io_data_network_contexts_[partition_info]);
  } else {
    DCHECK(relative_partition_path.empty());

    *network_context_request = std::move(iter->second);
    // Not strictly necessary, since this should only be called once per storage
    // partition.
    profile_io_data_context_requests_.erase(iter);
  }

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    *network_context_params =
        CreateNetworkContextParams(in_memory, relative_partition_path);
    return;
  }

  // Just use default if network service is enabled, to avoid the legacy
  // in-process URLRequestContext from fighting with the NetworkService over
  // ownership of on-disk files.
  *network_context_params = network::mojom::NetworkContextParams::New();
}

#if defined(OS_CHROMEOS)
void ProfileNetworkContextService::UpdateTrustAnchors(
    const net::CertificateList& trust_anchors) {
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](const net::CertificateList& trust_anchors,
             content::StoragePartition* storage_partition) {
            storage_partition->GetNetworkContext()->UpdateTrustAnchors(
                trust_anchors);
          },
          trust_anchors));
}
#endif

void ProfileNetworkContextService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kQuicAllowed, true);
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
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](const std::string& accept_language,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetNetworkContext()->SetAcceptLanguage(
                          accept_language);
                    },
                    ComputeAcceptLanguage()));
}

void ProfileNetworkContextService::UpdateBlockThirdPartyCookies() {
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](bool block_third_party_cookies,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetCookieManagerForBrowserProcess()
                          ->BlockThirdPartyCookies(block_third_party_cookies);
                    },
                    block_third_party_cookies_.GetValue()));
}

std::string ProfileNetworkContextService::ComputeAcceptLanguage() const {
  return ComputeAcceptLanguageFromPref(pref_accept_language_.GetValue());
}

void ProfileNetworkContextService::UpdateReferrersEnabled() {
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](bool enable_referrers,
             content::StoragePartition* storage_partition) {
            storage_partition->GetNetworkContext()->SetEnableReferrers(
                enable_referrers);
          },
          enable_referrers_.GetValue()));
}

void ProfileNetworkContextService::UpdateCTPolicyForContexts(
    const std::vector<network::mojom::NetworkContext*>& contexts) {
  auto* prefs = profile_->GetPrefs();
  const base::ListValue* ct_required =
      prefs->GetList(certificate_transparency::prefs::kCTRequiredHosts);
  const base::ListValue* ct_excluded =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedHosts);
  const base::ListValue* ct_excluded_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedSPKIs);
  const base::ListValue* ct_excluded_legacy_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedLegacySPKIs);

  std::vector<std::string> required(TranslateStringArray(ct_required));
  std::vector<std::string> excluded(TranslateStringArray(ct_excluded));
  std::vector<std::string> excluded_spkis(
      TranslateStringArray(ct_excluded_spkis));
  std::vector<std::string> excluded_legacy_spkis(
      TranslateStringArray(ct_excluded_legacy_spkis));

  for (auto* context : contexts) {
    context->SetCTPolicy(required, excluded, excluded_spkis,
                         excluded_legacy_spkis);
  }
}

void ProfileNetworkContextService::UpdateCTPolicy() {
  std::vector<network::mojom::NetworkContext*> contexts;
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](std::vector<network::mojom::NetworkContext*>* contexts_ptr,
             content::StoragePartition* storage_partition) {
            contexts_ptr->push_back(storage_partition->GetNetworkContext());
          },
          &contexts));

  UpdateCTPolicyForContexts(contexts);
}

void ProfileNetworkContextService::ScheduleUpdateCTPolicy() {
  ct_policy_update_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(0),
                                this,
                                &ProfileNetworkContextService::UpdateCTPolicy);
}

void ProfileNetworkContextService::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

network::mojom::NetworkContextParamsPtr
ProfileNetworkContextService::CreateNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  if (profile_->IsOffTheRecord())
    in_memory = true;
  base::FilePath path = profile_->GetPath();
  bool is_main_partition = relative_partition_path.empty();
  if (!is_main_partition)
    path = path.Append(relative_partition_path);

  network::mojom::NetworkContextParamsPtr network_context_params =
      g_browser_process->system_network_context_manager()
          ->CreateDefaultNetworkContextParams();

  network_context_params->context_name = std::string("main");

  network_context_params->accept_language = ComputeAcceptLanguage();
  network_context_params->enable_referrers = enable_referrers_.GetValue();

  // Always enable the HTTP cache.
  network_context_params->http_cache_enabled = true;

  network_context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();
  network_context_params->cookie_manager_params->block_third_party_cookies =
      block_third_party_cookies_.GetValue();
  network_context_params->cookie_manager_params
      ->secure_origin_cookies_allowed_schemes.push_back(
          content::kChromeUIScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    network_context_params->cookie_manager_params
        ->matching_scheme_cookies_allowed_schemes.push_back(
            extensions::kExtensionScheme);
  }
#endif

  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile_)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &settings);
  network_context_params->cookie_manager_params->settings = std::move(settings);

  // Configure on-disk storage for non-OTR profiles. OTR profiles just use
  // default behavior (in memory storage, default sizes).
  PrefService* prefs = profile_->GetPrefs();
  if (!in_memory) {
    // Configure the HTTP cache path and size.
    base::FilePath base_cache_path;
    chrome::GetUserCacheDirectory(path, &base_cache_path);
    base::FilePath disk_cache_dir = prefs->GetFilePath(prefs::kDiskCacheDir);
    if (!disk_cache_dir.empty())
      base_cache_path = disk_cache_dir.Append(base_cache_path.BaseName());
    network_context_params->http_cache_path =
        base_cache_path.Append(chrome::kCacheDirname);
    network_context_params->http_cache_max_size =
        prefs->GetInteger(prefs::kDiskCacheSize);

    // Currently this just contains HttpServerProperties, but that will likely
    // change.
    network_context_params->http_server_properties_path =
        path.Append(chrome::kNetworkPersistentStateFilename);

    base::FilePath cookie_path = path;
    cookie_path = cookie_path.Append(chrome::kCookieFilename);
    network_context_params->cookie_path = cookie_path;

    base::FilePath channel_id_path = path;
    channel_id_path = channel_id_path.Append(chrome::kChannelIDFilename);
    network_context_params->channel_id_path = channel_id_path;

    if (is_main_partition) {
      network_context_params->restore_old_session_cookies =
          profile_->ShouldRestoreOldSessionCookies();
      network_context_params->persist_session_cookies =
          profile_->ShouldPersistSessionCookies();
    } else {
      // Copy behavior of ProfileImplIOData::InitializeAppRequestContext.
      network_context_params->restore_old_session_cookies = false;
      network_context_params->persist_session_cookies = false;
    }

    network_context_params->transport_security_persister_path = path;
  }

  // NOTE(mmenke): Keep these protocol handlers and
  // ProfileIOData::SetUpJobFactoryDefaultsForBuilder in sync with
  // ProfileIOData::IsHandledProtocol().
  // TODO(mmenke): Find a better way of handling tracking supported schemes.
  network_context_params->enable_data_url_support = true;
  // File support is needed for PAC scripts that use file or data URLs.
  // TODO(crbug.com/839566): remove file support for all cases.
  // It is disabled with the network service as it is not responsible for
  // loading files.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    network_context_params->enable_file_url_support = true;
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support = true;
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  network_context_params->enable_certificate_reporting = true;
  network_context_params->enable_expect_ct_reporting = true;

  if (data_reduction_proxy::params::IsEnabledWithNetworkService()) {
    auto* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
    if (drp_settings) {
      network::mojom::CustomProxyConfigClientPtrInfo config_client_info;
      network_context_params->custom_proxy_config_client_request =
          mojo::MakeRequest(&config_client_info);
      drp_settings->SetCustomProxyConfigClient(std::move(config_client_info));
    }
  }

#if defined(OS_CHROMEOS)
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      user_manager &&
      policy::PolicyCertServiceFactory::CreateAndStartObservingForProfile(
          profile_)) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    // No need to initialize NSS for users with empty username hash:
    // Getters for a user's NSS slots always return NULL slot if the user's
    // username hash is empty, even when the NSS is not initialized for the
    // user.
    if (user && !user->username_hash().empty()) {
      network_context_params->username_hash = user->username_hash();
      network_context_params->nss_path = profile_->GetPath();

      policy::PolicyCertService* service =
          policy::PolicyCertServiceFactory::GetForProfile(profile_);
      network_context_params->initial_trust_anchors = service->trust_anchors();
    }
  }
#endif

  return network_context_params;
}

void ProfileNetworkContextService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES &&
      content_type != CONTENT_SETTINGS_TYPE_DEFAULT) {
    return;
  }

  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile_)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &settings);
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](ContentSettingsForOneType settings,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetCookieManagerForBrowserProcess()
                          ->SetContentSettings(settings);
                    },
                    settings));
}
