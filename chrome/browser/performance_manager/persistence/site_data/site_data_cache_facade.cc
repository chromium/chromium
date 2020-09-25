// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/util/type_safety/pass_key.h"
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
using PassKey = util::PassKey<SiteDataCacheFacade>;

SiteDataCacheFacade::SiteDataCacheFacade(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFacadeFactory::GetInstance()->OnBeforeFacadeCreated(PassKey());

  base::Optional<std::string> parent_context_id;
  if (browser_context->IsOffTheRecord()) {
    content::BrowserContext* parent_context =
        chrome::GetBrowserContextRedirectedInIncognito(browser_context);
    parent_context_id = parent_context->UniqueId();
  }

  // Creates the real cache on the SiteDataCache's sequence.
  SiteDataCacheFacadeFactory::GetInstance()->cache_factory()->Post(
      FROM_HERE, &SiteDataCacheFactory::OnBrowserContextCreated,
      browser_context->UniqueId(), browser_context->GetPath(),
      parent_context_id);

  history::HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(
          Profile::FromBrowserContext(browser_context_));
  if (history)
    history_observer_.Add(history);
}

SiteDataCacheFacade::~SiteDataCacheFacade() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFacadeFactory::GetInstance()->cache_factory()->Post(
      FROM_HERE, &SiteDataCacheFactory::OnBrowserContextDestroyed,
      browser_context_->UniqueId());
  SiteDataCacheFacadeFactory::GetInstance()->OnFacadeDestroyed(PassKey());
}

void SiteDataCacheFacade::IsDataCacheRecordingForTesting(
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFacadeFactory::GetInstance()
      ->cache_factory()
      ->PostTaskWithThisObject(
          FROM_HERE,
          base::BindOnce(
              [](base::OnceCallback<void(bool)> cb,
                 const std::string& browser_context_id,
                 SiteDataCacheFactory* cache_factory) {
                std::move(cb).Run(cache_factory->IsDataCacheRecordingForTesting(
                    browser_context_id));
              },
              std::move(cb), browser_context_->UniqueId()));
}

void SiteDataCacheFacade::WaitUntilCacheInitializedForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  SiteDataCacheFacadeFactory::GetInstance()
      ->cache_factory()
      ->PostTaskWithThisObject(
          FROM_HERE, base::BindOnce(
                         [](base::OnceClosure quit_closure,
                            const std::string& browser_context_id,
                            SiteDataCacheFactory* cache_factory) {
                           auto* cache =
                               cache_factory->GetDataCacheForBrowserContext(
                                   browser_context_id);
                           if (cache->IsRecording()) {
                             static_cast<SiteDataCacheImpl*>(cache)
                                 ->SetInitializationCallbackForTesting(
                                     std::move(quit_closure));
                           }
                         },
                         run_loop.QuitClosure(), browser_context_->UniqueId()));
  run_loop.Run();
}

void SiteDataCacheFacade::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    auto clear_all_site_data_cb = base::BindOnce(
        [](const std::string& browser_context_id,
           SiteDataCacheFactory* cache_factory) {
          auto* cache =
              cache_factory->GetDataCacheForBrowserContext(browser_context_id);
          if (cache->IsRecording())
            static_cast<SiteDataCacheImpl*>(cache)->ClearAllSiteData();
        },
        browser_context_->UniqueId());
    SiteDataCacheFacadeFactory::GetInstance()
        ->cache_factory()
        ->PostTaskWithThisObject(FROM_HERE, std::move(clear_all_site_data_cb));
  } else {
    std::vector<url::Origin> origins_to_remove;

    for (const auto& it : deletion_info.deleted_urls_origin_map()) {
      const url::Origin origin = url::Origin::Create(it.first);
      const int remaining_visits_in_history = it.second.first;

      // If the origin is no longer exists in history, clear the site data.
      DCHECK_GE(remaining_visits_in_history, 0);
      if (remaining_visits_in_history == 0)
        origins_to_remove.emplace_back(origin);
    }

    if (origins_to_remove.empty())
      return;

    auto clear_site_data_cb = base::BindOnce(
        [](const std::string& browser_context_id,
           const std::vector<url::Origin>& origins_to_remove,
           SiteDataCacheFactory* cache_factory) {
          auto* cache =
              cache_factory->GetDataCacheForBrowserContext(browser_context_id);
          if (cache->IsRecording()) {
            static_cast<SiteDataCacheImpl*>(cache)->ClearSiteDataForOrigins(
                origins_to_remove);
          }
        },
        browser_context_->UniqueId(), std::move(origins_to_remove));
    SiteDataCacheFacadeFactory::GetInstance()
        ->cache_factory()
        ->PostTaskWithThisObject(FROM_HERE, std::move(clear_site_data_cb));
  }
}

void SiteDataCacheFacade::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_observer_.Remove(history_service);
}

}  // namespace performance_manager
