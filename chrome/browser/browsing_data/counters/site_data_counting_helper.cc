// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"

#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_util.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;

SiteDataCountingHelper::SiteDataCountingHelper(
    Profile* profile,
    base::Time begin,
    base::Callback<void(int)> completion_callback)
    : profile_(profile),
      begin_(begin),
      completion_callback_(completion_callback),
      tasks_(0) {}

SiteDataCountingHelper::~SiteDataCountingHelper() {}

void SiteDataCountingHelper::CountAndDestroySelfWhenFinished() {
  content::StoragePartition* partition =
      content::BrowserContext::GetDefaultStoragePartition(profile_);

  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy(
      profile_->GetSpecialStoragePolicy());

  net::URLRequestContextGetter* rq_context = partition->GetURLRequestContext();

  tasks_ += 1;
  // Count origins with cookies.
  network::mojom::CookieManager* cookie_manager =
      partition->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookies(base::BindOnce(
      &SiteDataCountingHelper::GetCookiesCallback, base::Unretained(this)));

  storage::QuotaManager* quota_manager = partition->GetQuotaManager();
  if (quota_manager) {
    // Count origins with filesystem, websql, appcache, indexeddb,
    // serviceworkers and cachestorage using quota manager.
    auto origins_callback =
        base::BindRepeating(&SiteDataCountingHelper::GetQuotaOriginsCallback,
                            base::Unretained(this));
    const blink::mojom::StorageType types[] = {
        blink::mojom::StorageType::kTemporary,
        blink::mojom::StorageType::kPersistent,
        blink::mojom::StorageType::kSyncable};
    for (auto type : types) {
      tasks_ += 1;
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&storage::QuotaManager::GetOriginsModifiedSince,
                         quota_manager, type, begin_, origins_callback));
    }
  }

  // Count origins with local storage or session storage.
  content::DOMStorageContext* dom_storage = partition->GetDOMStorageContext();
  if (dom_storage) {
    tasks_ += 1;
    auto local_callback =
        base::Bind(&SiteDataCountingHelper::GetLocalStorageUsageInfoCallback,
                   base::Unretained(this), special_storage_policy);
    dom_storage->GetLocalStorageUsage(local_callback);
    // TODO(772337): Enable session storage counting when deletion is fixed.
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  // Count origins with flash data.
  flash_lso_helper_ = BrowsingDataFlashLSOHelper::Create(profile_);
  if (flash_lso_helper_) {
    tasks_ += 1;
    flash_lso_helper_->StartFetching(
        base::Bind(&SiteDataCountingHelper::SitesWithFlashDataCallback,
                   base::Unretained(this)));
  }
#endif

  // Counting site usage data and durable permissions.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile_);
  const ContentSettingsType content_settings[] = {
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, CONTENT_SETTINGS_TYPE_APP_BANNER};
  for (auto type : content_settings) {
    tasks_ += 1;
    GetOriginsFromHostContentSettignsMap(hcsm, type);
  }
  // Count origins with channel ids.
  tasks_ += 1;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SiteDataCountingHelper::GetChannelIDsOnIOThread,
                     base::Unretained(this), base::WrapRefCounted(rq_context)));
}

void SiteDataCountingHelper::GetOriginsFromHostContentSettignsMap(
    HostContentSettingsMap* hcsm,
    ContentSettingsType type) {
  std::set<GURL> origins;
  ContentSettingsForOneType settings;
  hcsm->GetSettingsForOneType(type, std::string(), &settings);
  for (const ContentSettingPatternSource& rule : settings) {
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
    if (cookie.CreationDate() >= begin_) {
      GURL url = net::cookie_util::CookieOriginToURL(cookie.Domain(),
                                                     cookie.IsSecure());
      origins.push_back(url);
    }
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(&SiteDataCountingHelper::Done,
                                          base::Unretained(this), origins));
}

void SiteDataCountingHelper::GetQuotaOriginsCallback(
    const std::set<url::Origin>& origins,
    blink::mojom::StorageType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<GURL> urls;
  urls.resize(origins.size());
  for (const url::Origin& origin : origins)
    urls.push_back(origin.GetURL());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&SiteDataCountingHelper::Done, base::Unretained(this),
                     std::move(urls)));
}

void SiteDataCountingHelper::GetLocalStorageUsageInfoCallback(
    const scoped_refptr<storage::SpecialStoragePolicy>& policy,
    const std::vector<content::LocalStorageUsageInfo>& infos) {
  std::vector<GURL> origins;
  for (const auto& info : infos) {
    if (info.last_modified >= begin_ &&
        (!policy || !policy->IsStorageProtected(info.origin))) {
      origins.push_back(info.origin);
    }
  }
  Done(origins);
}

void SiteDataCountingHelper::GetSessionStorageUsageInfoCallback(
    const scoped_refptr<storage::SpecialStoragePolicy>& policy,
    const std::vector<content::SessionStorageUsageInfo>& infos) {
  std::vector<GURL> origins;
  for (const auto& info : infos) {
    // Session storage doesn't know about creation time.
    if (!policy || !policy->IsStorageProtected(info.origin)) {
      origins.push_back(info.origin);
    }
  }
  Done(origins);
}

void SiteDataCountingHelper::SitesWithFlashDataCallback(
    const std::vector<std::string>& sites) {
  std::vector<GURL> origins;
  for (const std::string& site : sites) {
    origins.push_back(GURL(site));
  }
  Done(origins);
}

void SiteDataCountingHelper::GetChannelIDsOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& rq_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDService* channel_id_service =
      rq_context->GetURLRequestContext()->channel_id_service();
  channel_id_service->GetChannelIDStore()->GetAllChannelIDs(base::Bind(
      &SiteDataCountingHelper::GetChannelIDsCallback, base::Unretained(this)));
}

void SiteDataCountingHelper::GetChannelIDsCallback(
    const net::ChannelIDStore::ChannelIDList& channel_ids) {
  std::vector<GURL> origins;
  for (const net::ChannelIDStore::ChannelID& channel_id : channel_ids) {
    if (channel_id.creation_time() >= begin_) {
      // Assume url is https://<server_identifier> on default port because
      // channel ids don't know about their scheme or port.
      origins.push_back(GURL("https://" + channel_id.server_identifier()));
    }
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(&SiteDataCountingHelper::Done,
                                          base::Unretained(this), origins));
}

void SiteDataCountingHelper::Done(const std::vector<GURL>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(tasks_ > 0);
  for (const GURL& origin : origins) {
    if (BrowsingDataHelper::HasWebScheme(origin))
      unique_hosts_.insert(origin.host());
  }
  if (--tasks_ > 0)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(completion_callback_, unique_hosts_.size()));
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}
