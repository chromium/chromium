// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_url_lookup_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace {

constexpr char kVerdictCacheEventHistogram[] =
    "Enterprise.DataProtection.VerdictCacheEvent";

int GetCacheDurationSec(safe_browsing::RTLookupResponse* rt_lookup_response) {
  DCHECK(rt_lookup_response);
  const auto& threat_infos = rt_lookup_response->threat_info();

  // Defensive check
  if (threat_infos.empty()) {
    return 0;
  }

  // We do not check for matched rules, because that would exclude safe verdicts
  int cache_duration_sec = threat_infos[0].cache_duration_sec();
  for (int i = 1; i < threat_infos.size(); ++i) {
    const auto& threat_info = threat_infos[i];
    if (threat_info.cache_duration_sec() < cache_duration_sec) {
      cache_duration_sec = threat_info.cache_duration_sec();
    }
  }
  return cache_duration_sec;
}

}  // namespace
namespace enterprise_data_protection {

DataProtectionUrlLookupService::Verdict::Verdict() = default;
DataProtectionUrlLookupService::Verdict::Verdict(Verdict&&) = default;
DataProtectionUrlLookupService::Verdict::~Verdict() = default;

DataProtectionUrlLookupService::DataProtectionUrlLookupService()
    : verdict_cache_(GetVerdictCacheMaxSize()) {}

DataProtectionUrlLookupService::~DataProtectionUrlLookupService() = default;

void DataProtectionUrlLookupService::DoLookup(
    safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
    const GURL& url,
    const std::string& identifier,
    LookupCallback callback,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);
  DCHECK(callback);

  auto cached_verdict = verdict_cache_.Peek(url.spec());
  if (cached_verdict != verdict_cache_.end() &&
      !IsVerdictExpired(cached_verdict->second)) {
    // Proto assignment has deep copy semantics. There is room to optimize by
    // implementing shared ownership (both this service and
    // `DataProtectionPageUserData` own a ptr to RTLookupResponse).
    std::unique_ptr<safe_browsing::RTLookupResponse> response =
        std::make_unique<safe_browsing::RTLookupResponse>(
            *cached_verdict->second.response);
    std::move(callback).Run(std::move(response));
    base::UmaHistogramEnumeration(kVerdictCacheEventHistogram,
                                  URLVerdictCacheEvent::kCacheHit);
    return;
  }

  base::UmaHistogramEnumeration(kVerdictCacheEventHistogram,
                                URLVerdictCacheEvent::kUrlScanRequest);
  lookup_service->StartMaybeCachedLookup(
      url,
      base::BindOnce(&DataProtectionUrlLookupService::OnRealTimeLookupComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), url,
                     identifier),
      base::SequencedTaskRunner::GetCurrentDefault(),
      sessions::SessionTabHelper::IdForTab(web_contents),
      /*referring_app_info=*/std::nullopt, /*use_cache=*/
      !base::FeatureList::IsEnabled(kEnableSinglePageAppDataProtection));
}

void DataProtectionUrlLookupService::OnRealTimeLookupComplete(
    LookupCallback callback,
    const GURL& url,
    const std::string& identifier,
    bool is_success,
    bool is_cached,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!is_success) {
    rt_lookup_response.reset();
  } else if (base::FeatureList::IsEnabled(kEnableVerdictCache) &&
             rt_lookup_response) {
    // Guarantee that verdict cache contents are non-empty.
    int cache_duration_sec = GetCacheDurationSec(rt_lookup_response.get());
    if (cache_duration_sec > 0) {
      Verdict verdict;
      verdict.response = std::make_unique<safe_browsing::RTLookupResponse>(
          *rt_lookup_response);
      verdict.expiry_time =
          base::Time::Now() + base::Seconds(cache_duration_sec);
      verdict_cache_.Put(url.spec(), std::move(verdict));
    }
  }

  std::move(callback).Run(std::move(rt_lookup_response));
}

// static
bool DataProtectionUrlLookupService::IsVerdictExpired(const Verdict& verdict) {
  return base::Time::Now() > verdict.expiry_time;
}

// ====================================================
// DataProtectionUrlLookupServiceFactory implementation
// ====================================================

DataProtectionUrlLookupServiceFactory::DataProtectionUrlLookupServiceFactory()
    : ProfileKeyedServiceFactory("DataProtectionUrlLookupService",
                                 ProfileSelections::BuildForRegularProfile()) {}

DataProtectionUrlLookupServiceFactory::
    ~DataProtectionUrlLookupServiceFactory() = default;

// static
DataProtectionUrlLookupServiceFactory*
DataProtectionUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<DataProtectionUrlLookupServiceFactory> instance;
  return instance.get();
}

// static
DataProtectionUrlLookupService*
DataProtectionUrlLookupServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataProtectionUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
DataProtectionUrlLookupServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DataProtectionUrlLookupService>();
}

// static
size_t DataProtectionUrlLookupService::GetVerdictCacheMaxSize() {
  size_t max_value = enterprise_data_protection::kVerdictCacheMaxSize.Get();
  return max_value > 0
             ? max_value
             : enterprise_data_protection::kVerdictCacheMaxSize.default_value;
}

}  // namespace enterprise_data_protection
