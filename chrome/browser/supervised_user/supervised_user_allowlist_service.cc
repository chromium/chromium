// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_allowlist_service.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"

const char kName[] = "name";

SupervisedUserAllowlistService::SupervisedUserAllowlistService(
    PrefService* prefs,
    component_updater::SupervisedUserWhitelistInstaller* installer,
    const std::string& client_id)
    : prefs_(prefs), installer_(installer), client_id_(client_id) {
  DCHECK(prefs);
}

SupervisedUserAllowlistService::~SupervisedUserAllowlistService() {}

// static
void SupervisedUserAllowlistService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kSupervisedUserAllowlists);
}

void SupervisedUserAllowlistService::Init() {
  const base::DictionaryValue* allowlists =
      prefs_->GetDictionary(prefs::kSupervisedUserAllowlists);
  for (base::DictionaryValue::Iterator it(*allowlists); !it.IsAtEnd();
       it.Advance()) {
    registered_allowlists_.insert(it.key());
  }
  UMA_HISTOGRAM_COUNTS_100("ManagedUsers.Whitelist.Count", allowlists->size());

  // The installer can be null in some unit tests.
  if (!installer_)
    return;

  installer_->Subscribe(
      base::BindRepeating(&SupervisedUserAllowlistService::OnAllowlistReady,
                          weak_ptr_factory_.GetWeakPtr()));

  // Register allowlists specified on the command line.
  for (const auto& allowlist : GetAllowlistsFromCommandLine())
    RegisterAllowlist(allowlist.first, allowlist.second, FROM_COMMAND_LINE);
}

void SupervisedUserAllowlistService::AddSiteListsChangedCallback(
    const SiteListsChangedCallback& callback) {
  site_lists_changed_callbacks_.push_back(callback);

  std::vector<scoped_refptr<SupervisedUserSiteList>> allowlists;
  GetLoadedAllowlists(&allowlists);
  callback.Run(allowlists);
}

// static
std::map<std::string, std::string>
SupervisedUserAllowlistService::GetAllowlistsFromCommandLine() {
  std::map<std::string, std::string> allowlists;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string command_line_allowlists = command_line->GetSwitchValueASCII(
      switches::kInstallSupervisedUserAllowlists);
  std::vector<base::StringPiece> string_pieces =
      base::SplitStringPiece(command_line_allowlists, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const base::StringPiece& allowlist : string_pieces) {
    std::string id;
    std::string name;
    size_t separator = allowlist.find(':');
    if (separator != base::StringPiece::npos) {
      id = std::string(allowlist.substr(0, separator));
      name = std::string(allowlist.substr(separator + 1));
    } else {
      id = std::string(allowlist);
    }

    const bool result = allowlists.insert(std::make_pair(id, name)).second;
    DCHECK(result);
  }

  return allowlists;
}

void SupervisedUserAllowlistService::LoadAllowlistForTesting(
    const std::string& id,
    const base::string16& title,
    const base::FilePath& path) {
  bool result = registered_allowlists_.insert(id).second;
  DCHECK(result);
  OnAllowlistReady(id, title, base::FilePath(), path);
}

void SupervisedUserAllowlistService::UnloadAllowlist(const std::string& id) {
  bool result = registered_allowlists_.erase(id) > 0u;
  DCHECK(result);
  loaded_allowlists_.erase(id);
  NotifyAllowlistsChanged();
}

// static
syncer::SyncData SupervisedUserAllowlistService::CreateAllowlistSyncData(
    const std::string& id,
    const std::string& name) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ManagedUserWhitelistSpecifics* allowlist =
      specifics.mutable_managed_user_whitelist();
  allowlist->set_id(id);
  allowlist->set_name(name);

  return syncer::SyncData::CreateLocalData(id, name, specifics);
}

void SupervisedUserAllowlistService::WaitUntilReadyToSync(
    base::OnceClosure done) {
  // This service handles sync events at any time.
  std::move(done).Run();
}

base::Optional<syncer::ModelError>
SupervisedUserAllowlistService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS, type);

  syncer::SyncChangeList change_list;

  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUserAllowlists);
  base::DictionaryValue* pref_dict = update.Get();
  std::set<std::string> seen_ids;

  for (const syncer::SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS,
              sync_data.GetDataType());
    const sync_pb::ManagedUserWhitelistSpecifics& allowlist =
        sync_data.GetSpecifics().managed_user_whitelist();
    std::string id = allowlist.id();
    std::string name = allowlist.name();
    seen_ids.insert(id);
    base::DictionaryValue* dict = nullptr;
    if (pref_dict->GetDictionary(id, &dict)) {
      std::string old_name;
      bool result = dict->GetString(kName, &old_name);
      DCHECK(result);
      if (name != old_name) {
        SetAllowlistProperties(dict, allowlist);
      }
    } else {
      AddNewAllowlist(pref_dict, allowlist);
    }
  }

  std::set<std::string> ids_to_remove;
  for (base::DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd();
       it.Advance()) {
    if (seen_ids.find(it.key()) == seen_ids.end())
      ids_to_remove.insert(it.key());
  }

  for (const std::string& id : ids_to_remove)
    RemoveAllowlist(pref_dict, id);

  // Notify if allowlists have been uninstalled. We will notify about newly
  // added allowlists later, when they are actually available
  // (in OnAllowlistLoaded).
  if (!ids_to_remove.empty())
    NotifyAllowlistsChanged();

  // The function does not generate any errors, so it can always return
  // base::nullopt.
  return base::nullopt;
}

void SupervisedUserAllowlistService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS, type);
}

syncer::SyncDataList SupervisedUserAllowlistService::GetAllSyncDataForTesting(
    syncer::ModelType type) const {
  syncer::SyncDataList sync_data;
  const base::DictionaryValue* allowlists =
      prefs_->GetDictionary(prefs::kSupervisedUserAllowlists);
  for (base::DictionaryValue::Iterator it(*allowlists); !it.IsAtEnd();
       it.Advance()) {
    const std::string& id = it.key();
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    std::string name;
    bool result = dict->GetString(kName, &name);
    DCHECK(result);
    sync_pb::EntitySpecifics specifics;
    sync_pb::ManagedUserWhitelistSpecifics* allowlist =
        specifics.mutable_managed_user_whitelist();
    allowlist->set_id(id);
    allowlist->set_name(name);
    sync_data.push_back(syncer::SyncData::CreateLocalData(id, name, specifics));
  }
  return sync_data;
}

base::Optional<syncer::ModelError>
SupervisedUserAllowlistService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  bool allowlists_removed = false;
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUserAllowlists);
  base::DictionaryValue* pref_dict = update.Get();
  for (const syncer::SyncChange& sync_change : change_list) {
    syncer::SyncData data = sync_change.sync_data();
    DCHECK_EQ(syncer::DEPRECATED_SUPERVISED_USER_ALLOWLISTS,
              data.GetDataType());
    const sync_pb::ManagedUserWhitelistSpecifics& allowlist =
        data.GetSpecifics().managed_user_whitelist();
    std::string id = allowlist.id();
    switch (sync_change.change_type()) {
      case syncer::SyncChange::ACTION_ADD: {
        DCHECK(!pref_dict->HasKey(id)) << id;
        AddNewAllowlist(pref_dict, allowlist);
        break;
      }
      case syncer::SyncChange::ACTION_UPDATE: {
        base::DictionaryValue* dict = nullptr;
        pref_dict->GetDictionaryWithoutPathExpansion(id, &dict);
        SetAllowlistProperties(dict, allowlist);
        break;
      }
      case syncer::SyncChange::ACTION_DELETE: {
        DCHECK(pref_dict->HasKey(id)) << id;
        RemoveAllowlist(pref_dict, id);
        allowlists_removed = true;
        break;
      }
    }
  }

  if (allowlists_removed)
    NotifyAllowlistsChanged();

  return base::nullopt;
}

void SupervisedUserAllowlistService::AddNewAllowlist(
    base::DictionaryValue* pref_dict,
    const sync_pb::ManagedUserWhitelistSpecifics& allowlist) {
  base::RecordAction(base::UserMetricsAction("ManagedUsers_Whitelist_Added"));

  RegisterAllowlist(allowlist.id(), allowlist.name(), FROM_SYNC);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  SetAllowlistProperties(dict.get(), allowlist);
  pref_dict->SetWithoutPathExpansion(allowlist.id(), std::move(dict));
}

void SupervisedUserAllowlistService::SetAllowlistProperties(
    base::DictionaryValue* dict,
    const sync_pb::ManagedUserWhitelistSpecifics& allowlist) {
  dict->SetString(kName, allowlist.name());
}

void SupervisedUserAllowlistService::RemoveAllowlist(
    base::DictionaryValue* pref_dict,
    const std::string& id) {
  base::RecordAction(base::UserMetricsAction("ManagedUsers_Whitelist_Removed"));

  pref_dict->RemoveKey(id);
  installer_->UnregisterWhitelist(client_id_, id);
  UnloadAllowlist(id);
}

void SupervisedUserAllowlistService::RegisterAllowlist(const std::string& id,
                                                       const std::string& name,
                                                       AllowlistSource source) {
  bool result = registered_allowlists_.insert(id).second;
  DCHECK(result);

  // Using an empty client ID for allowlists installed from the command line
  // causes the installer to not persist the installation, so the allowlist will
  // be removed the next time the browser is started without the command line
  // flag.
  installer_->RegisterWhitelist(
      source == FROM_COMMAND_LINE ? std::string() : client_id_, id, name);
}

void SupervisedUserAllowlistService::GetLoadedAllowlists(
    std::vector<scoped_refptr<SupervisedUserSiteList>>* allowlists) {
  for (const auto& allowlist : loaded_allowlists_)
    allowlists->push_back(allowlist.second);
}

void SupervisedUserAllowlistService::NotifyAllowlistsChanged() {
  std::vector<scoped_refptr<SupervisedUserSiteList>> allowlists;
  GetLoadedAllowlists(&allowlists);

  for (const auto& callback : site_lists_changed_callbacks_)
    callback.Run(allowlists);
}

void SupervisedUserAllowlistService::OnAllowlistReady(
    const std::string& id,
    const base::string16& title,
    const base::FilePath& large_icon_path,
    const base::FilePath& allowlist_path) {
  // If we did not register the allowlist or it has been unregistered in the
  // mean time, ignore it.
  if (registered_allowlists_.count(id) == 0u)
    return;

  SupervisedUserSiteList::Load(
      id, title, large_icon_path, allowlist_path,
      base::Bind(&SupervisedUserAllowlistService::OnAllowlistLoaded,
                 weak_ptr_factory_.GetWeakPtr(), id));
}

void SupervisedUserAllowlistService::OnAllowlistLoaded(
    const std::string& id,
    const scoped_refptr<SupervisedUserSiteList>& allowlist) {
  if (!allowlist) {
    LOG(WARNING) << "Couldn't load allowlist " << id;
    return;
  }
  // If the allowlist has been unregistered in the mean time, ignore it.
  if (registered_allowlists_.count(id) == 0u)
    return;

  loaded_allowlists_[id] = allowlist;
  NotifyAllowlistsChanged();
}
