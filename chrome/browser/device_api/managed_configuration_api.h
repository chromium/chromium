// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
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
    virtual const url::Origin& GetOrigin() const = 0;
  };

  static const char kOriginKey[];
  static const char kManagedConfigurationUrlKey[];
  static const char kManagedConfigurationHashKey[];

  explicit ManagedConfigurationAPI(Profile* profile);
  ~ManagedConfigurationAPI() override;
  ManagedConfigurationAPI(const ManagedConfigurationAPI&) = delete;
  ManagedConfigurationAPI& operator=(const ManagedConfigurationAPI&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Tries to retrieve the managed configuration for the |origin|, mapped by
  // |keys|. Returns a dictionary, mapping each found key to a value.
  void GetOriginPolicyConfiguration(
      const url::Origin& origin,
      const std::vector<std::string>& keys,
      base::OnceCallback<void(std::optional<base::Value::Dict>)> callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Whether this application can have managed configuration set. essentially,
  // this checks whether the application is managed.
  bool CanHaveManagedStore(const url::Origin& origin);

  // Returns the list of all origins that have a managed configuration set.
  const std::set<url::Origin>& GetManagedOrigins() const;

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
      data_decoder::DataDecoder::ValueOrError result);

  // Sends an operation to set the configured value on FILE thread.
  void PostStoreConfiguration(const url::Origin& origin,
                              base::Value::Dict configuration);
  void InformObserversIfConfigurationChanged(const url::Origin& origin,
                                             bool changed);

  void MaybeCreateStoreForOrigin(const url::Origin& origin);
  base::FilePath GetStoreLocation(const url::Origin& origin);

  // Assigns observers from |unmanaged_observers_| to a particular store if
  // their origin has a configuration. There is no need to unassign those
  // observers when the origin becomes unmanaged, since the managed data is
  // cleared in the ManagedConfigurationStore, without destroying the actual
  // store object.
  void PromoteObservers();

  const raw_ptr<Profile> profile_;

  const base::FilePath stores_path_;
  std::map<url::Origin, base::SequenceBound<ManagedConfigurationStore>>
      store_map_;
  // Stores current configuration downloading managers.
  std::map<url::Origin, std::unique_ptr<ManagedConfigurationDownloader>>
      downloaders_;
  std::map<url::Origin, base::ObserverList<Observer>> observers_;

  // Stores the list of orrigins which have a managed configuration(may not yet
  // loaded).
  std::set<url::Origin> managed_origins_;

  std::set<raw_ptr<Observer, SetExperimental>> unmanaged_observers_;

  // Observes changes to WebAppInstallForceList.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::WeakPtrFactory<ManagedConfigurationAPI> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_
