// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_store.h"
#include "components/sync/model/syncable_service.h"

class PersistentPrefStore;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

// This class syncs supervised user settings from a server, which are mapped to
// preferences. The downloaded settings are persisted in a PrefStore (which is
// not directly hooked up to the PrefService; it's just used internally).
// Settings are key-value pairs, where the key uniquely identifies the setting.
// The value is a string containing a JSON serialization of an arbitrary value,
// which is the value of the setting.
//
// There are two kinds of settings handled by this class: Atomic and split
// settings.
// Atomic settings consist of a single key (which will be mapped to a pref key)
// and a single (arbitrary) value.
// Split settings encode a dictionary value and are stored as multiple Sync
// items, one for each dictionary entry. The key for each of these Sync items
// is the key of the split setting, followed by a separator (':') and the key
// for the dictionary entry. The value of the Sync item is the value of the
// dictionary entry.
//
// As an example, a split setting with key "Moose" and value
//   {
//     "foo": "bar",
//     "baz": "blurp"
//   }
// would be encoded as two sync items, one with key "Moose:foo" and value "bar",
// and one with key "Moose:baz" and value "blurp".
class SupervisedUserSettingsService : public KeyedService,
                                      public syncer::SyncableService,
                                      public PrefStore::Observer {
 public:
  // A callback whose first parameter is a dictionary containing all supervised
  // user settings. If the dictionary is NULL, it means that the service is
  // inactive, i.e. the user is not supervised.
  using SettingsCallbackType = void(const base::DictionaryValue*);
  using SettingsCallback = base::Callback<SettingsCallbackType>;
  using SettingsCallbackList = base::CallbackList<SettingsCallbackType>;

  using ShutdownCallbackType = void();
  using ShutdownCallback = base::Callback<ShutdownCallbackType>;
  using ShutdownCallbackList = base::CallbackList<ShutdownCallbackType>;

  SupervisedUserSettingsService();
  ~SupervisedUserSettingsService() override;

  // Initializes the service by loading its settings from a file underneath the
  // |profile_path|. File I/O will be serialized via the
  // |sequenced_task_runner|. If |load_synchronously| is true, the settings will
  // be loaded synchronously, otherwise asynchronously.
  void Init(base::FilePath profile_path,
            base::SequencedTaskRunner* sequenced_task_runner,
            bool load_synchronously);

  // Initializes the service by loading its settings from the |pref_store|.
  // Use this method in tests to inject a different PrefStore than the
  // default one.
  void Init(scoped_refptr<PersistentPrefStore> pref_store);

  // Adds a callback to be called when supervised user settings are initially
  // available, or when they change.
  std::unique_ptr<SettingsCallbackList::Subscription>
  SubscribeForSettingsChange(const SettingsCallback& callback)
      WARN_UNUSED_RESULT;

  // Subscribe for a notification when the keyed service is shut down. The
  // subscription object can be destroyed to unsubscribe.
  std::unique_ptr<ShutdownCallbackList::Subscription> SubscribeForShutdown(
      const ShutdownCallback& callback);

  // Activates/deactivates the service. This is called by the
  // SupervisedUserService when it is (de)activated.
  void SetActive(bool active);

  // Whether supervised user settings are available.
  bool IsReady() const;

  // Clears all supervised user settings and items.
  void Clear();

  // Constructs a key for a split supervised user setting from a prefix and a
  // variable key.
  static std::string MakeSplitSettingKey(const std::string& prefix,
                                         const std::string& key);

  // Uploads an item to the Sync server. Items are the same data structure as
  // supervised user settings (i.e. key-value pairs, as described at the top of
  // the file), but they are only uploaded (whereas supervised user settings are
  // only downloaded), and never passed to the preference system.
  // An example of an uploaded item is an access request to a blocked URL.
  void UploadItem(const std::string& key, std::unique_ptr<base::Value> value);

  // Sets the setting with the given |key| to a copy of the given |value|.
  void SetLocalSetting(const std::string& key,
                       std::unique_ptr<base::Value> value);

  // Public for testing.
  static syncer::SyncData CreateSyncDataForSetting(const std::string& name,
                                                   const base::Value& value);

  // KeyedService implementation:
  void Shutdown() override;

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

  // PrefStore::Observer implementation:
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool success) override;

  const base::DictionaryValue* LocalSettingsForTest() const;

  // Returns the dictionary where a given Sync item should be stored, depending
  // on whether the supervised user setting is atomic or split. In case of a
  // split setting, the split setting prefix of |key| is removed, so that |key|
  // can be used to update the returned dictionary.
  base::DictionaryValue* GetDictionaryAndSplitKey(std::string* key) const;

 private:
  base::DictionaryValue* GetOrCreateDictionary(const std::string& key) const;
  base::DictionaryValue* GetAtomicSettings() const;
  base::DictionaryValue* GetSplitSettings() const;
  base::DictionaryValue* GetQueuedItems() const;

  // Returns a dictionary with all supervised user settings if the service is
  // active, or NULL otherwise.
  std::unique_ptr<base::DictionaryValue> GetSettings();

  // Sends the settings to all subscribers. This method should be called by the
  // subclass whenever the settings change.
  void InformSubscribers();

  void PushItemToSync(const std::string& key,
                      std::unique_ptr<base::Value> value);

  // Used for persisting the settings. Unlike other PrefStores, this one is not
  // directly hooked up to the PrefService.
  scoped_refptr<PersistentPrefStore> store_;

  bool active_;

  bool initialization_failed_;

  // Set when WaitUntilReadyToSync() is invoked before initialization completes.
  base::OnceClosure wait_until_ready_to_sync_cb_;

  // A set of local settings that are fixed and not configured remotely.
  std::unique_ptr<base::DictionaryValue> local_settings_;

  SettingsCallbackList settings_callback_list_;

  ShutdownCallbackList shutdown_callback_list_;

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSettingsService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SETTINGS_SERVICE_H_
