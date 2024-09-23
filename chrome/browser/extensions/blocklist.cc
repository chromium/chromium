// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/blocklist_factory.h"
#include "chrome/browser/extensions/blocklist_state_fetcher.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_id.h"

using content::BrowserThread;
using safe_browsing::SafeBrowsingDatabaseManager;

namespace extensions {

namespace {

// The safe browsing database manager to use. Make this a global/static variable
// rather than a member of Blocklist because Blocklist accesses the real
// database manager before it has a chance to get a fake one.
class LazySafeBrowsingDatabaseManager {
 public:
  LazySafeBrowsingDatabaseManager() {
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      instance_ =
          g_browser_process->safe_browsing_service()->database_manager();
    }
#endif
  }

  scoped_refptr<SafeBrowsingDatabaseManager> get() { return instance_; }

  void set(scoped_refptr<SafeBrowsingDatabaseManager> instance) {
    instance_ = instance;
    database_changed_callback_list_.Notify();
  }

  base::CallbackListSubscription RegisterDatabaseChangedCallback(
      const base::RepeatingClosure& cb) {
    return database_changed_callback_list_.Add(cb);
  }

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> instance_;
  base::RepeatingClosureList database_changed_callback_list_;
};

static base::LazyInstance<LazySafeBrowsingDatabaseManager>::DestructorAtExit
    g_database_manager = LAZY_INSTANCE_INITIALIZER;

// Implementation of SafeBrowsingDatabaseManager::Client, the class which is
// called back from safebrowsing queries.
//
// Constructed on any thread but lives on the IO from then on.
class SafeBrowsingClientImpl
    : public SafeBrowsingDatabaseManager::Client,
      public base::RefCountedThreadSafe<SafeBrowsingClientImpl> {
 public:
  using OnResultCallback =
      base::OnceCallback<void(const std::set<ExtensionId>&)>;

  SafeBrowsingClientImpl(const SafeBrowsingClientImpl&) = delete;
  SafeBrowsingClientImpl& operator=(const SafeBrowsingClientImpl&) = delete;

  // Constructs a client to query the database manager for |extension_ids| and
  // run |callback| with the IDs of those which have been blocklisted.
  static void Start(const std::set<ExtensionId>& extension_ids,
                    OnResultCallback callback) {
    auto safe_browsing_client = base::WrapRefCounted(
        new SafeBrowsingClientImpl(extension_ids, std::move(callback)));
    safe_browsing_client->StartCheck(g_database_manager.Get().get(),
                                     extension_ids);
  }

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingClientImpl>;

  SafeBrowsingClientImpl(const std::set<ExtensionId>& extension_ids,
                         OnResultCallback callback)
      : callback_task_runner_(
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        callback_(std::move(callback)) {}

  ~SafeBrowsingClientImpl() override {}

  // Pass |database_manager| as a parameter to avoid touching
  // SafeBrowsingService on the IO thread.
  void StartCheck(scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
                  const std::set<ExtensionId>& extension_ids) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (database_manager->CheckExtensionIDs(extension_ids, this)) {
      // Definitely not blocklisted. Callback immediately.
      callback_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), std::set<ExtensionId>()));
      return;
    }
    // Something might be blocklisted, response will come in
    // OnCheckExtensionsResult.
    AddRef();  // Balanced in OnCheckExtensionsResult
  }

  void OnCheckExtensionsResult(const std::set<ExtensionId>& hits) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::move(callback_).Run(hits);
    Release();  // Balanced in StartCheck.
  }

  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  OnResultCallback callback_;
};

void CheckOneExtensionState(Blocklist::IsBlocklistedCallback callback,
                            const Blocklist::BlocklistStateMap& state_map) {
  std::move(callback).Run(state_map.empty() ? NOT_BLOCKLISTED
                                            : state_map.begin()->second);
}

void GetMalwareFromBlocklistStateMap(
    Blocklist::GetMalwareIDsCallback callback,
    const Blocklist::BlocklistStateMap& state_map) {
  std::set<ExtensionId> malware;
  for (const auto& state_pair : state_map) {
    // TODO(oleg): UNKNOWN is treated as MALWARE for backwards compatibility.
    // In future GetMalwareIDs will be removed and the caller will have to
    // deal with BLOCKLISTED_UNKNOWN state returned from GetBlocklistedIDs.
    if (state_pair.second == BLOCKLISTED_MALWARE ||
        state_pair.second == BLOCKLISTED_UNKNOWN) {
      malware.insert(state_pair.first);
    }
  }
  std::move(callback).Run(malware);
}

}  // namespace

Blocklist::Observer::Observer(Blocklist* blocklist) : blocklist_(blocklist) {
  blocklist_->AddObserver(this);
}

Blocklist::Observer::~Observer() {
  blocklist_->RemoveObserver(this);
}

Blocklist::Blocklist() {
  auto& lazy_database_manager = g_database_manager.Get();
  // Using base::Unretained is safe because when this object goes away, the
  // subscription will automatically be destroyed.
  database_changed_subscription_ =
      lazy_database_manager.RegisterDatabaseChangedCallback(base::BindRepeating(
          &Blocklist::ObserveNewDatabase, base::Unretained(this)));

  ObserveNewDatabase();
}

Blocklist::~Blocklist() {}

// static
Blocklist* Blocklist::Get(content::BrowserContext* context) {
  return BlocklistFactory::GetForBrowserContext(context);
}

void Blocklist::GetBlocklistedIDs(const std::set<ExtensionId>& ids,
                                  GetBlocklistedIDsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (ids.empty() || !GetDatabaseManager().get()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), BlocklistStateMap()));
    return;
  }

  // Constructing the SafeBrowsingClientImpl begins the process of asking
  // safebrowsing for the blocklisted extensions. The set of blocklisted
  // extensions returned by SafeBrowsing will then be passed to
  // GetBlocklistStateIDs to get the particular BlocklistState for each id.
  SafeBrowsingClientImpl::Start(
      ids, base::BindOnce(&Blocklist::GetBlocklistStateForIDs,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Blocklist::GetMalwareIDs(const std::set<ExtensionId>& ids,
                              GetMalwareIDsCallback callback) {
  GetBlocklistedIDs(ids, base::BindOnce(&GetMalwareFromBlocklistStateMap,
                                        std::move(callback)));
}

void Blocklist::IsBlocklisted(const ExtensionId& extension_id,
                              IsBlocklistedCallback callback) {
  std::set<ExtensionId> check;
  check.insert(extension_id);
  GetBlocklistedIDs(
      check, base::BindOnce(&CheckOneExtensionState, std::move(callback)));
}

void Blocklist::GetBlocklistStateForIDs(
    GetBlocklistedIDsCallback callback,
    const std::set<ExtensionId>& blocklisted_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<ExtensionId> ids_unknown_state;
  BlocklistStateMap extensions_state;
  for (const auto& blocklisted_id : blocklisted_ids) {
    auto cache_it = blocklist_state_cache_.find(blocklisted_id);
    if (cache_it == blocklist_state_cache_.end() ||
        cache_it->second ==
            BLOCKLISTED_UNKNOWN) {  // Do not return UNKNOWN
                                    // from cache, retry request.
      ids_unknown_state.insert(blocklisted_id);
    } else {
      extensions_state[blocklisted_id] = cache_it->second;
    }
  }

  if (ids_unknown_state.empty()) {
    std::move(callback).Run(extensions_state);
  } else {
    // After the extension blocklist states have been downloaded, call this
    // functions again, but prevent infinite cycle in case server is offline
    // or some other reason prevents us from receiving the blocklist state for
    // these extensions.
    RequestExtensionsBlocklistState(
        ids_unknown_state,
        base::BindOnce(&Blocklist::ReturnBlocklistStateMap,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       blocklisted_ids));
  }
}

void Blocklist::ReturnBlocklistStateMap(
    GetBlocklistedIDsCallback callback,
    const std::set<ExtensionId>& blocklisted_ids) {
  BlocklistStateMap extensions_state;
  for (const auto& blocklisted_id : blocklisted_ids) {
    auto cache_it = blocklist_state_cache_.find(blocklisted_id);
    if (cache_it != blocklist_state_cache_.end())
      extensions_state[blocklisted_id] = cache_it->second;
    // If for some reason we still haven't cached the state of this extension,
    // we silently skip it.
  }

  std::move(callback).Run(extensions_state);
}

void Blocklist::RequestExtensionsBlocklistState(
    const std::set<ExtensionId>& ids,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!state_fetcher_)
    state_fetcher_ = std::make_unique<BlocklistStateFetcher>();

  state_requests_.emplace_back(std::vector<ExtensionId>(ids.begin(), ids.end()),
                               std::move(callback));
  for (const auto& id : ids) {
    state_fetcher_->Request(id,
                            base::BindOnce(&Blocklist::OnBlocklistStateReceived,
                                           weak_ptr_factory_.GetWeakPtr(), id));
  }
}

void Blocklist::OnBlocklistStateReceived(const ExtensionId& id,
                                         BlocklistState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blocklist_state_cache_[id] = state;

  // Go through the opened requests and call the callbacks for those requests
  // for which we already got all the required blocklist states.
  auto requests_it = state_requests_.begin();
  while (requests_it != state_requests_.end()) {
    const std::vector<ExtensionId>& ids = requests_it->first;

    bool have_all_in_cache = true;
    for (const auto& id_str : ids) {
      if (!base::Contains(blocklist_state_cache_, id_str)) {
        have_all_in_cache = false;
        break;
      }
    }

    if (have_all_in_cache) {
      std::move(requests_it->second).Run();
      requests_it = state_requests_.erase(requests_it);  // returns next element
    } else {
      ++requests_it;
    }
  }
}

void Blocklist::SetBlocklistStateFetcherForTest(
    BlocklistStateFetcher* fetcher) {
  state_fetcher_.reset(fetcher);
}

BlocklistStateFetcher* Blocklist::ResetBlocklistStateFetcherForTest() {
  return state_fetcher_.release();
}

void Blocklist::ResetDatabaseUpdatedListenerForTest() {
  database_updated_subscription_ = {};
}

void Blocklist::ResetBlocklistStateCacheForTest() {
  blocklist_state_cache_.clear();
}

void Blocklist::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void Blocklist::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void Blocklist::IsDatabaseReady(DatabaseReadyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto database_manager = GetDatabaseManager();
  if (!database_manager) {
    std::move(callback).Run(false);
    return;
  }

  // Need to post task to this thread because the safe browsing database is
  // initialized after this point in startup.
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SafeBrowsingDatabaseManager::IsDatabaseReady,
                     database_manager),
      base::BindOnce(
          [](base::WeakPtr<Blocklist> blocklist_service,
             DatabaseReadyCallback callback, bool is_ready) {
            std::move(callback).Run(blocklist_service && is_ready);
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// static
void Blocklist::SetDatabaseManager(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager) {
  g_database_manager.Get().set(database_manager);
}

// static
scoped_refptr<SafeBrowsingDatabaseManager> Blocklist::GetDatabaseManager() {
  return g_database_manager.Get().get();
}

void Blocklist::ObserveNewDatabase() {
  auto database_manager = GetDatabaseManager();
  if (database_manager.get()) {
    // Using base::Unretained is safe because when this object goes away, the
    // subscription from the callback list will automatically be destroyed.
    database_updated_subscription_ =
        database_manager.get()->RegisterDatabaseUpdatedCallback(
            base::BindRepeating(&Blocklist::NotifyObservers,
                                base::Unretained(this)));
  } else {
    database_updated_subscription_ = {};
  }
}

void Blocklist::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnBlocklistUpdated();
}

}  // namespace extensions
