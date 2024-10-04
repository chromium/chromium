// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/cookie_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/1125897
#endif

using content::BrowserThread;

SiteDataCountingHelper::SiteDataCountingHelper(
    Profile* profile,
    base::Time begin,
    base::Time end,
    base::OnceCallback<void(int)> completion_callback)
    : profile_(profile),
      begin_(begin),
      end_(end),
      completion_callback_(std::move(completion_callback)),
      tasks_(0) {}

SiteDataCountingHelper::~SiteDataCountingHelper() {}

void SiteDataCountingHelper::CountAndDestroySelfWhenFinished() {
  content::StoragePartition* partition = profile_->GetDefaultStoragePartition();

  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy(
      profile_->GetSpecialStoragePolicy());

  tasks_ += 1;
  // Count origins with cookies.
  network::mojom::CookieManager* cookie_manager =
      partition->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookies(base::BindOnce(
      &SiteDataCountingHelper::GetCookiesCallback, base::Unretained(this)));

  storage::QuotaManager* quota_manager = partition->GetQuotaManager();
  if (quota_manager) {
    // Count storage keys with filesystem, websql, indexeddb, serviceworkers,
    // cachestorage, and medialicense using quota manager.
    // TODO(crbug.com/40264778): For now, media licenses are part of the quota
    // management system, but when dis-integrated, remove media license logic
    // from quota logic.
    auto buckets_callback =
        base::BindRepeating(&SiteDataCountingHelper::GetQuotaBucketsCallback,
                            base::Unretained(this));
    const blink::mojom::StorageType types[] = {
        blink::mojom::StorageType::kTemporary,
        blink::mojom::StorageType::kSyncable};
    for (auto type : types) {
      tasks_ += 1;
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&storage::QuotaManager::GetBucketsModifiedBetween,
                         quota_manager, type, begin_, end_, buckets_callback));
    }
  }

  // Count origins with local storage or session storage.
  content::DOMStorageContext* dom_storage = partition->GetDOMStorageContext();
  if (dom_storage) {
    tasks_ += 1;
    auto local_callback = base::BindOnce(
        &SiteDataCountingHelper::GetLocalStorageUsageInfoCallback,
        base::Unretained(this), special_storage_policy);
    dom_storage->GetLocalStorageUsage(std::move(local_callback));
    // TODO(crbug.com/41348517): Enable session storage counting when deletion
    // is fixed.
  }

// TODO(crbug.com/40272342): Add CdmStorageManager logic to count origins, and
// add test to browsing_data_remover_browsertest.cc to test counting logic.
#if BUILDFLAG(IS_ANDROID)
  // Count origins with media licenses on Android.
  tasks_ += 1;
  Done(cdm::MediaDrmStorageImpl::GetOriginsModifiedBetween(profile_->GetPrefs(),
                                                           begin_, end_));
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  tasks_ += 1;
  auto cdm_storage_callback = base::BindOnce(
      &SiteDataCountingHelper::GetCdmStorageCallback, base::Unretained(this));
  partition->GetCdmStorageDataModel()->GetUsagePerAllStorageKeys(
      std::move(cdm_storage_callback), begin_, end_);
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // Counting site usage data and durable permissions.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile_);
  const ContentSettingsType content_settings[] = {
    ContentSettingsType::DURABLE_STORAGE,
    ContentSettingsType::APP_BANNER,
  };
  for (auto type : content_settings) {
    tasks_ += 1;
    GetOriginsFromHostContentSettignsMap(hcsm, type);
  }

  if (base::FeatureList::IsEnabled(
          network::features::kCompressionDictionaryTransportBackend)) {
    tasks_ += 1;
    partition->GetNetworkContext()->GetSharedDictionaryOriginsBetween(
        begin_, end_,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(
                &SiteDataCountingHelper::GetSharedDictionaryOriginsCallback,
                base::Unretained(this)),
            std::vector<url::Origin>()));
  }
}

void SiteDataCountingHelper::GetOriginsFromHostContentSettignsMap(
    HostContentSettingsMap* hcsm,
    ContentSettingsType type) {
  std::set<GURL> origins;
  for (const ContentSettingPatternSource& rule :
       hcsm->GetSettingsForOneType(type)) {
    GURL url(rule.primary_pattern.ToString());
    if (!url.is_empty()) {
      origins.insert(url);
    }
  }
  Done(std::vector<GURL>(origins.begin(), origins.end()));
}

void SiteDataCountingHelper::GetCookiesCallback(
    const net::CookieList& cookies) {
  std::vector<GURL> origins;
  for (const net::CanonicalCookie& cookie : cookies) {
    if (cookie.CreationDate() >= begin_ && cookie.CreationDate() < end_) {
      GURL url = net::cookie_util::CookieOriginToURL(cookie.Domain(),
                                                     cookie.SecureAttribute());
      origins.push_back(url);
    }
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SiteDataCountingHelper::Done,
                                base::Unretained(this), origins));
}

void SiteDataCountingHelper::GetCdmStorageCallback(
    const CdmStorageKeyUsageSize& usage_per_storage_keys) {
  std::vector<GURL> urls;

  for (auto const& [key, _] : usage_per_storage_keys) {
    urls.emplace_back(key.origin().GetURL());
  }

  Done(urls);
}

void SiteDataCountingHelper::GetQuotaBucketsCallback(
    const std::set<storage::BucketLocator>& buckets) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::set<GURL> urls;
  for (const storage::BucketLocator& bucket : buckets)
    urls.insert(bucket.storage_key.origin().GetURL());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SiteDataCountingHelper::Done, base::Unretained(this),
                     std::vector<GURL>(urls.begin(), urls.end())));
}

void SiteDataCountingHelper::GetSharedDictionaryOriginsCallback(
    const std::vector<url::Origin>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<GURL> urls;
  for (const url::Origin& origin : origins) {
    urls.emplace_back(origin.GetURL());
  }
  Done(urls);
}

void SiteDataCountingHelper::GetLocalStorageUsageInfoCallback(
    const scoped_refptr<storage::SpecialStoragePolicy>& policy,
    const std::vector<content::StorageUsageInfo>& infos) {
  std::vector<GURL> origins;
  for (const auto& info : infos) {
    if (info.last_modified >= begin_ && info.last_modified < end_ &&
        (!policy ||
         !policy->IsStorageProtected(info.storage_key.origin().GetURL()))) {
      origins.push_back(info.storage_key.origin().GetURL());
    }
  }
  Done(origins);
}

void SiteDataCountingHelper::Done(const std::vector<GURL>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(tasks_ > 0);
  for (const GURL& origin : origins) {
    if (browsing_data::HasWebScheme(origin))
      unique_hosts_.insert(origin.host());
  }
  if (--tasks_ > 0)
    return;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback_), unique_hosts_.size()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}
