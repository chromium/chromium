// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_WHITELIST_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_WHITELIST_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/sync/model/syncable_service.h"

class PrefService;
class SupervisedUserSiteList;

namespace base {
class DictionaryValue;
class FilePath;
}

namespace component_updater {
class SupervisedUserWhitelistInstaller;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_pb {
class ManagedUserWhitelistSpecifics;
}

class SupervisedUserWhitelistService : public syncer::SyncableService {
 public:
  typedef base::Callback<void(
      const std::vector<scoped_refptr<SupervisedUserSiteList> >&)>
      SiteListsChangedCallback;

  SupervisedUserWhitelistService(
      PrefService* prefs,
      component_updater::SupervisedUserWhitelistInstaller* installer,
      const std::string& client_id);
  ~SupervisedUserWhitelistService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Init();

  // Adds a callback to be called when the list of loaded site lists changes.
  // The callback will also be called immediately, to get the current
  // site lists.
  void AddSiteListsChangedCallback(const SiteListsChangedCallback& callback);

  // Returns a map (from CRX ID to name) of whitelists to be installed,
  // specified on the command line.
  static std::map<std::string, std::string> GetWhitelistsFromCommandLine();

  // Loads an already existing whitelist on disk (i.e. without downloading it as
  // a component).
  void LoadWhitelistForTesting(const std::string& id,
                               const base::string16& title,
                               const base::FilePath& path);

  // Unloads a whitelist. Public for testing.
  void UnloadWhitelist(const std::string& id);

  // Creates Sync data for a whitelist with the given |id| and |name|.
  // Public for testing.
  static syncer::SyncData CreateWhitelistSyncData(const std::string& id,
                                                  const std::string& name);

  // SyncableService implementation:
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

 private:
  // The following methods handle whitelist additions, updates and removals,
  // usually coming from Sync.
  void AddNewWhitelist(base::DictionaryValue* pref_dict,
                       const sync_pb::ManagedUserWhitelistSpecifics& whitelist);
  void SetWhitelistProperties(
      base::DictionaryValue* pref_dict,
      const sync_pb::ManagedUserWhitelistSpecifics& whitelist);
  void RemoveWhitelist(base::DictionaryValue* pref_dict, const std::string& id);

  enum WhitelistSource {
    FROM_SYNC,
    FROM_COMMAND_LINE,
  };

  // Registers a new or existing whitelist.
  void RegisterWhitelist(const std::string& id,
                         const std::string& name,
                         WhitelistSource source);

  void GetLoadedWhitelists(
      std::vector<scoped_refptr<SupervisedUserSiteList>>* whitelists);

  void NotifyWhitelistsChanged();

  void OnWhitelistReady(const std::string& id,
                        const base::string16& title,
                        const base::FilePath& large_icon_path,
                        const base::FilePath& whitelist_path);
  void OnWhitelistLoaded(
      const std::string& id,
      base::TimeTicks start_time,
      const scoped_refptr<SupervisedUserSiteList>& whitelist);

  PrefService* prefs_;
  component_updater::SupervisedUserWhitelistInstaller* installer_;

  std::string client_id_;
  std::vector<SiteListsChangedCallback> site_lists_changed_callbacks_;

  // The set of registered whitelists. A whitelist might be registered but not
  // loaded yet, in which case it will not be in |loaded_whitelists_| yet.
  // On the other hand, every loaded whitelist has to be registered.
  std::set<std::string> registered_whitelists_;
  std::map<std::string, scoped_refptr<SupervisedUserSiteList> >
      loaded_whitelists_;

  base::WeakPtrFactory<SupervisedUserWhitelistService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWhitelistService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_WHITELIST_SERVICE_H_
