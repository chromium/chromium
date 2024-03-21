// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/threading/sequence_bound.h"
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
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager {

namespace {

// Helper to invoke functions on SiteDataCacheFactory. If the factory lives on
// the PM sequence, it posts a task to invoke the function, otherwise it calls
// it directly.
//
// Use with care: there can be subtle behaviour differences when the same code
// path sometimes posts and sometimes doesn't. It's needed here to be sure that
// ClearAllSiteData() and ClearSiteDataForOrigins() are handled correctly during
// shutdown:
//
// * HistoryService calls OnURLsDeleted() on the UI thread. Calling
//   ClearAllSiteData() synchronously on the same thread is guaranteed to
//   complete before shutdown.
// * When SiteDataCacheFactory lives on the PM sequence, must post a task to
//   call ClearAllSiteData(). This is guaranteed to run before shutdown because
//   the PM sequence is BLOCK_SHUTDOWN.
// * But tasks on the UI thread do NOT block shutdown, so when
//   SiteDataCacheFactory lives on the UI sequence, posting a task to call
//   ClearAllSiteData() would just put it back in the task queue where it isn't
//   guaranteed to run before shutdown.
// * Therefore, when SiteDataCacheFactory lives on the UI sequence, must call
//   ClearAllSiteData() without posting. That means other accesses to
//   SiteDataCacheFactory shouldn't post, to preserve ordering.
//
// The implementation of ClearAllSiteData() is async, but once it starts it will
// finish its work before shutdown, because it only posts tasks to a different
// BLOCK_SHUTDOWN sequence (to write to leveldb).
//
// Other functions that write to the SiteData db are triggered from PageNode
// observers on the PM sequence, rather from the UI thread, so they're already
// invoked without posting tasks and don't need an adapter like this.
class PostOrCall {
 public:
  // If `cache_factory_variant` holds a SiteDataCacheFactory, invokes `callback`
  // immediately. If it holds a SequenceBound<SiteDataCacheFactory>, posts
  // `callback` to the sequence it's bound to.
  PostOrCall(const base::Location& from_here,
             SiteDataCacheFactoryVariant& cache_factory_variant,
             base::OnceCallback<void(SiteDataCacheFactory*)> callback)
      : from_here_(from_here), callback_(std::move(callback)) {
    absl::visit(*this, cache_factory_variant);
  }

  // If `cache_factory_variant` holds a SiteDataCacheFactory, calls
  // `method(args, ...)` on the factory. If it holds a
  // SequenceBound<SiteDataCacheFactory>, posts a task to the factory's sequence
  // that calls `method(args, ...)` on it.
  template <typename... Args,
            typename Method = void (SiteDataCacheFactory::*)(Args...)>
  PostOrCall(const base::Location& from_here,
             SiteDataCacheFactoryVariant& cache_factory_variant,
             Method&& method,
             Args&&... args)
      // Wrap `method` and `args` in a callback and delegate to the other
      // constructor.
      : PostOrCall(from_here,
                   cache_factory_variant,
                   base::BindOnce(
                       [](Method&& method,
                          Args&&... args,
                          SiteDataCacheFactory* cache_factory) {
                         std::invoke(method, cache_factory,
                                     std::forward<Args>(args)...);
                       },
                       std::forward<Method>(method),
                       std::forward<Args>(args)...)) {}

  ~PostOrCall() = default;

  PostOrCall(const PostOrCall&) = delete;
  PostOrCall& operator=(const PostOrCall&) = delete;

  // operator() is invoked by `absl::visit` from the constructor with the type
  // contained in the variant.
  void operator()(absl::monostate) {
    // SiteDataCacheFactory was not initialized.
    NOTREACHED_NORETURN();
  }
  void operator()(SiteDataCacheFactory& cache_factory) {
    std::move(callback_).Run(&cache_factory);
  }
  void operator()(base::SequenceBound<SiteDataCacheFactory>& cache_factory) {
    cache_factory.PostTaskWithThisObject(std::move(callback_),
                                         from_here_.get());
  }

 private:
  const raw_ref<const base::Location> from_here_;
  base::OnceCallback<void(SiteDataCacheFactory*)> callback_;
};

}  // namespace

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
        chrome::GetBrowserContextRedirectedInIncognito(browser_context);
    parent_context_id = parent_context->UniqueId();
  }

  // Creates the real cache on the SiteDataCache's sequence.
  PostOrCall(FROM_HERE,
             SiteDataCacheFacadeFactory::GetInstance()->cache_factory(),
             &SiteDataCacheFactory::OnBrowserContextCreated,
             browser_context->UniqueId(), browser_context->GetPath(),
             std::move(parent_context_id));

  history::HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(
          Profile::FromBrowserContext(browser_context_));
  if (history)
    history_observation_.Observe(history);
}

SiteDataCacheFacade::~SiteDataCacheFacade() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PostOrCall(FROM_HERE,
             SiteDataCacheFacadeFactory::GetInstance()->cache_factory(),
             &SiteDataCacheFactory::OnBrowserContextDestroyed,
             browser_context_->UniqueId());
  SiteDataCacheFacadeFactory::GetInstance()->OnFacadeDestroyed(PassKey());
}

void SiteDataCacheFacade::IsDataCacheRecordingForTesting(
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PostOrCall(
      FROM_HERE, SiteDataCacheFacadeFactory::GetInstance()->cache_factory(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> cb,
             const std::string& browser_context_id,
             SiteDataCacheFactory* cache_factory) {
            std::move(cb).Run(
                cache_factory->IsDataCacheRecordingForTesting(  // IN-TEST
                    browser_context_id));
          },
          std::move(cb), browser_context_->UniqueId()));
}

void SiteDataCacheFacade::WaitUntilCacheInitializedForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  PostOrCall(
      FROM_HERE, SiteDataCacheFacadeFactory::GetInstance()->cache_factory(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::string& browser_context_id,
             SiteDataCacheFactory* cache_factory) {
            auto* cache = cache_factory->GetDataCacheForBrowserContext(
                browser_context_id);
            if (cache->IsRecording()) {
              static_cast<SiteDataCacheImpl*>(cache)
                  ->SetInitializationCallbackForTesting(  // IN-TEST
                      std::move(quit_closure));
            }
          },
          run_loop.QuitClosure(), browser_context_->UniqueId()));
  run_loop.Run();
}

void SiteDataCacheFacade::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deletion_info.IsAllHistory()) {
    if (!browser_context_->IsOffTheRecord()) {
      base::UmaHistogramBoolean(
          "PerformanceManager.SiteDB.WriteScheduled.ClearAllSiteData", true);
    }
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

  if (!browser_context_->IsOffTheRecord()) {
    base::UmaHistogramBoolean(
        "PerformanceManager.SiteDB.WriteScheduled.ClearSiteDataForOrigins",
        true);
  }
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
  PostOrCall(FROM_HERE,
             SiteDataCacheFacadeFactory::GetInstance()->cache_factory(),
             std::move(clear_site_data_cb));
}

void SiteDataCacheFacade::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(history_observation_.IsObservingSource(history_service));
  history_observation_.Reset();
}

void SiteDataCacheFacade::ClearAllSiteData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto clear_all_site_data_cb = base::BindOnce(
      [](const std::string& browser_context_id,
         SiteDataCacheFactory* cache_factory) {
        auto* cache =
            cache_factory->GetDataCacheForBrowserContext(browser_context_id);
        if (cache->IsRecording()) {
          static_cast<SiteDataCacheImpl*>(cache)->ClearAllSiteData();
        }
      },
      browser_context_->UniqueId());
  PostOrCall(FROM_HERE,
             SiteDataCacheFacadeFactory::GetInstance()->cache_factory(),
             std::move(clear_all_site_data_cb));
}

}  // namespace performance_manager
