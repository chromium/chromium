// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_settings_service.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/browser_thread.h"

using base::DictionaryValue;
using base::JSONReader;
using base::UserMetricsAction;
using base::Value;
using content::BrowserThread;
using syncer::SUPERVISED_USER_SETTINGS;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

const char kAtomicSettings[] = "atomic_settings";
const char kSupervisedUserInternalItemPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

namespace {

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !base::StartsWith(name, kSupervisedUserInternalItemPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

SupervisedUserSettingsService::SupervisedUserSettingsService()
    : active_(false),
      initialization_failed_(false),
      local_settings_(new base::DictionaryValue) {}

SupervisedUserSettingsService::~SupervisedUserSettingsService() {}

void SupervisedUserSettingsService::Init(
    base::FilePath profile_path,
    base::SequencedTaskRunner* sequenced_task_runner,
    bool load_synchronously) {
  base::FilePath path =
      profile_path.Append(chrome::kSupervisedUserSettingsFilename);
  PersistentPrefStore* store = new JsonPrefStore(
      path, std::unique_ptr<PrefFilter>(), sequenced_task_runner);
  Init(store);
  if (load_synchronously) {
    store_->ReadPrefs();
    DCHECK(IsReady());
  } else {
    store_->ReadPrefsAsync(nullptr);
  }
}

void SupervisedUserSettingsService::Init(
    scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_.get());
  store_ = store;
  store_->AddObserver(this);
}

std::unique_ptr<
    SupervisedUserSettingsService::SettingsCallbackList::Subscription>
SupervisedUserSettingsService::SubscribeForSettingsChange(
    const SettingsCallback& callback) {
  if (IsReady()) {
    std::unique_ptr<base::DictionaryValue> settings = GetSettings();
    callback.Run(settings.get());
  }

  return settings_callback_list_.Add(callback);
}

std::unique_ptr<
    SupervisedUserSettingsService::ShutdownCallbackList::Subscription>
SupervisedUserSettingsService::SubscribeForShutdown(
    const ShutdownCallback& callback) {
  return shutdown_callback_list_.Add(callback);
}

void SupervisedUserSettingsService::SetActive(bool active) {
  active_ = active;
  InformSubscribers();
}

bool SupervisedUserSettingsService::IsReady() const {
  // Initialization cannot be complete but have failed at the same time.
  DCHECK(!(store_->IsInitializationComplete() && initialization_failed_));
  return initialization_failed_ || store_->IsInitializationComplete();
}

void SupervisedUserSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->RemoveValue(kSplitSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

// static
std::string SupervisedUserSettingsService::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void SupervisedUserSettingsService::UploadItem(
    const std::string& key,
    std::unique_ptr<base::Value> value) {
  DCHECK(!SettingShouldApplyToPrefs(key));
  PushItemToSync(key, std::move(value));
}

void SupervisedUserSettingsService::PushItemToSync(
    const std::string& key,
    std::unique_ptr<base::Value> value) {
  std::string key_suffix = key;
  base::DictionaryValue* dict = nullptr;
  if (sync_processor_) {
    base::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Syncing"));
    dict = GetDictionaryAndSplitKey(&key_suffix);
    DCHECK(GetQueuedItems()->empty());
    SyncChangeList change_list;
    SyncData data = CreateSyncDataForSetting(key, *value);
    SyncChange::SyncChangeType change_type =
        dict->HasKey(key_suffix) ? SyncChange::ACTION_UPDATE
                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    SyncError error =
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
    DCHECK(!error.IsSet()) << error.ToString();
  } else {
    // Queue the item up to be uploaded when we start syncing
    // (in MergeDataAndStartSyncing()).
    base::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Queued"));
    dict = GetQueuedItems();
  }
  dict->SetWithoutPathExpansion(key_suffix, std::move(value));
}

void SupervisedUserSettingsService::SetLocalSetting(
    const std::string& key,
    std::unique_ptr<base::Value> value) {
  if (value)
    local_settings_->SetWithoutPathExpansion(key, std::move(value));
  else
    local_settings_->RemoveWithoutPathExpansion(key, nullptr);

  InformSubscribers();
}

// static
SyncData SupervisedUserSettingsService::CreateSyncDataForSetting(
    const std::string& name,
    const base::Value& value) {
  std::string json_value;
  base::JSONWriter::Write(value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_setting()->set_name(name);
  specifics.mutable_managed_user_setting()->set_value(json_value);
  return SyncData::CreateLocalData(name, name, specifics);
}

void SupervisedUserSettingsService::Shutdown() {
  store_->RemoveObserver(this);
  shutdown_callback_list_.Notify();
}

void SupervisedUserSettingsService::WaitUntilReadyToSync(
    base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);
  if (IsReady()) {
    std::move(done).Run();
  } else {
    // Wait until OnInitializationCompleted().
    wait_until_ready_to_sync_cb_ = std::move(done);
  }
}

SyncMergeResult SupervisedUserSettingsService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<SyncChangeProcessor> sync_processor,
    std::unique_ptr<SyncErrorFactory> error_handler) {
  DCHECK_EQ(SUPERVISED_USER_SETTINGS, type);
  sync_processor_ = std::move(sync_processor);
  error_handler_ = std::move(error_handler);

  std::set<std::string> seen_keys;
  int num_before_association = 0;
  // Getting number of atomic setting items.
  num_before_association = GetAtomicSettings()->size();
  for (base::DictionaryValue::Iterator it(*GetAtomicSettings()); !it.IsAtEnd();
       it.Advance()) {
    seen_keys.insert(it.key());
  }
  // Getting number of split setting items.
  for (base::DictionaryValue::Iterator it(*GetSplitSettings()); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    num_before_association += dict->size();
    for (base::DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd();
         jt.Advance()) {
      seen_keys.insert(MakeSplitSettingKey(it.key(), jt.key()));
    }
  }

  int num_deleted = num_before_association;
  // Getting number of queued items.
  base::DictionaryValue* queued_items = GetQueuedItems();
  num_before_association += queued_items->size();

  // Clear all atomic and split settings, then recreate them from Sync data.
  Clear();
  int num_added = 0;
  int num_modified = 0;
  std::set<std::string> added_sync_keys;
  for (const SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, sync_data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    std::unique_ptr<base::Value> value =
        JSONReader::ReadDeprecated(supervised_user_setting.value());
    // Wrongly formatted input will cause null values.
    // SetWithoutPathExpansion below requires non-null values.
    if (!value) {
      DLOG(ERROR) << "Invalid managed user setting value: "
                  << supervised_user_setting.value()
                  << ". Values must be JSON values.";
      continue;
    }
    std::string name_suffix = supervised_user_setting.name();
    std::string name_key = name_suffix;
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&name_suffix);
    dict->SetWithoutPathExpansion(name_suffix, std::move(value));
    if (seen_keys.find(name_key) == seen_keys.end()) {
      added_sync_keys.insert(name_key);
      num_added++;
    } else {
      num_modified++;
    }
  }

  num_deleted -= num_modified;

  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();

  // Upload all the queued up items (either with an ADD or an UPDATE action,
  // depending on whether they already exist) and move them to split settings.
  SyncChangeList change_list;
  for (base::DictionaryValue::Iterator it(*queued_items); !it.IsAtEnd();
       it.Advance()) {
    std::string key_suffix = it.key();
    std::string name_key = key_suffix;
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&key_suffix);
    SyncData data = CreateSyncDataForSetting(it.key(), it.value());
    SyncChange::SyncChangeType change_type =
        dict->HasKey(key_suffix) ? SyncChange::ACTION_UPDATE
                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    dict->SetKey(key_suffix, it.value().Clone());
    if (added_sync_keys.find(name_key) != added_sync_keys.end()) {
      num_added--;
    }
  }
  queued_items->Clear();

  SyncMergeResult result(SUPERVISED_USER_SETTINGS);
  // Process all the accumulated changes from the queued items.
  if (!change_list.empty()) {
    store_->ReportValueChanged(kQueuedItems,
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));
  }

  // Calculating number of items after association.
  int num_after_association = 0;
  // Getting number of atomic setting items.
  num_after_association = GetAtomicSettings()->size();
  // Getting number of split setting items.
  for (base::DictionaryValue::Iterator it(*GetSplitSettings()); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    num_after_association += dict->size();
  }
  // Getting number of queued items.
  queued_items = GetQueuedItems();
  num_after_association += queued_items->size();

  result.set_num_items_added(num_added);
  result.set_num_items_modified(num_modified);
  result.set_num_items_deleted(num_deleted);
  result.set_num_items_before_association(num_before_association);
  result.set_num_items_after_association(num_after_association);
  return result;
}

void SupervisedUserSettingsService::StopSyncing(ModelType type) {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  sync_processor_.reset();
  error_handler_.reset();
}

SyncDataList SupervisedUserSettingsService::GetAllSyncData(
    ModelType type) const {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  SyncDataList data;
  for (base::DictionaryValue::Iterator it(*GetAtomicSettings()); !it.IsAtEnd();
       it.Advance()) {
    data.push_back(CreateSyncDataForSetting(it.key(), it.value()));
  }
  for (base::DictionaryValue::Iterator it(*GetSplitSettings()); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    for (base::DictionaryValue::Iterator jt(*dict);
         !jt.IsAtEnd(); jt.Advance()) {
      data.push_back(CreateSyncDataForSetting(
          MakeSplitSettingKey(it.key(), jt.key()), jt.value()));
    }
  }
  DCHECK_EQ(0u, GetQueuedItems()->size());
  return data;
}

SyncError SupervisedUserSettingsService::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  for (const SyncChange& sync_change : change_list) {
    SyncData data = sync_change.sync_data();
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        data.GetSpecifics().managed_user_setting();
    std::string key = supervised_user_setting.name();
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&key);
    SyncChange::SyncChangeType change_type = sync_change.change_type();
    switch (change_type) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        std::unique_ptr<base::Value> value =
            JSONReader::ReadDeprecated(supervised_user_setting.value());
        if (dict->HasKey(key)) {
          DLOG_IF(WARNING, change_type == SyncChange::ACTION_ADD)
              << "Value for key " << key << " already exists";
        } else {
          DLOG_IF(WARNING, change_type == SyncChange::ACTION_UPDATE)
              << "Value for key " << key << " doesn't exist yet";
        }
        dict->SetWithoutPathExpansion(key, std::move(value));
        break;
      }
      case SyncChange::ACTION_DELETE: {
        DLOG_IF(WARNING, !dict->HasKey(key)) << "Trying to delete nonexistent "
                                             << "key " << key;
        dict->RemoveWithoutPathExpansion(key, nullptr);
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();

  SyncError error;
  return error;
}

void SupervisedUserSettingsService::OnPrefValueChanged(const std::string& key) {
}

void SupervisedUserSettingsService::OnInitializationCompleted(bool success) {
  if (!success) {
    // If this happens, it means the profile directory was not found. There is
    // not much we can do, but the whole profile will probably be useless
    // anyway. Just mark initialization as failed and continue otherwise,
    // because subscribers might still expect to be called back.
    initialization_failed_ = true;
  }

  DCHECK(IsReady());

  if (wait_until_ready_to_sync_cb_)
    std::move(wait_until_ready_to_sync_cb_).Run();

  InformSubscribers();
}

const base::DictionaryValue*
SupervisedUserSettingsService::LocalSettingsForTest() const {
  return local_settings_.get();
}

base::DictionaryValue* SupervisedUserSettingsService::GetDictionaryAndSplitKey(
    std::string* key) const {
  size_t pos = key->find_first_of(kSplitSettingKeySeparator);
  if (pos == std::string::npos)
    return GetAtomicSettings();

  base::DictionaryValue* split_settings = GetSplitSettings();
  std::string prefix = key->substr(0, pos);
  base::DictionaryValue* dict = nullptr;
  if (!split_settings->GetDictionary(prefix, &dict)) {
    DCHECK(!split_settings->HasKey(prefix));
    dict = split_settings->SetDictionary(
        prefix, std::make_unique<base::DictionaryValue>());
  }
  key->erase(0, pos + 1);
  return dict;
}

base::DictionaryValue* SupervisedUserSettingsService::GetOrCreateDictionary(
    const std::string& key) const {
  base::Value* value = nullptr;
  if (!store_->GetMutableValue(key, &value)) {
    store_->SetValue(
        key, std::make_unique<base::Value>(base::Value::Type::DICTIONARY),
        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    store_->GetMutableValue(key, &value);
  }
  base::DictionaryValue* dict = nullptr;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);
  return dict;
}

base::DictionaryValue*
SupervisedUserSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

base::DictionaryValue* SupervisedUserSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

base::DictionaryValue* SupervisedUserSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

std::unique_ptr<base::DictionaryValue>
SupervisedUserSettingsService::GetSettings() {
  DCHECK(IsReady());
  if (!active_ || initialization_failed_)
    return std::unique_ptr<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> settings(local_settings_->DeepCopy());

  base::DictionaryValue* atomic_settings = GetAtomicSettings();
  for (base::DictionaryValue::Iterator it(*atomic_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), std::make_unique<base::Value>(it.value().Clone()));
  }

  base::DictionaryValue* split_settings = GetSplitSettings();
  for (base::DictionaryValue::Iterator it(*split_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), std::make_unique<base::Value>(it.value().Clone()));
  }

  return settings;
}

void SupervisedUserSettingsService::InformSubscribers() {
  if (!IsReady())
    return;

  std::unique_ptr<base::DictionaryValue> settings = GetSettings();
  settings_callback_list_.Notify(settings.get());
}
