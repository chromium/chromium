// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/blacklist_factory.h"
#include "chrome/browser/extensions/blacklist_state_fetcher.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/db/util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"

using content::BrowserThread;
using safe_browsing::SafeBrowsingDatabaseManager;

namespace extensions {

namespace {

// The safe browsing database manager to use. Make this a global/static variable
// rather than a member of Blacklist because Blacklist accesses the real
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

  scoped_refptr<SafeBrowsingDatabaseManager> get() {
    return instance_;
  }

  void set(scoped_refptr<SafeBrowsingDatabaseManager> instance) {
    instance_ = instance;
    database_changed_callback_list_.Notify();
  }

  std::unique_ptr<base::CallbackList<void()>::Subscription>
  RegisterDatabaseChangedCallback(const base::RepeatingClosure& cb) {
    return database_changed_callback_list_.Add(cb);
  }

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> instance_;
  base::CallbackList<void()> database_changed_callback_list_;
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
  using OnResultCallback = base::Callback<void(const std::set<std::string>&)>;

  // Constructs a client to query the database manager for |extension_ids| and
  // run |callback| with the IDs of those which have been blacklisted.
  static void Start(
      const std::set<std::string>& extension_ids,
      const OnResultCallback& callback) {
    auto safe_browsing_client = base::WrapRefCounted(
        new SafeBrowsingClientImpl(extension_ids, callback));
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SafeBrowsingClientImpl::StartCheck,
                       safe_browsing_client, g_database_manager.Get().get(),
                       extension_ids));
  }

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingClientImpl>;

  SafeBrowsingClientImpl(const std::set<std::string>& extension_ids,
                         const OnResultCallback& callback)
      : callback_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(callback) {}

  ~SafeBrowsingClientImpl() override {}

  // Pass |database_manager| as a parameter to avoid touching
  // SafeBrowsingService on the IO thread.
  void StartCheck(scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
                  const std::set<std::string>& extension_ids) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (database_manager->CheckExtensionIDs(extension_ids, this)) {
      // Definitely not blacklisted. Callback immediately.
      callback_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(callback_, std::set<std::string>()));
      return;
    }
    // Something might be blacklisted, response will come in
    // OnCheckExtensionsResult.
    AddRef();  // Balanced in OnCheckExtensionsResult
  }

  void OnCheckExtensionsResult(const std::set<std::string>& hits) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    callback_task_runner_->PostTask(FROM_HERE, base::BindOnce(callback_, hits));
    Release();  // Balanced in StartCheck.
  }

  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  OnResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingClientImpl);
};

void CheckOneExtensionState(
    const Blacklist::IsBlacklistedCallback& callback,
    const Blacklist::BlacklistStateMap& state_map) {
  callback.Run(state_map.empty() ? NOT_BLACKLISTED : state_map.begin()->second);
}

void GetMalwareFromBlacklistStateMap(
    const Blacklist::GetMalwareIDsCallback& callback,
    const Blacklist::BlacklistStateMap& state_map) {
  std::set<std::string> malware;
  for (const auto& state_pair : state_map) {
    // TODO(oleg): UNKNOWN is treated as MALWARE for backwards compatibility.
    // In future GetMalwareIDs will be removed and the caller will have to
    // deal with BLACKLISTED_UNKNOWN state returned from GetBlacklistedIDs.
    if (state_pair.second == BLACKLISTED_MALWARE ||
        state_pair.second == BLACKLISTED_UNKNOWN) {
      malware.insert(state_pair.first);
    }
  }
  callback.Run(malware);
}

}  // namespace

Blacklist::Observer::Observer(Blacklist* blacklist) : blacklist_(blacklist) {
  blacklist_->AddObserver(this);
}

Blacklist::Observer::~Observer() {
  blacklist_->RemoveObserver(this);
}

Blacklist::ScopedDatabaseManagerForTest::ScopedDatabaseManagerForTest(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : original_(GetDatabaseManager()) {
  SetDatabaseManager(database_manager);
}

Blacklist::ScopedDatabaseManagerForTest::~ScopedDatabaseManagerForTest() {
  SetDatabaseManager(original_);
}

Blacklist::Blacklist(ExtensionPrefs* prefs) {
  auto& lazy_database_manager = g_database_manager.Get();
  // Using base::Unretained is safe because when this object goes away, the
  // subscription will automatically be destroyed.
  database_changed_subscription_ =
      lazy_database_manager.RegisterDatabaseChangedCallback(base::BindRepeating(
          &Blacklist::ObserveNewDatabase, base::Unretained(this)));

  ObserveNewDatabase();
}

Blacklist::~Blacklist() {
}

// static
Blacklist* Blacklist::Get(content::BrowserContext* context) {
  return BlacklistFactory::GetForBrowserContext(context);
}

void Blacklist::GetBlacklistedIDs(const std::set<std::string>& ids,
                                  const GetBlacklistedIDsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (ids.empty() || !GetDatabaseManager().get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, BlacklistStateMap()));
    return;
  }

  // Constructing the SafeBrowsingClientImpl begins the process of asking
  // safebrowsing for the blacklisted extensions. The set of blacklisted
  // extensions returned by SafeBrowsing will then be passed to
  // GetBlacklistStateIDs to get the particular BlacklistState for each id.
  SafeBrowsingClientImpl::Start(
      ids,
      base::Bind(&Blacklist::GetBlacklistStateForIDs, AsWeakPtr(), callback));
}

void Blacklist::GetMalwareIDs(const std::set<std::string>& ids,
                              const GetMalwareIDsCallback& callback) {
  GetBlacklistedIDs(ids, base::Bind(&GetMalwareFromBlacklistStateMap,
                                    callback));
}


void Blacklist::IsBlacklisted(const std::string& extension_id,
                              const IsBlacklistedCallback& callback) {
  std::set<std::string> check;
  check.insert(extension_id);
  GetBlacklistedIDs(check, base::Bind(&CheckOneExtensionState, callback));
}

void Blacklist::GetBlacklistStateForIDs(
    const GetBlacklistedIDsCallback& callback,
    const std::set<std::string>& blacklisted_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<std::string> ids_unknown_state;
  BlacklistStateMap extensions_state;
  for (const auto& blacklisted_id : blacklisted_ids) {
    auto cache_it = blacklist_state_cache_.find(blacklisted_id);
    if (cache_it == blacklist_state_cache_.end() ||
        cache_it->second ==
            BLACKLISTED_UNKNOWN) {  // Do not return UNKNOWN
                                    // from cache, retry request.
      ids_unknown_state.insert(blacklisted_id);
    } else {
      extensions_state[blacklisted_id] = cache_it->second;
    }
  }

  if (ids_unknown_state.empty()) {
    callback.Run(extensions_state);
  } else {
    // After the extension blacklist states have been downloaded, call this
    // functions again, but prevent infinite cycle in case server is offline
    // or some other reason prevents us from receiving the blacklist state for
    // these extensions.
    RequestExtensionsBlacklistState(
        ids_unknown_state,
        base::BindOnce(&Blacklist::ReturnBlacklistStateMap, AsWeakPtr(),
                       callback, blacklisted_ids));
  }
}

void Blacklist::ReturnBlacklistStateMap(
    const GetBlacklistedIDsCallback& callback,
    const std::set<std::string>& blacklisted_ids) {
  BlacklistStateMap extensions_state;
  for (const auto& blacklisted_id : blacklisted_ids) {
    auto cache_it = blacklist_state_cache_.find(blacklisted_id);
    if (cache_it != blacklist_state_cache_.end())
      extensions_state[blacklisted_id] = cache_it->second;
    // If for some reason we still haven't cached the state of this extension,
    // we silently skip it.
  }

  callback.Run(extensions_state);
}

void Blacklist::RequestExtensionsBlacklistState(
    const std::set<std::string>& ids,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!state_fetcher_)
    state_fetcher_.reset(new BlacklistStateFetcher());

  state_requests_.emplace_back(std::vector<std::string>(ids.begin(), ids.end()),
                               std::move(callback));
  for (const auto& id : ids) {
    state_fetcher_->Request(
        id, base::Bind(&Blacklist::OnBlacklistStateReceived, AsWeakPtr(), id));
  }
}

void Blacklist::OnBlacklistStateReceived(const std::string& id,
                                         BlacklistState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blacklist_state_cache_[id] = state;

  // Go through the opened requests and call the callbacks for those requests
  // for which we already got all the required blacklist states.
  auto requests_it = state_requests_.begin();
  while (requests_it != state_requests_.end()) {
    const std::vector<std::string>& ids = requests_it->first;

    bool have_all_in_cache = true;
    for (const auto& id : ids) {
      if (!base::Contains(blacklist_state_cache_, id)) {
        have_all_in_cache = false;
        break;
      }
    }

    if (have_all_in_cache) {
      std::move(requests_it->second).Run();
      requests_it = state_requests_.erase(requests_it); // returns next element
    } else {
      ++requests_it;
    }
  }
}

void Blacklist::SetBlacklistStateFetcherForTest(
    BlacklistStateFetcher* fetcher) {
  state_fetcher_.reset(fetcher);
}

BlacklistStateFetcher* Blacklist::ResetBlacklistStateFetcherForTest() {
  return state_fetcher_.release();
}

void Blacklist::ResetDatabaseUpdatedListenerForTest() {
  database_updated_subscription_.reset();
}

void Blacklist::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void Blacklist::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

// static
void Blacklist::SetDatabaseManager(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager) {
  g_database_manager.Get().set(database_manager);
}

// static
scoped_refptr<SafeBrowsingDatabaseManager> Blacklist::GetDatabaseManager() {
  return g_database_manager.Get().get();
}

void Blacklist::ObserveNewDatabase() {
  auto database_manager = GetDatabaseManager();
  if (database_manager.get()) {
    // Using base::Unretained is safe because when this object goes away, the
    // subscription to the callback list will automatically be destroyed.
    database_updated_subscription_ =
        database_manager.get()->RegisterDatabaseUpdatedCallback(
            base::BindRepeating(&Blacklist::NotifyObservers,
                                base::Unretained(this)));
  } else {
    database_updated_subscription_.reset();
  }
}

void Blacklist::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnBlacklistUpdated();
}

}  // namespace extensions
