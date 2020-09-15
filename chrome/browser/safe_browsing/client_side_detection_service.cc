// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/escape.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

const int ClientSideDetectionService::kInitialClientModelFetchDelayMs = 10000;
const int ClientSideDetectionService::kReportsIntervalDays = 1;
const int ClientSideDetectionService::kMaxReportsPerInterval = 3;
const int ClientSideDetectionService::kNegativeCacheIntervalDays = 1;
const int ClientSideDetectionService::kPositiveCacheIntervalMinutes = 30;

const char ClientSideDetectionService::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";

struct ClientSideDetectionService::ClientPhishingReportInfo {
  std::unique_ptr<network::SimpleURLLoader> loader;
  ClientReportPhishingRequestCallback callback;
  GURL phishing_url;
};

ClientSideDetectionService::CacheState::CacheState(bool phish, base::Time time)
    : is_phishing(phish), timestamp(time) {}

ClientSideDetectionService::ClientSideDetectionService(Profile* profile)
    : profile_(profile),
      enabled_(false),
      extended_reporting_(false),
      url_loader_factory_(
          g_browser_process->safe_browsing_service()
              ? g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
                    profile)
              : nullptr) {
  // |profile_| can be null in unit tests
  if (!profile_)
    return;

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::Bind(&ClientSideDetectionService::OnPrefsUpdated,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::Bind(&ClientSideDetectionService::OnPrefsUpdated,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingScoutReportingEnabled,
      base::Bind(&ClientSideDetectionService::OnPrefsUpdated,
                 base::Unretained(this)));

  // Do an initial check of the prefs.
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() {
  weak_factory_.InvalidateWeakPtrs();
}

void ClientSideDetectionService::Shutdown() {
  url_loader_factory_.reset();
}

void ClientSideDetectionService::OnPrefsUpdated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool enabled = IsSafeBrowsingEnabled(*profile_->GetPrefs());
  bool extended_reporting =
      IsEnhancedProtectionEnabled(*profile_->GetPrefs()) ||
      IsExtendedReportingEnabled(*profile_->GetPrefs());
  if (enabled == enabled_ && extended_reporting_ == extended_reporting)
    return;

  enabled_ = enabled;
  extended_reporting_ = extended_reporting;

  if (enabled_) {
    if (!model_factory_.is_null()) {
      model_loader_ = model_factory_.Run();
    } else {
      model_loader_ = std::make_unique<ModelLoader>(
          base::BindRepeating(&ClientSideDetectionService::SendModelToRenderers,
                              base::Unretained(this)),
          profile_->GetURLLoaderFactory(), extended_reporting_);
    }
    // Refresh the models when the service is enabled.  This can happen when
    // either of the preferences are toggled, or early during startup if
    // safe browsing is already enabled. In a lot of cases the model will be
    // in the cache so it  won't actually be fetched from the network.
    // We delay the first model fetches to avoid slowing down browser startup.
    model_loader_->ScheduleFetch(kInitialClientModelFetchDelayMs);
  } else {
    if (model_loader_) {
      // Cancel model loads in progress.
      model_loader_->CancelFetcher();
    }
    // Invoke pending callbacks with a false verdict.
    for (auto it = client_phishing_reports_.begin();
         it != client_phishing_reports_.end(); ++it) {
      ClientPhishingReportInfo* info = it->second.get();
      if (!info->callback.is_null())
        info->callback.Run(info->phishing_url, false);
    }
    client_phishing_reports_.clear();
    cache_.clear();
  }

  SendModelToRenderers();  // always refresh the renderer state
}

void ClientSideDetectionService::SendClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> verdict,
    bool is_extended_reporting,
    bool is_enhanced_reporting,
    const ClientReportPhishingRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientSideDetectionService::StartClientReportPhishingRequest,
          weak_factory_.GetWeakPtr(), std::move(verdict), is_extended_reporting,
          is_enhanced_reporting, callback));
}

bool ClientSideDetectionService::IsPrivateIPAddress(
    const std::string& ip_address) const {
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(ip_address)) {
    // Err on the side of privacy and assume this might be private.
    return true;
  }

  return !address.IsPubliclyRoutable();
}

void ClientSideDetectionService::AddClientSideDetectionHost(
    ClientSideDetectionHost* host) {
  csd_hosts_.push_back(host);
}

void ClientSideDetectionService::RemoveClientSideDetectionHost(
    ClientSideDetectionHost* host) {
  std::vector<ClientSideDetectionHost*>::iterator position =
      std::find(csd_hosts_.begin(), csd_hosts_.end(), host);
  if (position != csd_hosts_.end())
    csd_hosts_.erase(position);
}

void ClientSideDetectionService::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  std::string data;
  if (response_body)
    data = std::move(*response_body.get());
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  DCHECK(base::Contains(client_phishing_reports_, url_loader));
  HandlePhishingVerdict(url_loader, url_loader->GetFinalURL(),
                        url_loader->NetError(), response_code, data);
}

void ClientSideDetectionService::SendModelToRenderers() {
  for (ClientSideDetectionHost* host : csd_hosts_) {
    host->SendModelToRenderFrame();
  }
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> request,
    bool is_extended_reporting,
    bool is_enhanced_reporting,
    const ClientReportPhishingRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!enabled_) {
    if (!callback.is_null())
      callback.Run(GURL(request->url()), false);
    return;
  }

  // Fill in metadata about which model we used.
  request->set_model_filename(model_loader_->name());
  if (is_extended_reporting || is_enhanced_reporting) {
    if (is_enhanced_reporting) {
      request->mutable_population()->set_user_population(
          ChromeUserPopulation::ENHANCED_PROTECTION);
    } else {
      request->mutable_population()->set_user_population(
          ChromeUserPopulation::EXTENDED_REPORTING);
    }
  } else {
    request->mutable_population()->set_user_population(
        ChromeUserPopulation::SAFE_BROWSING);
  }
  request->mutable_population()->set_profile_management_status(
      GetProfileManagementStatus(
          g_browser_process->browser_policy_connector()));

  std::string request_data;
  request->SerializeToString(&request_data);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "safe_browsing_client_side_phishing_detector", R"(
          semantics {
            sender: "Safe Browsing Client-Side Phishing Detector"
            description:
              "If the client-side phishing detector determines that the "
              "current page contents are similar to phishing pages, it will "
              "send a request to Safe Browsing to ask for a final verdict. If "
              "Safe Browsing agrees the page is dangerous, Chrome will show a "
              "full-page interstitial warning."
            trigger:
              "Whenever the clinet-side detector machine learning model "
              "computes a phishy-ness score above a threshold, after page-load."
            data:
              "Top-level page URL without CGI parameters, boolean and double "
              "features extracted from DOM, such as the number of resources "
              "loaded in the page, if certain likely phishing and social "
              "engineering terms found on the page, etc."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "Safe browsing cookie store"
            setting:
              "Users can enable or disable this feature by toggling 'Protect "
              "you and your device from dangerous sites' in Chrome settings "
              "under Privacy. This feature is enabled by default."
            chrome_policy {
              SafeBrowsingEnabled {
                policy_options {mode: MANDATORY}
                SafeBrowsingEnabled: false
              }
            }
          })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetClientReportUrl(kClientReportPhishingUrl);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  loader->AttachStringForUpload(request_data, "application/octet-stream");
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ClientSideDetectionService::OnURLLoaderComplete,
                     base::Unretained(this), loader.get()));

  // Remember which callback and URL correspond to the current fetcher object.
  std::unique_ptr<ClientPhishingReportInfo> info(new ClientPhishingReportInfo);
  auto* loader_ptr = loader.get();
  info->loader = std::move(loader);
  info->callback = callback;
  info->phishing_url = GURL(request->url());
  client_phishing_reports_[loader_ptr] = std::move(info);

  // Record that we made a request
  phishing_report_times_.push(base::Time::Now());
}

void ClientSideDetectionService::HandlePhishingVerdict(
    network::SimpleURLLoader* source,
    const GURL& url,
    int net_error,
    int response_code,
    const std::string& data) {
  ClientPhishingResponse response;
  std::unique_ptr<ClientPhishingReportInfo> info =
      std::move(client_phishing_reports_[source]);
  client_phishing_reports_.erase(source);

  bool is_phishing = false;
  if (net_error == net::OK && net::HTTP_OK == response_code &&
      response.ParseFromString(data)) {
    // Cache response, possibly flushing an old one.
    cache_[info->phishing_url] =
        base::WrapUnique(new CacheState(response.phishy(), base::Time::Now()));
    is_phishing = response.phishy();
  }
  if (!info->callback.is_null())
    info->callback.Run(info->phishing_url, is_phishing);
}

bool ClientSideDetectionService::IsInCache(const GURL& url) {
  UpdateCache();

  return cache_.find(url) != cache_.end();
}

bool ClientSideDetectionService::GetValidCachedResult(const GURL& url,
                                                      bool* is_phishing) {
  UpdateCache();

  auto it = cache_.find(url);
  if (it == cache_.end()) {
    return false;
  }

  // We still need to check if the result is valid.
  const CacheState& cache_state = *it->second;
  if (cache_state.is_phishing
          ? cache_state.timestamp >
                base::Time::Now() -
                    base::TimeDelta::FromMinutes(kPositiveCacheIntervalMinutes)
          : cache_state.timestamp >
                base::Time::Now() -
                    base::TimeDelta::FromDays(kNegativeCacheIntervalDays)) {
    *is_phishing = cache_state.is_phishing;
    return true;
  }
  return false;
}

void ClientSideDetectionService::UpdateCache() {
  // Since we limit the number of requests but allow pass-through for cache
  // refreshes, we don't want to remove elements from the cache if they
  // could be used for this purpose even if we will not use the entry to
  // satisfy the request from the cache.
  base::TimeDelta positive_cache_interval =
      std::max(base::TimeDelta::FromMinutes(kPositiveCacheIntervalMinutes),
               base::TimeDelta::FromDays(kReportsIntervalDays));
  base::TimeDelta negative_cache_interval =
      std::max(base::TimeDelta::FromDays(kNegativeCacheIntervalDays),
               base::TimeDelta::FromDays(kReportsIntervalDays));

  // Remove elements from the cache that will no longer be used.
  for (auto it = cache_.begin(); it != cache_.end();) {
    const CacheState& cache_state = *it->second;
    if (cache_state.is_phishing
            ? cache_state.timestamp >
                  base::Time::Now() - positive_cache_interval
            : cache_state.timestamp >
                  base::Time::Now() - negative_cache_interval) {
      ++it;
    } else {
      cache_.erase(it++);
    }
  }
}

bool ClientSideDetectionService::OverPhishingReportLimit() {
  return GetPhishingNumReports() > kMaxReportsPerInterval;
}

int ClientSideDetectionService::GetPhishingNumReports() {
  return GetNumReports(&phishing_report_times_);
}

int ClientSideDetectionService::GetNumReports(
    base::queue<base::Time>* report_times) {
  base::Time cutoff =
      base::Time::Now() - base::TimeDelta::FromDays(kReportsIntervalDays);

  // Erase items older than cutoff because we will never care about them again.
  while (!report_times->empty() && report_times->front() < cutoff) {
    report_times->pop();
  }

  // Return the number of elements that are above the cutoff.
  return report_times->size();
}

// static
GURL ClientSideDetectionService::GetClientReportUrl(
    const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));

  return url;
}

ModelLoader::ClientModelStatus
ClientSideDetectionService::GetLastModelStatus() {
  // |model_loader_| can be null in tests
  return model_loader_ ? model_loader_->last_client_model_status()
                       : ModelLoader::MODEL_NEVER_FETCHED;
}

std::string ClientSideDetectionService::GetModelStr() {
  return model_loader_ ? model_loader_->model_str() : "";
}

void ClientSideDetectionService::SetModelLoaderFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<ModelLoader>()> factory) {
  model_factory_ = factory;
}

void ClientSideDetectionService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

}  // namespace safe_browsing
