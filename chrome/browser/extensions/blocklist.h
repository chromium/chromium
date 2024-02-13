// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKLIST_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKLIST_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace extensions {

class BlocklistStateFetcher;

// The blocklist of extensions backed by safe browsing.
class Blocklist : public KeyedService {
 public:
  class Observer {
   public:
    // Observes |blocklist| on construction and unobserves on destruction.
    explicit Observer(Blocklist* blocklist);

    virtual void OnBlocklistUpdated() = 0;

   protected:
    virtual ~Observer();

   private:
    raw_ptr<Blocklist> blocklist_;
  };

  using BlocklistStateMap = std::map<ExtensionId, BlocklistState>;

  using GetBlocklistedIDsCallback =
      base::OnceCallback<void(const BlocklistStateMap&)>;

  using GetMalwareIDsCallback =
      base::OnceCallback<void(const std::set<ExtensionId>&)>;

  using IsBlocklistedCallback = base::OnceCallback<void(BlocklistState)>;

  using DatabaseReadyCallback = base::OnceCallback<void(bool)>;

  Blocklist();

  Blocklist(const Blocklist&) = delete;
  Blocklist& operator=(const Blocklist&) = delete;

  ~Blocklist() override;

  static Blocklist* Get(content::BrowserContext* context);

  // From the set of extension IDs passed in via |ids|, asynchronously checks
  // which are blocklisted and includes them in the resulting map passed
  // via |callback|, which will be sent on the caller's message loop. The values
  // of the map are the blocklist state for each extension. Extensions with
  // a BlocklistState of NOT_BLOCKLISTED are not included in the result.
  //
  // For a synchronous version which ONLY CHECKS CURRENTLY INSTALLED EXTENSIONS
  // see ExtensionPrefs::IsExtensionBlocklisted.
  void GetBlocklistedIDs(const std::set<ExtensionId>& ids,
                         GetBlocklistedIDsCallback callback);

  // From the subset of extension IDs passed in via |ids|, select the ones
  // marked in the blocklist as BLOCKLISTED_MALWARE and asynchronously pass
  // to |callback|. Basically, will call GetBlocklistedIDs and filter its
  // results.
  void GetMalwareIDs(const std::set<ExtensionId>& ids,
                     GetMalwareIDsCallback callback);

  // More convenient form of GetBlocklistedIDs for checking a single extension.
  void IsBlocklisted(const ExtensionId& extension_id,
                     IsBlocklistedCallback callback);

  // Used to mock BlocklistStateFetcher in unit tests. Blocklist owns the
  // |fetcher|.
  void SetBlocklistStateFetcherForTest(BlocklistStateFetcher* fetcher);

  // Reset the owned BlocklistStateFetcher to null and return the current
  // BlocklistStateFetcher.
  BlocklistStateFetcher* ResetBlocklistStateFetcherForTest();

  // Reset the listening for an updated database.
  void ResetDatabaseUpdatedListenerForTest();

  // Reset blocklist state cache to make sure the blocklist state is
  // fetched from the blocklist state fetcher.
  void ResetBlocklistStateCacheForTest();

  // Adds/removes an observer to the blocklist.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Invokes the callback method with a boolean indicating
  // whether the database is ready.
  void IsDatabaseReady(DatabaseReadyCallback callback);

 private:
  friend class ScopedDatabaseManagerForTest;

  // Use via ScopedDatabaseManagerForTest.
  static void SetDatabaseManager(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);
  static scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetDatabaseManager();

  void ObserveNewDatabase();

  void NotifyObservers();

  void GetBlocklistStateForIDs(GetBlocklistedIDsCallback callback,
                               const std::set<ExtensionId>& blocklisted_ids);

  void RequestExtensionsBlocklistState(const std::set<ExtensionId>& ids,
                                       base::OnceClosure callback);

  void OnBlocklistStateReceived(const ExtensionId& id, BlocklistState state);

  void ReturnBlocklistStateMap(GetBlocklistedIDsCallback callback,
                               const std::set<ExtensionId>& blocklisted_ids);

  base::ObserverList<Observer>::Unchecked observers_;

  base::CallbackListSubscription database_updated_subscription_;
  base::CallbackListSubscription database_changed_subscription_;

  // The cached BlocklistState's, received from BlocklistStateFetcher.
  BlocklistStateMap blocklist_state_cache_;

  std::unique_ptr<BlocklistStateFetcher> state_fetcher_;

  // The list of ongoing requests for blocklist states that couldn't be
  // served directly from the cache. A new request is created in
  // GetBlocklistedIDs and deleted when the callback is called from
  // OnBlocklistStateReceived.
  //
  // This is a list of requests. Each item in the list is a request. A request
  // is a pair of [vector of string ids to check, response closure].
  std::list<std::pair<std::vector<std::string>, base::OnceClosure>>
      state_requests_;

  base::WeakPtrFactory<Blocklist> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKLIST_H_
