// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Assume that any proxy host ending with this suffix is a Data Reduction Proxy.
const char kDataReductionProxyDefaultHostSuffix[] = ".googlezip.net";

// Searches |proxy_list| for any Data Reduction Proxies, even if they don't
// match a currently configured Data Reduction Proxy.
bool ContainsDataReductionProxyDefaultHostSuffix(
    const net::ProxyList& proxy_list) {
  for (const net::ProxyServer& proxy : proxy_list.GetAll()) {
    if (proxy.is_valid() && !proxy.is_direct() &&
        base::EndsWith(proxy.host_port_pair().host(),
                       kDataReductionProxyDefaultHostSuffix,
                       base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

// Searches |proxy_rules| for any Data Reduction Proxies, even if they don't
// match a currently configured Data Reduction Proxy.
bool ContainsDataReductionProxyDefaultHostSuffix(
    const net::ProxyConfig::ProxyRules& proxy_rules) {
  return ContainsDataReductionProxyDefaultHostSuffix(
             proxy_rules.proxies_for_http) ||
         ContainsDataReductionProxyDefaultHostSuffix(
             proxy_rules.proxies_for_https);
}

// Extract the embedded PAC script from the given |pac_url|, and store the
// extracted script in |pac_script|. Returns true if extraction was successful,
// otherwise returns false. |pac_script| must not be NULL.
bool GetEmbeddedPacScript(base::StringPiece pac_url, std::string* pac_script) {
  DCHECK(pac_script);
  static const char kPacURLPrefix[] =
      "data:application/x-ns-proxy-autoconfig;base64,";
  return base::StartsWith(pac_url, kPacURLPrefix,
                          base::CompareCase::SENSITIVE) &&
         base::Base64Decode(pac_url.substr(base::size(kPacURLPrefix) - 1),
                            pac_script);
}

}  // namespace

// The Data Reduction Proxy has been turned into a "best effort" proxy,
// meaning it is used only if the effective proxy configuration resolves to
// DIRECT for a URL. It no longer can be a ProxyConfig in the proxy preference
// hierarchy. This method removes the Data Reduction Proxy configuration from
// prefs, if present. |proxy_pref_name| is the name of the proxy pref.
void DataReductionProxyChromeSettings::MigrateDataReductionProxyOffProxyPrefs(
    PrefService* prefs) {
  ProxyPrefMigrationResult proxy_pref_status =
      MigrateDataReductionProxyOffProxyPrefsHelper(prefs);
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.ProxyPrefMigrationResult",
                            proxy_pref_status,
                            DataReductionProxyChromeSettings::PROXY_PREF_MAX);
}

DataReductionProxyChromeSettings::ProxyPrefMigrationResult
DataReductionProxyChromeSettings::MigrateDataReductionProxyOffProxyPrefsHelper(
    PrefService* prefs) {
  base::DictionaryValue* dict = (base::DictionaryValue*)prefs->GetUserPrefValue(
      proxy_config::prefs::kProxy);
  if (!dict)
    return PROXY_PREF_NOT_CLEARED;

  // Clear empty "proxy" dictionary created by a bug. See http://crbug/448172.
  if (dict->empty()) {
    prefs->ClearPref(proxy_config::prefs::kProxy);
    return PROXY_PREF_CLEARED_EMPTY;
  }

  std::string mode;
  if (!dict->GetString("mode", &mode))
    return PROXY_PREF_NOT_CLEARED;
  // Clear "system" proxy entry since this is the default. This entry was
  // created by bug (http://crbug/448172).
  if (ProxyModeToString(ProxyPrefs::MODE_SYSTEM) == mode) {
    prefs->ClearPref(proxy_config::prefs::kProxy);
    return PROXY_PREF_CLEARED_MODE_SYSTEM;
  }

  // From M36 to M40, the DRP was configured using MODE_FIXED_SERVERS in the
  // proxy pref.
  if (ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS) == mode) {
    std::string proxy_server;
    if (!dict->GetString("server", &proxy_server))
      return PROXY_PREF_NOT_CLEARED;
    net::ProxyConfig::ProxyRules proxy_rules;
    proxy_rules.ParseFromString(proxy_server);
    // Clear the proxy pref if it matches a currently configured Data Reduction
    // Proxy, or if the proxy host ends with ".googlezip.net", in order to
    // ensure that any DRP in the pref is cleared even if the DRP configuration
    // was changed. See http://crbug.com/476610.
    ProxyPrefMigrationResult rv;
    if (Config()->ContainsDataReductionProxy(proxy_rules))
      rv = PROXY_PREF_CLEARED_DRP;
    else if (ContainsDataReductionProxyDefaultHostSuffix(proxy_rules))
      rv = PROXY_PREF_CLEARED_GOOGLEZIP;
    else
      return PROXY_PREF_NOT_CLEARED;

    prefs->ClearPref(proxy_config::prefs::kProxy);
    return rv;
  }

  // Before M35, the DRP was configured using a PAC script base64 encoded into a
  // PAC url.
  if (ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT) == mode) {
    std::string pac_url;
    std::string pac_script;
    if (!dict->GetString("pac_url", &pac_url) ||
        !GetEmbeddedPacScript(pac_url, &pac_script)) {
      return PROXY_PREF_NOT_CLEARED;
    }

    // In M35 and earlier, the way of specifying the DRP in a PAC script would
    // always include the port number after the host even if the port number
    // could be implied, so searching for ".googlezip.net:" in the PAC script
    // indicates whether there's a proxy in that PAC script with a host of the
    // form "*.googlezip.net".
    if (pac_script.find(".googlezip.net:") == std::string::npos)
      return PROXY_PREF_NOT_CLEARED;

    prefs->ClearPref(proxy_config::prefs::kProxy);
    return PROXY_PREF_CLEARED_PAC_GOOGLEZIP;
  }

  return PROXY_PREF_NOT_CLEARED;
}

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings(
    bool is_off_the_record_profile)
    : data_reduction_proxy::DataReductionProxySettings(
          is_off_the_record_profile),
      profile_(nullptr) {
  DCHECK(!is_off_the_record_profile);
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {}

void DataReductionProxyChromeSettings::Shutdown() {
  data_reduction_proxy::DataReductionProxyService* service =
      data_reduction_proxy_service();
  if (service)
    service->Shutdown();
}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    Profile* profile,
    std::unique_ptr<data_reduction_proxy::DataStore> store,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner) {
  profile_ = profile;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_ANDROID)
  // On mobile we write Data Reduction Proxy prefs directly to the pref service.
  // On desktop we store Data Reduction Proxy prefs in memory, writing to disk
  // every 60 minutes and on termination. Shutdown hooks must be added for
  // Android and iOS in order for non-zero delays to be supported.
  // (http://crbug.com/408264)
  base::TimeDelta commit_delay = base::TimeDelta();
#else
  base::TimeDelta commit_delay = base::TimeDelta::FromMinutes(60);
#endif

  if (!data_use_measurement::ChromeDataUseMeasurement::GetInstance()) {
    data_use_measurement::ChromeDataUseMeasurement::CreateInstance(
        g_browser_process->local_state());
  }
  PrefService* profile_prefs = profile->GetPrefs();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  std::unique_ptr<data_reduction_proxy::DataReductionProxyService> service =
      std::make_unique<data_reduction_proxy::DataReductionProxyService>(
          this, profile_prefs, url_loader_factory, std::move(store),
          g_browser_process->network_quality_tracker(),
          content::GetNetworkConnectionTracker(),
          data_use_measurement::ChromeDataUseMeasurement::GetInstance(),
          db_task_runner, commit_delay, GetClient(),
          version_info::GetChannelString(chrome::GetChannel()), GetUserAgent());
  data_reduction_proxy::DataReductionProxySettings::
      InitDataReductionProxySettings(profile_prefs, std::move(service));

#if defined(OS_CHROMEOS)
  data_reduction_proxy_service()->config()->EnableGetNetworkIdAsynchronously();
#endif

  data_reduction_proxy::DataReductionProxySettings::
      SetCallbackToRegisterSyntheticFieldTrial(base::Bind(
          &ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial));
  // In M35 and earlier, the Data Reduction Proxy enabled/disabled setting was
  // stored in prefs, so this setting needs to be migrated to the new way of
  // storing the setting. Removing this migration code would cause users
  // upgrading from M35 and earlier with the Data Reduction Proxy enabled to be
  // unable to browse non-SSL sites for the most part (see
  // http://crbug.com/476610).
  MigrateDataReductionProxyOffProxyPrefs(profile_prefs);
}

void DataReductionProxyChromeSettings::SetIgnoreLongTermBlackListRules(
    bool ignore_long_term_black_list_rules) {
  // |previews_service| is null if |profile_| is off the record.
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile_);
  if (previews_service && previews_service->previews_ui_service()) {
    previews_service->previews_ui_service()
        ->SetIgnoreLongTermBlackListForServerPreviews(
            ignore_long_term_black_list_rules);
  }
}

std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
DataReductionProxyChromeSettings::CreateDataFromNavigationHandle(
    content::NavigationHandle* handle,
    const net::HttpResponseHeaders* headers) {
  // Some unit tests don't have data_reduction_proxy_service() set.
  if (!data_reduction_proxy_service())
    return test_data_ ? std::move(test_data_) : nullptr;

  // TODO(721403): Need to fill in:
  //  - request_info_
  auto data = std::make_unique<data_reduction_proxy::DataReductionProxyData>();
  data->set_request_url(handle->GetURL());
  data->set_effective_connection_type(
      data_reduction_proxy_service()->GetEffectiveConnectionType());
  data->set_connection_type(net::NetworkChangeNotifier::ConnectionType(
      data_reduction_proxy_service()->GetConnectionType()));
  data->set_used_data_reduction_proxy(
      IsConfiguredDataReductionProxy(handle->GetProxyServer()));

  if (!headers || headers->IsRedirect(nullptr))
    return data;

  if (handle->WasResponseCached() &&
      (headers->HasHeader(data_reduction_proxy::chrome_proxy_header()) ||
       // Check for via header since Chrome-Proxy header maybe missing in
       // streamed responses.
       data_reduction_proxy::HasDataReductionProxyViaHeader(*headers,
                                                            nullptr)) &&
      !handle->GetURL().SchemeIsCryptographic()) {
    data->set_was_cached_data_reduction_proxy_response(true);
  }

  switch (data_reduction_proxy::ParseResponseTransform(*headers)) {
    case data_reduction_proxy::TRANSFORM_LITE_PAGE:
      data->set_lite_page_received(true);
      break;
    case data_reduction_proxy::TRANSFORM_IDENTITY:
    case data_reduction_proxy::TRANSFORM_COMPRESSED_VIDEO:
    case data_reduction_proxy::TRANSFORM_NONE:
    case data_reduction_proxy::TRANSFORM_UNKNOWN:
      break;
  }

  const ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<const ChromeNavigationUIData*>(handle->GetNavigationUIData());
  if (data_reduction_proxy::params::IsEnabledWithNetworkService() &&
      base::FeatureList::IsEnabled(
          data_reduction_proxy::features::
              kDataReductionProxyPopulatePreviewsPageIDToPingback)) {
    if (chrome_navigation_ui_data) {
      data->set_page_id(
          chrome_navigation_ui_data->data_reduction_proxy_page_id());
    }
    const auto session_key =
        data_reduction_proxy::DataReductionProxyRequestOptions::
            GetSessionKeyFromRequestHeaders(GetProxyRequestHeaders());
    if (session_key)
      data->set_session_key(session_key.value());
  }
  return data;
}

void DataReductionProxyChromeSettings::SetDataForNextCommitForTesting(
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData> data) {
  test_data_ = std::move(data);
}

// static
data_reduction_proxy::Client DataReductionProxyChromeSettings::GetClient() {
#if defined(OS_ANDROID)
  return data_reduction_proxy::Client::CHROME_ANDROID;
#elif defined(OS_MACOSX)
  return data_reduction_proxy::Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
  return data_reduction_proxy::Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
  return data_reduction_proxy::Client::CHROME_LINUX;
#elif defined(OS_WIN)
  return data_reduction_proxy::Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
  return data_reduction_proxy::Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
  return data_reduction_proxy::Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
  return data_reduction_proxy::Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
  return data_reduction_proxy::Client::CHROME_QNX;
#else
  return data_reduction_proxy::Client::UNKNOWN;
#endif
}
