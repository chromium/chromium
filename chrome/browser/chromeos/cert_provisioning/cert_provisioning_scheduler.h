// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_invalidator.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_platform_keys_helpers.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
class PrefService;

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class NetworkStateHandler;

namespace cert_provisioning {

class CertProvisioningWorker;

using WorkerMap =
    std::map<CertProfileId, std::unique_ptr<CertProvisioningWorker>>;

using CertProfileSet = base::flat_set<CertProfile, CertProfileComparator>;

// Holds information about a worker which failed that is still useful (e.g. for
// UI) after the worker has been destroyed.
struct FailedWorkerInfo {
  // The state the worker had prior to switching to the failed state
  // (CertProvisioningWorkerState::kFailed).
  CertProvisioningWorkerState state_before_failure =
      CertProvisioningWorkerState::kInitState;
  // The DER-encoded X.509 SPKI.
  std::string public_key;
  // The time the worker was last updated, i.e. when it transferred to the
  // failed state.
  base::Time last_update_time;
};

// An observer that gets notified about state changes of the
// CertProvisioningScheduler.
class CertProvisioningSchedulerObserver : public base::CheckedObserver {
 public:
  // Called when the "visible state" of the observerd CertProvisioningScheduler
  // has changed, i.e. when:
  // (*) the list of active workers changed,
  // (*) the list of recently failed workers changed,
  // (*) the state of a worker changed.
  virtual void OnVisibleStateChanged() = 0;
};

// Interface for the scheduler for client certificate provisioning using device
// management.
class CertProvisioningScheduler {
 public:
  virtual ~CertProvisioningScheduler() = default;

  // Intended to be called when a user presses a button in certificate manager
  // UI. Retries provisioning of a specific certificate.
  virtual void UpdateOneCert(const CertProfileId& cert_profile_id) = 0;
  virtual void UpdateAllCerts() = 0;

  // Returns all certificate provisioning workers that are currently active.
  virtual const WorkerMap& GetWorkers() const = 0;

  // Returns a |FailedWorkerInfo| for certificate provisioning processes that
  // failed and have not been restarted (yet).
  virtual const base::flat_map<CertProfileId, FailedWorkerInfo>&
  GetFailedCertProfileIds() const = 0;

  // Adds |observer| which will observer this CertProvisioningScheduler.
  virtual void AddObserver(CertProvisioningSchedulerObserver* observer) = 0;

  // Removes a previously added |observer|.
  virtual void RemoveObserver(CertProvisioningSchedulerObserver* observer) = 0;
};

// This class is a part of certificate provisioning feature. It tracks updates
// of |RequiredClientCertificateForUser|, |RequiredClientCertificateForDevice|
// policies and creates one CertProvisioningWorker for every policy entry.
// Should work on the UI thread because it interacts with PlatformKeysService
// and some methods are called from the UI to populate certificate manager
// settings page.
class CertProvisioningSchedulerImpl
    : public CertProvisioningScheduler,
      public NetworkStateHandlerObserver,
      public platform_keys::PlatformKeysServiceObserver {
 public:
  static std::unique_ptr<CertProvisioningScheduler>
  CreateUserCertProvisioningScheduler(Profile* profile);
  static std::unique_ptr<CertProvisioningScheduler>
  CreateDeviceCertProvisioningScheduler(
      policy::AffiliatedInvalidationServiceProvider*
          invalidation_service_provider);

  CertProvisioningSchedulerImpl(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      policy::CloudPolicyClient* cloud_policy_client,
      platform_keys::PlatformKeysService* platform_keys_service,
      NetworkStateHandler* network_state_handler,
      std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory);
  ~CertProvisioningSchedulerImpl() override;

  CertProvisioningSchedulerImpl(const CertProvisioningSchedulerImpl&) = delete;
  CertProvisioningSchedulerImpl& operator=(
      const CertProvisioningSchedulerImpl&) = delete;

  // CertProvisioningScheduler:
  void UpdateOneCert(const CertProfileId& cert_profile_id) override;
  void UpdateAllCerts() override;
  const WorkerMap& GetWorkers() const override;
  const base::flat_map<CertProfileId, FailedWorkerInfo>&
  GetFailedCertProfileIds() const override;
  void AddObserver(CertProvisioningSchedulerObserver* observer) override;
  void RemoveObserver(CertProvisioningSchedulerObserver* observer) override;

  // Invoked when the CertProvisioningWorker corresponding to |profile| reached
  // its final state.
  // Public so it can be called from tests.
  void OnProfileFinished(const CertProfile& profile,
                         CertProvisioningWorkerState state);

  // Called when any state visible from the outside has changed.
  // Public so it can be called from tests.
  void OnVisibleStateChanged();

 private:
  void ScheduleInitialUpdate();
  void ScheduleDailyUpdate();
  // Posts delayed task to call UpdateOneCertImpl.
  void ScheduleRetry(const CertProfileId& profile_id);
  void ScheduleRenewal(const CertProfileId& profile_id, base::TimeDelta delay);

  void InitialUpdateCerts();
  void DeleteCertsWithoutPolicy();
  void OnDeleteCertsWithoutPolicyDone(platform_keys::Status status);
  void CancelWorkersWithoutPolicy(const std::vector<CertProfile>& profiles);
  void CleanVaKeysIfIdle();
  void OnCleanVaKeysIfIdleDone(base::Optional<bool> delete_result);
  void RegisterForPrefsChanges();

  void InitiateRenewal(const CertProfileId& cert_profile_id);
  void UpdateOneCertImpl(const CertProfileId& cert_profile_id);
  void UpdateCertList(std::vector<CertProfile> profiles);
  void UpdateCertListWithExistingCerts(
      std::vector<CertProfile> profiles,
      base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
          existing_certs_with_ids,
      platform_keys::Status status);

  void OnPrefsChange();
  void DailyUpdateCerts();
  void DeserializeWorkers();

  // Creates a new worker for |profile| if there is no at the moment.
  // Recreates a worker if existing one has a different version of the profile.
  // Continues an existing worker if it is in a waiting state.
  void ProcessProfile(const CertProfile& profile);

  base::Optional<CertProfile> GetOneCertProfile(
      const CertProfileId& cert_profile_id);
  std::vector<CertProfile> GetCertProfiles();

  void CreateCertProvisioningWorker(CertProfile profile);
  CertProvisioningWorker* FindWorker(CertProfileId profile_id);
  // Adds |worker| to |workers_| and returns an unowned pointer to |worker|.
  // Triggers a state change notification.
  CertProvisioningWorker* AddWorkerToMap(
      std::unique_ptr<CertProvisioningWorker> worker);
  // Removes the element referenced by |worker_iter| from |workers_|.
  // Triggers a state change notification.
  void RemoveWorkerFromMap(WorkerMap::iterator worker_iter);

  // Returns true if the process can be continued (if it's not required to
  // wait).
  bool MaybeWaitForInternetConnection();
  void WaitForInternetConnection();
  void OnNetworkChange(const NetworkState* network);
  // NetworkStateHandlerObserver
  void DefaultNetworkChanged(const NetworkState* network) override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  void UpdateFailedCertProfiles(const CertProvisioningWorker& worker);

  // PlatformKeysServiceObserver
  void OnPlatformKeysServiceShutDown() override;

  // Notifies each observer from |observers_| that the state has changed.
  void NotifyObserversVisibleStateChanged();

  CertScope cert_scope_ = CertScope::kUser;
  // |profile_| can be nullptr for the device-wide instance of
  // CertProvisioningScheduler.
  Profile* profile_ = nullptr;
  PrefService* pref_service_ = nullptr;
  const char* pref_name_ = nullptr;
  policy::CloudPolicyClient* cloud_policy_client_ = nullptr;
  // |platform_keys_service_| can be nullptr if it has been shut down.
  platform_keys::PlatformKeysService* platform_keys_service_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  WorkerMap workers_;
  // Contains cert profile ids that will be renewed before next daily update.
  // Helps to prevent creation of more than one delayed task for renewal. When
  // the renewal starts for a profile id, it is removed from the set.
  base::flat_set<CertProfileId> scheduled_renewals_;
  // Collection of cert profile ids that failed recently. They will not be
  // retried until next |DailyUpdateCerts|. FailedWorkerInfo contains some extra
  // information about the failure. Profiles that failed with
  // kInconsistentDataError will not be stored into this collection.
  base::flat_map<CertProfileId, FailedWorkerInfo> failed_cert_profiles_;
  // Equals true if the last attempt to update certificates failed because there
  // was no internet connection.
  bool is_waiting_for_online_ = false;

  // Contains profiles that should be updated after the current update batch
  // run, because an update for them was triggered during the current run.
  CertProfileSet queued_profiles_to_update_;

  LatestCertsWithIdsGetter certs_with_ids_getter_;
  CertDeleter cert_deleter_;
  std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory_;

  // Observers that are observing this CertProvisioningSchedulerImpl.
  base::ObserverList<CertProvisioningSchedulerObserver> observers_;
  // True when a task for notifying observers about a state change has been
  // scheduled but not executed yet.
  bool notify_observers_pending_ = false;

  ScopedObserver<platform_keys::PlatformKeysService,
                 platform_keys::PlatformKeysServiceObserver>
      scoped_platform_keys_service_observer_{this};

  base::WeakPtrFactory<CertProvisioningSchedulerImpl> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
