// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/origin.h"

class Profile;
class ManagedConfigurationStore;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A keyed service responsible for the Managed Configuration API. It stores,
// synchronizes and parses JSON key-value configurations set by the device
// administration per origin and provides it to the callers.
class ManagedConfigurationAPI : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnManagedConfigurationChanged() = 0;
  };

  static const char kOriginKey[];
  static const char kManagedConfigurationUrlKey[];
  static const char kManagedConfigurationHashKey[];

  explicit ManagedConfigurationAPI(Profile* profile);
  ~ManagedConfigurationAPI() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Tries to retrieve the managed configuration for the |origin|, mapped by
  // |keys|. Returns a dictionary, mapping each found key to a value.
  void GetOriginPolicyConfiguration(
      const url::Origin& origin,
      const std::vector<std::string>& keys,
      base::OnceCallback<void(std::unique_ptr<base::DictionaryValue>)>
          callback);

  void AddObserver(const url::Origin& origin, Observer* observer);
  void RemoveObserver(const url::Origin& origin, Observer* observer);

 private:
  class ManagedConfigurationDownloader;

  // Tries to download the managed configuration stored at the url provided
  // by policy.
  void OnConfigurationPolicyChanged();
  void UpdateStoredDataForOrigin(const url::Origin& origin,
                                 const std::string& configuration_url,
                                 const std::string& configuration_hash);

  // Downloads the data from the data_url in a worker thread, and returns it in
  // a callback.
  void DecodeData(const url::Origin& origin,
                  const std::string& url_hash,
                  std::unique_ptr<std::string> data);
  void ProcessDecodedConfiguration(
      const url::Origin& origin,
      const std::string& url_hash,
      const data_decoder::DataDecoder::ValueOrError result);

  // Sends an operation to set the configured value on FILE thread.
  void PostStoreConfiguration(const url::Origin& origin,
                              base::DictionaryValue configuration);

  std::unique_ptr<base::DictionaryValue> GetConfigurationOnBackend(
      const url::Origin& origin,
      const std::vector<std::string>& keys);
  void StoreConfigurationOnBackend(const url::Origin& origin,
                                   base::DictionaryValue configuration);

  ManagedConfigurationStore* GetOrLoadStoreForOrigin(const url::Origin& origin);
  base::FilePath GetStoreLocation(const url::Origin& origin);

  Profile* const profile_;

  const base::FilePath stores_path_;
  std::map<url::Origin, std::unique_ptr<ManagedConfigurationStore>> store_map_;
  // Stores current configuration downloading managers.
  std::map<url::Origin, std::unique_ptr<ManagedConfigurationDownloader>>
      downloaders_;

  // Blocking task runner for IO related tasks.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Observes changes to WebAppInstallForceList.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::WeakPtrFactory<ManagedConfigurationAPI> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_
