// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

class GraphImpl;

SiteDataCacheFacade::SiteDataCacheFacade(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* parent_context = nullptr;
  if (browser_context->IsOffTheRecord()) {
    parent_context =
        chrome::GetBrowserContextRedirectedInIncognito(browser_context);
  }
  SiteDataCacheFactory::OnBrowserContextCreatedOnUIThread(
      SiteDataCacheFactory::GetInstance(), browser_context_, parent_context);
}

SiteDataCacheFacade::~SiteDataCacheFacade() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFactory::OnBrowserContextDestroyedOnUIThread(
      SiteDataCacheFactory::GetInstance(), browser_context_);
}

void SiteDataCacheFacade::IsDataCacheRecordingForTesting(
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFactory::GetInstance()->IsDataCacheRecordingForTesting(
      browser_context_->UniqueId(), std::move(cb));
}

void SiteDataCacheFacade::WaitUntilCacheInitializedForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure quit_closure,
                        const std::string& browser_context_id,
                        GraphImpl* graph_unused) {
                       auto* cache = SiteDataCacheFactory::GetInstance()
                                         ->GetDataCacheForBrowserContext(
                                             browser_context_id);
                       if (cache->IsRecordingForTesting()) {
                         static_cast<SiteDataCacheImpl*>(cache)
                             ->SetInitializationCallbackForTesting(
                                 std::move(quit_closure));
                       }
                     },
                     run_loop.QuitClosure(), browser_context_->UniqueId()));
  run_loop.Run();
}

}  // namespace performance_manager
