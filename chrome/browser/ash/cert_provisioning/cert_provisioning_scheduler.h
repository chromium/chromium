// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_

#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_platform_keys_helpers.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;
class PrefService;

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace ash {

class NetworkStateHandler;

namespace cert_provisioning {

class CertProvisioningClient;
class CertProvisioningWorker;

using WorkerMap =
    std::map<CertProfileId, std::unique_ptr<CertProvisioningWorker>>;

using CertProfileSet = base::flat_set<CertProfile, CertProfileComparator>;

// Holds information about a worker which failed that is still useful (e.g. for
// UI) after the worker has been destroyed.
struct FailedWorkerInfo {
  FailedWorkerInfo();
  ~FailedWorkerInfo();
  FailedWorkerInfo(const FailedWorkerInfo&);
  FailedWorkerInfo& operator=(const FailedWorkerInfo&);
  // The state the worker had prior to switching to the failed state
  // (CertProvisioningWorkerState::kFailed).
  CertProvisioningWorkerState state_before_failure =
      CertProvisioningWorkerState::kInitState;
  // The DER-encoded X.509 SPKI.
  std::vector<uint8_t> public_key;
  // Human-readable certificate profile name (UTF-8).
  std::string cert_profile_name;
  // The time the worker was last updated, i.e. when it transferred to the
  // failed state.
  base::Time last_update_time;
  // Holds a message describing the reason for the failure.
  std::string failure_message;
};

// Interface for the scheduler for client certificate provisioning using device
// management.
class CertProvisioningScheduler {
 public:
  virtual ~CertProvisioningScheduler() = default;

  // Intended to be called when a user presses a button in certificate manager
  // UI. Retries the process of provisioning a specific certificate.
  // Returns "false" if `cert_profile_id` is not found and "true" otherwise.
  virtual bool UpdateOneWorker(const CertProfileId& cert_profile_id) = 0;
  virtual void UpdateAllWorkers() = 0;
  // Resets the process of provisioning a specific certificate.
  // Returns "false" if `cert_profile_id` is not found and "true" otherwise.
  virtual bool ResetOneWorker(const CertProfileId& cert_profile_id) = 0;

  // Returns all certificate provisioning workers that are currently active.
  virtual const WorkerMap& GetWorkers() const = 0;

  // Returns a |FailedWorkerInfo| for certificate provisioning processes that
  // failed and have not been restarted (yet).
  virtual const base::flat_map<CertProfileId, FailedWorkerInfo>&
  GetFailedCertProfileIds() const = 0;

  // Saves the |callback| to call it when the "visible state" of the scheduler
  // changes, i.e.
  // (*) the list of active workers changes,
  // (*) the list of recently failed workers changes,
  // (*) the state of a worker changes.
  // (As long as the returned subscription is alive.)
  virtual base::CallbackListSubscription AddObserver(
      base::RepeatingClosure callback) = 0;
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
      policy::CloudPolicyClient* cloud_policy_client,
      std::variant<policy::AffiliatedInvalidationServiceProvider*,
                   invalidation::InvalidationListener*>
          invalidation_service_provider_or_listener);

  CertProvisioningSchedulerImpl(
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      std::unique_ptr<CertProvisioningClient> cert_provisioning_client,
      platform_keys::PlatformKeysService* platform_keys_service,
      NetworkStateHandler* network_state_handler,
      std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory);
  ~CertProvisioningSchedulerImpl() override;

  CertProvisioningSchedulerImpl(const CertProvisioningSchedulerImpl&) = delete;
  CertProvisioningSchedulerImpl& operator=(
      const CertProvisioningSchedulerImpl&) = delete;

  // CertProvisioningScheduler:
  bool UpdateOneWorker(const CertProfileId& cert_profile_id) override;
  void UpdateAllWorkers() override;
  bool ResetOneWorker(const CertProfileId& cert_profile_id) override;
  const WorkerMap& GetWorkers() const override;
  const base::flat_map<CertProfileId, FailedWorkerInfo>&
  GetFailedCertProfileIds() const override;
  base::CallbackListSubscription AddObserver(
      base::RepeatingClosure callback) override;

  // Invoked when the CertProvisioningWorker corresponding to |profile| reached
  // its final state.
  // Public so it can be called from tests.
  void OnProfileFinished(CertProfile profile,
                         std::string process_id,
                         CertProvisioningWorkerState state);

  // Called when any state visible from the outside has changed.
  // Public so it can be called from tests.
  void OnVisibleStateChanged();

 private:
  void ScheduleInitialUpdate();
  void ScheduleDailyUpdate();
  // Posts delayed task to call UpdateOneWorkerImpl.
  void ScheduleRetry(const CertProfileId& profile_id);
  void ScheduleRenewal(const CertProfileId& profile_id, base::TimeDelta delay);

  void InitialUpdateCerts();
  void DeleteCertsWithoutPolicy();
  void OnDeleteCertsWithoutPolicyDone(chromeos::platform_keys::Status status);
  void CancelWorkersWithoutPolicy(const std::vector<CertProfile>& profiles);
  void CleanVaKeysIfIdle();
  void OnCleanVaKeysIfIdleDone(bool delete_result);
  void RegisterForPrefsChanges();

  void InitiateRenewal(const CertProfileId& cert_profile_id);
  void UpdateOneWorkerImpl(const CertProfileId& cert_profile_id);
  void UpdateWorkerList(std::vector<CertProfile> profiles);
  void UpdateWorkerListWithExistingCerts(
      std::vector<CertProfile> profiles,
      base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
          existing_certs_with_ids,
      chromeos::platform_keys::Status status);

  void OnPrefsChange();
  void DailyUpdateWorkers();
  void DeserializeWorkers();

  // Creates a new worker for |profile| if there is no at the moment.
  // Recreates a worker if existing one has a different version of the profile.
  // Continues an existing worker if it is in a waiting state.
  void ProcessProfile(const CertProfile& profile);

  std::optional<CertProfile> GetOneCertProfile(
      const CertProfileId& cert_profile_id);
  std::vector<CertProfile> GetCertProfiles();

  void CreateCertProvisioningWorker(CertProfile profile);
  CertProvisioningWorker* FindWorker(CertProfileId profile_id);
  // Adds |worker| to |workers_| and returns an unowned pointer to |worker|.
  // Triggers a state change notification.
  CertProvisioningWorker* AddWorkerToMap(
      std::unique_ptr<CertProvisioningWorker> worker);
  // Removes the element referenced by |worker_iter| from |workers_|.
  // Triggers a state change notification if send_visible_state_changed_update
  // is true.
  void RemoveWorkerFromMap(WorkerMap::iterator worker_iter,
                           bool send_visible_state_changed_update);

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

  // Called by |hold_back_updates_timer_| when the notifications should be sent
  // again. Notifies observers if there were any events during the hold back
  // period.
  void OnHoldBackUpdatesTimerExpired();
  // Notifies each observer from |observers_| that the state has changed.
  void NotifyObserversVisibleStateChanged();

  CertScope cert_scope_ = CertScope::kUser;
  // |profile_| can be nullptr for the device-wide instance of
  // CertProvisioningScheduler.
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  const char* pref_name_ = nullptr;
  std::unique_ptr<CertProvisioningClient> cert_provisioning_client_;
  // |platform_keys_service_| can be nullptr if it has been shut down.
  raw_ptr<platform_keys::PlatformKeysService> platform_keys_service_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  PrefChangeRegistrar pref_change_registrar_;
  WorkerMap workers_;
  // Contains cert profile ids that will be renewed before next daily update.
  // Helps to prevent creation of more than one delayed task for renewal. When
  // the renewal starts for a profile id, it is removed from the set.
  base::flat_set<CertProfileId> scheduled_renewals_;
  // Collection of cert profile ids that failed recently. They will not be
  // retried until next |DailyUpdateWorkers|. FailedWorkerInfo contains some
  // extra information about the failure. Profiles that failed with
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
  base::RepeatingClosureList observers_;
  // True when a task for notifying observers about a state change has been
  // scheduled but not executed yet.
  bool notify_observers_pending_ = false;
  // When this timer is running, notifications should not be sent until it
  // fires. Used to prevent spamming the observers if many events happen in
  // rapid succession.
  base::OneShotTimer hold_back_updates_timer_;
  // When this is true, an update should be sent to the UI when
  // |hold_back_updates_timer_| fires.
  bool update_after_hold_back_ = false;

  base::ScopedObservation<platform_keys::PlatformKeysService,
                          platform_keys::PlatformKeysServiceObserver>
      scoped_platform_keys_service_observation_{this};

  base::WeakPtrFactory<CertProvisioningSchedulerImpl> weak_factory_{this};
};

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_SCHEDULER_H_
