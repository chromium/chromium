// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/types/pass_key.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

class GraphImpl;
using PassKey = base::PassKey<SiteDataCacheFacade>;

SiteDataCacheFacade::SiteDataCacheFacade(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFacadeFactory::GetInstance()->OnBeforeFacadeCreated(PassKey());

  std::optional<std::string> parent_context_id;
  if (browser_context->IsOffTheRecord()) {
    content::BrowserContext* parent_context =
        GetBrowserContextRedirectedInIncognito(browser_context);
    parent_context_id = parent_context->UniqueId();
  }

  // Creates the real cache on the SiteDataCache's sequence.
  SiteDataCacheFacadeFactory::GetInstance()
      ->cache_factory()
      ->OnBrowserContextCreated(browser_context->UniqueId(),
                                browser_context->GetPath(), parent_context_id);

  history::HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(
          Profile::FromBrowserContext(browser_context_));
  if (history)
    history_observation_.Observe(history);
}

SiteDataCacheFacade::~SiteDataCacheFacade() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFacadeFactory::GetInstance()
      ->cache_factory()
      ->OnBrowserContextDestroyed(browser_context_->UniqueId());
  SiteDataCacheFacadeFactory::GetInstance()->OnFacadeDestroyed(PassKey());
}

bool SiteDataCacheFacade::IsDataCacheRecordingForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return SiteDataCacheFacadeFactory::GetInstance()
      ->cache_factory()
      ->IsDataCacheRecordingForTesting(  // IN-TEST
          browser_context_->UniqueId());
}

void SiteDataCacheFacade::WaitUntilCacheInitializedForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;

  auto* cache =
      SiteDataCacheFacadeFactory::GetInstance()
          ->cache_factory()
          ->GetDataCacheForBrowserContext(browser_context_->UniqueId());
  if (cache->IsRecording()) {
    static_cast<SiteDataCacheImpl*>(cache)
        ->SetInitializationCallbackForTesting(  // IN-TEST
            run_loop.QuitClosure());
  }
  run_loop.Run();
}

void SiteDataCacheFacade::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deletion_info.IsAllHistory()) {
    ClearAllSiteData();
    return;
  }

  std::vector<url::Origin> origins_to_remove;

  for (const auto& it : deletion_info.deleted_urls_origin_map()) {
    const url::Origin origin = url::Origin::Create(it.first);
    const int remaining_visits_in_history = it.second.first;

    // If the origin is no longer exists in history, clear the site data.
    DCHECK_GE(remaining_visits_in_history, 0);
    if (remaining_visits_in_history == 0) {
      origins_to_remove.emplace_back(origin);
    }
  }

  if (origins_to_remove.empty()) {
    return;
  }

  auto* cache =
      SiteDataCacheFacadeFactory::GetInstance()
          ->cache_factory()
          ->GetDataCacheForBrowserContext(browser_context_->UniqueId());
  if (cache->IsRecording()) {
    static_cast<SiteDataCacheImpl*>(cache)->ClearSiteDataForOrigins(
        origins_to_remove);
  }
}

void SiteDataCacheFacade::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(history_observation_.IsObservingSource(history_service));
  history_observation_.Reset();
}

void SiteDataCacheFacade::ClearAllSiteData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* cache =
      SiteDataCacheFacadeFactory::GetInstance()
          ->cache_factory()
          ->GetDataCacheForBrowserContext(browser_context_->UniqueId());
  if (cache->IsRecording()) {
    static_cast<SiteDataCacheImpl*>(cache)->ClearAllSiteData();
  }
}

}  // namespace performance_manager
