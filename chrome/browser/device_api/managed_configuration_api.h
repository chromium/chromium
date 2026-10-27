// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "url/origin.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace features {
BASE_DECLARE_FEATURE(kManagedConfigurationRecoverFromFailedFetches);
}  // namespace features

// A keyed service responsible for the Managed Configuration API. It stores,
// synchronizes and parses JSON key-value configurations set by the device
// administration per origin and provides it to the callers.
class ManagedConfigurationAPI
    : public KeyedService,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Observer interface for configuration changes for a specific origin.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnManagedConfigurationChanged() = 0;
    virtual const url::Origin& GetOrigin() const = 0;
  };

  // Observer interface for configuration changes across any origin.
  class AnyOriginObserver : public base::CheckedObserver {
   public:
    virtual void OnAnyManagedConfigurationChanged(
        const url::Origin& origin) = 0;
  };

  // Reasons why syncing an origin's managed configuration needed a retry.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ManagedConfigurationRetryReason)
  enum class RetryReason {
    kDownloadFailed = 0,
    kInvalidJson = 1,
    kStoreWriteFailed = 2,
    kMaxValue = kStoreWriteFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:ManagedConfigurationRetryReason)

  static constexpr char kRetryReasonHistogram[] =
      "WebApp.ManagedConfiguration.RetryReason";
  static constexpr char kRetryCountUntilSuccessHistogram[] =
      "WebApp.ManagedConfiguration.RetryCountUntilSuccess";

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
      base::OnceCallback<void(std::optional<base::DictValue>)> callback);

  // Tries to retrieve the full managed configuration for the |origin|. Returns
  // a dictionary containing all key-value pairs.
  void GetOriginPolicyConfiguration(
      const url::Origin& origin,
      base::OnceCallback<void(std::optional<base::DictValue>)> callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void AddObserver(AnyOriginObserver* observer);
  void RemoveObserver(AnyOriginObserver* observer);

  // Whether this application can have managed configuration set. essentially,
  // this checks whether the application is managed.
  bool CanHaveManagedStore(const url::Origin& origin);

  // Returns the list of all origins that have a managed configuration set.
  const std::set<url::Origin>& GetManagedOrigins() const;

  // Sets the managed configuration for testing purposes without network
  // requests.
  void SetConfigurationForTesting(const url::Origin& origin,
                                  base::DictValue configuration);

  // Test-only overrides. They apply to origins whose state is created
  // afterwards, so call them before policy or SetConfigurationForTesting()
  // names the origin.
  void SetRetryBackoffForTesting(base::TimeDelta initial_delay,
                                 double multiply_factor);
  void SetStoreTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Returns whether a configuration download for `origin` is in flight.
  bool IsFetchInProgressForTesting(const url::Origin& origin) const;

 private:
  // Encapsulates the store, active downloader and retry backoff/timer for a
  // single origin so all per-origin resources are cleaned up together via RAII.
  class OriginConfiguration;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Tries to download the managed configuration stored at the url provided
  // by policy.
  void OnConfigurationPolicyChanged();
  OriginConfiguration& GetOrCreateOriginConfiguration(
      const url::Origin& origin);
  void InformObserversIfConfigurationChanged(const url::Origin& origin,
                                             bool changed);

  base::FilePath GetStoreLocation(const url::Origin& origin);

  // Assigns observers from |unmanaged_observers_| to a particular store if
  // their origin has a configuration. There is no need to unassign those
  // observers when the origin becomes unmanaged, since the managed data is
  // cleared in the ManagedConfigurationStore, without destroying the actual
  // store object.
  void PromoteObservers();

  const raw_ptr<Profile> profile_;

  const base::FilePath stores_path_;
  std::map<url::Origin, std::unique_ptr<OriginConfiguration>> origins_;
  std::map<url::Origin, base::ObserverList<Observer>> observers_;
  base::ObserverList<AnyOriginObserver> any_origin_observers_;

  // Stores the list of origins which have a managed configuration (may not yet
  // be loaded).
  std::set<url::Origin> managed_origins_;

  std::set<raw_ptr<Observer, SetExperimental>> unmanaged_observers_;

  // Observes changes to ManagedConfigurationPerOrigin.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // `content::GetNetworkConnectionTracker()` returns a process-global leaky
  // singleton (`g_network_connection_tracker` in
  // `content/browser/network_service_instance_impl.cc`) that is never destroyed
  // during browser execution and therefore outlives all Profile-keyed services.
  // `base::ScopedObservation` pins the exact tracker pointer observed at
  // construction (in case tests swap the global tracker via
  // `content::SetNetworkConnectionTrackerForTesting()`) and unregisters `this`
  // when `ManagedConfigurationAPI` is destroyed.
  base::ScopedObservation<
      network::NetworkConnectionTracker,
      network::NetworkConnectionTracker::NetworkConnectionObserver>
      network_connection_observation_{this};

  // Test-only overrides; see the corresponding setters.
  std::optional<base::TimeDelta> retry_initial_delay_for_testing_;
  double retry_multiply_factor_for_testing_ = 0;
  scoped_refptr<base::SequencedTaskRunner> store_task_runner_for_testing_;

  base::WeakPtrFactory<ManagedConfigurationAPI> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_API_H_
