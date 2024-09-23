// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <unordered_set>
#include <variant>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_metrics.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace ash::cert_provisioning {

namespace {

template <typename Container, typename Value>
void EraseByKey(Container& container, const Value& value) {
  auto iter = container.find(value);
  if (iter == container.end()) {
    return;
  }

  container.erase(iter);
}

const base::TimeDelta kInconsistentDataErrorRetryDelay = base::Seconds(30);

policy::CloudPolicyClient* GetCloudPolicyClientForUser(Profile* profile) {
  policy::UserCloudPolicyManagerAsh* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!user_cloud_policy_manager) {
    return nullptr;
  }

  policy::CloudPolicyCore* core = user_cloud_policy_manager->core();
  if (!core) {
    return nullptr;
  }

  return core->client();
}

NetworkStateHandler* GetNetworkStateHandler() {
  // Can happen in tests.
  if (!NetworkHandler::IsInitialized()) {
    return nullptr;
  }
  return NetworkHandler::Get()->network_state_handler();
}

}  // namespace

FailedWorkerInfo::FailedWorkerInfo() = default;
FailedWorkerInfo::~FailedWorkerInfo() = default;
FailedWorkerInfo::FailedWorkerInfo(const FailedWorkerInfo&) = default;
FailedWorkerInfo& FailedWorkerInfo::operator=(const FailedWorkerInfo&) =
    default;

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningSchedulerImpl::CreateUserCertProvisioningScheduler(
    Profile* profile) {
  PrefService* pref_service = profile->GetPrefs();
  policy::CloudPolicyClient* cloud_policy_client =
      GetCloudPolicyClientForUser(profile);
  platform_keys::PlatformKeysService* platform_keys_service =
      GetPlatformKeysService(CertScope::kUser, profile);
  NetworkStateHandler* network_state_handler = GetNetworkStateHandler();

  if (!profile || !pref_service || !cloud_policy_client ||
      !network_state_handler) {
    LOG(ERROR) << "Failed to create user certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningSchedulerImpl>(
      CertScope::kUser, profile, pref_service,
      std::make_unique<CertProvisioningClientImpl>(*cloud_policy_client),
      platform_keys_service, network_state_handler,
      std::make_unique<CertProvisioningUserInvalidatorFactory>(profile));
}

// static
std::unique_ptr<CertProvisioningScheduler>
CertProvisioningSchedulerImpl::CreateDeviceCertProvisioningScheduler(
    policy::CloudPolicyClient* cloud_policy_client,
    std::variant<policy::AffiliatedInvalidationServiceProvider*,
                 invalidation::InvalidationListener*>
        invalidation_service_provider_or_listener) {
  PrefService* pref_service = g_browser_process->local_state();
  platform_keys::PlatformKeysService* platform_keys_service =
      GetPlatformKeysService(CertScope::kDevice, /*profile=*/nullptr);
  NetworkStateHandler* network_state_handler = GetNetworkStateHandler();

  if (!pref_service || !cloud_policy_client || !network_state_handler ||
      !platform_keys_service) {
    LOG(ERROR) << "Failed to create device certificate provisioning scheduler";
    return nullptr;
  }

  return std::make_unique<CertProvisioningSchedulerImpl>(
      CertScope::kDevice, /*profile=*/nullptr, pref_service,
      std::make_unique<CertProvisioningClientImpl>(*cloud_policy_client),
      platform_keys_service, network_state_handler,
      std::make_unique<CertProvisioningDeviceInvalidatorFactory>(
          invalidation_service_provider_or_listener));
}

CertProvisioningSchedulerImpl::CertProvisioningSchedulerImpl(
    CertScope cert_scope,
    Profile* profile,
    PrefService* pref_service,
    std::unique_ptr<CertProvisioningClient> cert_provisioning_client,
    platform_keys::PlatformKeysService* platform_keys_service,
    NetworkStateHandler* network_state_handler,
    std::unique_ptr<CertProvisioningInvalidatorFactory> invalidator_factory)
    : cert_scope_(cert_scope),
      profile_(profile),
      pref_service_(pref_service),
      cert_provisioning_client_(std::move(cert_provisioning_client)),
      platform_keys_service_(platform_keys_service),
      network_state_handler_(network_state_handler),
      certs_with_ids_getter_(cert_scope, platform_keys_service),
      cert_deleter_(cert_scope, platform_keys_service),
      invalidator_factory_(std::move(invalidator_factory)) {
  CHECK(profile_ || cert_scope_ == CertScope::kDevice);
  CHECK(pref_service_);
  CHECK(cert_provisioning_client_);
  CHECK(platform_keys_service_);
  CHECK(network_state_handler);
  CHECK(invalidator_factory_);

  pref_name_ = GetPrefNameForCertProfiles(cert_scope);
  CHECK(pref_name_);

  scoped_platform_keys_service_observation_.Observe(
      platform_keys_service_.get());

  network_state_handler_observer_.Observe(network_state_handler_.get());

  ScheduleInitialUpdate();
  ScheduleDailyUpdate();
}

CertProvisioningSchedulerImpl::~CertProvisioningSchedulerImpl() = default;

void CertProvisioningSchedulerImpl::ScheduleInitialUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::InitialUpdateCerts,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::ScheduleDailyUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::DailyUpdateWorkers,
                     weak_factory_.GetWeakPtr()),
      base::Days(1));
}

void CertProvisioningSchedulerImpl::ScheduleRetry(
    const CertProfileId& profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO: b/299054905 - Instead of using a hardcoded delay time, trigger a
  // policy refresh and restart workers when policies have been applied.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::UpdateOneWorkerImpl,
                     weak_factory_.GetWeakPtr(), profile_id),
      kInconsistentDataErrorRetryDelay);
}

void CertProvisioningSchedulerImpl::ScheduleRenewal(
    const CertProfileId& profile_id,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::Contains(scheduled_renewals_, profile_id)) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CertProvisioningSchedulerImpl::InitiateRenewal,
                     weak_factory_.GetWeakPtr(), profile_id),
      delay);
}

void CertProvisioningSchedulerImpl::InitialUpdateCerts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DeleteCertsWithoutPolicy();
}

void CertProvisioningSchedulerImpl::DeleteCertsWithoutPolicy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // No-op if the PlatformKeysService has already been shut down.
  if (!platform_keys_service_) {
    return;
  }

  cert_deleter_.DeleteCerts(
      base::MakeFlatSet<CertProfileId>(GetCertProfiles(), {},
                                       &CertProfile::profile_id),
      base::BindOnce(
          &CertProvisioningSchedulerImpl::OnDeleteCertsWithoutPolicyDone,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::OnDeleteCertsWithoutPolicyDone(
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Failed to delete certificates without policies: "
               << chromeos::platform_keys::StatusToString(status);
  }

  DeserializeWorkers();
  CleanVaKeysIfIdle();
}

void CertProvisioningSchedulerImpl::CleanVaKeysIfIdle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!workers_.empty()) {
    OnCleanVaKeysIfIdleDone(true);
    return;
  }

  DeleteVaKeysByPrefix(
      cert_scope_, profile_, kKeyNamePrefix,
      base::BindOnce(&CertProvisioningSchedulerImpl::OnCleanVaKeysIfIdleDone,
                     weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::OnCleanVaKeysIfIdleDone(
    bool delete_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delete_result) {
    LOG(ERROR) << "Failed to delete keys while idle";
  }

  RegisterForPrefsChanges();
  UpdateAllWorkers();
}

void CertProvisioningSchedulerImpl::RegisterForPrefsChanges() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_name_,
      base::BindRepeating(&CertProvisioningSchedulerImpl::OnPrefsChange,
                          weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::DailyUpdateWorkers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  failed_cert_profiles_.clear();
  UpdateAllWorkers();
  ScheduleDailyUpdate();
}

void CertProvisioningSchedulerImpl::DeserializeWorkers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::Dict& saved_workers =
      pref_service_->GetDict(GetPrefNameForSerialization(cert_scope_));

  for (const auto kv : saved_workers) {
    const base::Value::Dict& saved_worker = kv.second.GetDict();

    std::unique_ptr<CertProvisioningWorker> worker =
        CertProvisioningWorkerFactory::Get()->Deserialize(
            cert_scope_, profile_, pref_service_, saved_worker,
            cert_provisioning_client_.get(), invalidator_factory_->Create(),
            base::BindRepeating(
                &CertProvisioningSchedulerImpl::OnVisibleStateChanged,
                weak_factory_.GetWeakPtr()),
            base::BindOnce(&CertProvisioningSchedulerImpl::OnProfileFinished,
                           weak_factory_.GetWeakPtr()));
    if (!worker) {
      // Deserialization error message was already logged.
      continue;
    }

    AddWorkerToMap(std::move(worker));
  }
}

void CertProvisioningSchedulerImpl::OnPrefsChange() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateAllWorkers();
}

void CertProvisioningSchedulerImpl::InitiateRenewal(
    const CertProfileId& cert_profile_id) {
  scheduled_renewals_.erase(cert_profile_id);
  UpdateOneWorkerImpl(cert_profile_id);
}

bool CertProvisioningSchedulerImpl::UpdateOneWorker(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto worker_iter = workers_.find(cert_profile_id);
  if (worker_iter == workers_.end()) {
    return false;
  }

  RecordEvent(worker_iter->second->GetCertProfile().protocol_version,
              cert_scope_, CertProvisioningEvent::kWorkerRetryManual);
  UpdateOneWorkerImpl(cert_profile_id);
  return true;
}

void CertProvisioningSchedulerImpl::UpdateOneWorkerImpl(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  EraseByKey(failed_cert_profiles_, cert_profile_id);

  std::optional<CertProfile> cert_profile = GetOneCertProfile(cert_profile_id);
  if (!cert_profile) {
    return;
  }

  UpdateWorkerList({std::move(cert_profile).value()});
}

void CertProvisioningSchedulerImpl::UpdateAllWorkers() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<CertProfile> profiles = GetCertProfiles();
  CancelWorkersWithoutPolicy(profiles);

  if (profiles.empty()) {
    return;
  }

  UpdateWorkerList(std::move(profiles));
}

void CertProvisioningSchedulerImpl::UpdateWorkerList(
    std::vector<CertProfile> profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // No-op if the PlatformKeysService has already been shut down.
  if (!platform_keys_service_) {
    return;
  }

  if (!MaybeWaitForInternetConnection()) {
    return;
  }

  if (certs_with_ids_getter_.IsRunning()) {
    queued_profiles_to_update_.insert(std::make_move_iterator(profiles.begin()),
                                      std::make_move_iterator(profiles.end()));
    return;
  }

  certs_with_ids_getter_.GetCertsWithIds(base::BindOnce(
      &CertProvisioningSchedulerImpl::UpdateWorkerListWithExistingCerts,
      weak_factory_.GetWeakPtr(), std::move(profiles)));
}

void CertProvisioningSchedulerImpl::UpdateWorkerListWithExistingCerts(
    std::vector<CertProfile> profiles,
    base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>
        existing_certs_with_ids,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Failed to get existing cert ids: "
               << chromeos::platform_keys::StatusToString(status);
    return;
  }

  for (const auto& profile : profiles) {
    if (base::Contains(failed_cert_profiles_, profile.profile_id)) {
      continue;
    }

    auto cert_iter = existing_certs_with_ids.find(profile.profile_id);
    if (cert_iter == existing_certs_with_ids.end()) {
      // The certificate does not exists and should be provisioned.
      ProcessProfile(profile);
      continue;
    }

    const auto& cert = cert_iter->second;
    base::Time now = base::Time::Now();
    if ((now + profile.renewal_period) >= cert->valid_expiry()) {
      // The certificate should be renewed immediately.
      ProcessProfile(profile);
      continue;
    }

    if ((now + base::Days(1) + profile.renewal_period) >=
        cert->valid_expiry()) {
      // The certificate should be renewed within 1 day.
      base::Time target_time = cert->valid_expiry() - profile.renewal_period;
      ScheduleRenewal(profile.profile_id, /*delay=*/target_time - now);
      continue;
    }
  }

  if (!queued_profiles_to_update_.empty()) {
    // base::flat_set::extract() guaranties that the set is `empty()`
    // afterwards.
    UpdateWorkerList(std::move(queued_profiles_to_update_).extract());
  }
}

void CertProvisioningSchedulerImpl::ProcessProfile(
    const CertProfile& cert_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CertProvisioningWorker* worker = FindWorker(cert_profile.profile_id);
  if (!worker) {
    CreateCertProvisioningWorker(cert_profile);
    return;
  }

  if ((worker->GetCertProfile().policy_version !=
       cert_profile.policy_version)) {
    // The worker has outdated policy version. Make it stop, clean up current
    // state and report back through its callback. That will trigger retry for
    // its certificate profile.
    worker->Stop(CertProvisioningWorkerState::kInconsistentDataError);
    return;
  }

  if (worker->IsWaiting()) {
    worker->DoStep();
    return;
  }

  // There already is an active worker for this profile. No action required.
  return;
}

void CertProvisioningSchedulerImpl::CreateCertProvisioningWorker(
    CertProfile cert_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<CertProvisioningWorker> worker =
      CertProvisioningWorkerFactory::Get()->Create(
          GenerateCertProvisioningId(), cert_scope_, profile_, pref_service_,
          cert_profile, cert_provisioning_client_.get(),
          invalidator_factory_->Create(),
          base::BindRepeating(
              &CertProvisioningSchedulerImpl::OnVisibleStateChanged,
              weak_factory_.GetWeakPtr()),
          base::BindOnce(&CertProvisioningSchedulerImpl::OnProfileFinished,
                         weak_factory_.GetWeakPtr()));
  CertProvisioningWorker* worker_unowned = AddWorkerToMap(std::move(worker));
  worker_unowned->DoStep();
}

void CertProvisioningSchedulerImpl::OnProfileFinished(
    CertProfile profile,
    std::string process_id,
    CertProvisioningWorkerState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto worker_iter = workers_.find(profile.profile_id);
  if (worker_iter == workers_.end()) {
    NOTREACHED_IN_MIGRATION();
    LOG(WARNING) << "Finished worker is not found"
                 << base::StringPrintf(" [cppId: %s]", process_id.c_str());
    return;
  }
  bool recreate = false;
  switch (state) {
    case CertProvisioningWorkerState::kSucceeded:
      VLOG(0) << "Successfully provisioned certificate"
              << base::StringPrintf(" [cppId: %s, profileId: %s]",
                                    process_id.c_str(),
                                    profile.profile_id.c_str());
      break;
    case CertProvisioningWorkerState::kInconsistentDataError:
      LOG(WARNING) << "Inconsistent data error"
                   << base::StringPrintf(" [cppId: %s, profileId: %s]",
                                         process_id.c_str(),
                                         profile.profile_id.c_str());
      ScheduleRetry(profile.profile_id);
      break;
    case CertProvisioningWorkerState::kCanceled:
      if (worker_iter->second->IsWorkerMarkedForReset()) {
        recreate = true;
      }
      break;
    default:
      LOG(ERROR) << "Failed to process certificate"
                 << base::StringPrintf(" [cppId: %s, profileId: %s]",
                                       process_id.c_str(),
                                       profile.profile_id.c_str());
      UpdateFailedCertProfiles(*(worker_iter->second));
      break;
  }

  if (recreate) {
    // Avoid updating the ui after removal of the worker as it it will close the
    // dialogue while reseting. The ui will be updated when the new worker is
    // created.
    RemoveWorkerFromMap(worker_iter,
                        /*send_visible_state_changed_update=*/false);
    CreateCertProvisioningWorker(std::move(profile));
  } else {
    RemoveWorkerFromMap(worker_iter,
                        /*send_visible_state_changed_update=*/true);
  }
}

bool CertProvisioningSchedulerImpl::ResetOneWorker(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<CertProfile> cert_profile = GetOneCertProfile(cert_profile_id);
  if (!cert_profile) {
    return false;
  }
  CertProvisioningWorker* worker = FindWorker(cert_profile_id);
  if (!worker) {
    UpdateOneWorkerImpl(cert_profile_id);
    return true;
  }
  if (worker->IsWorkerMarkedForReset()) {
    return true;
  }
  worker->MarkWorkerForReset();
  worker->Stop(CertProvisioningWorkerState::kCanceled);
  return true;
}

CertProvisioningWorker* CertProvisioningSchedulerImpl::FindWorker(
    CertProfileId profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = workers_.find(profile_id);
  if (iter == workers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

CertProvisioningWorker* CertProvisioningSchedulerImpl::AddWorkerToMap(
    std::unique_ptr<CertProvisioningWorker> worker) {
  CertProvisioningWorker* worker_unowned = worker.get();
  workers_[worker_unowned->GetCertProfile().profile_id] = std::move(worker);
  OnVisibleStateChanged();
  return worker_unowned;
}

void CertProvisioningSchedulerImpl::RemoveWorkerFromMap(
    WorkerMap::iterator worker_iter,
    bool send_visible_state_changed_update) {
  workers_.erase(worker_iter);
  // In a case like removing an existing worker for the intent of recreation,
  // the ui should not be sent an update here as this will cause the worker
  // dialogue to be closed. Instead, the ui update will be triggered by the new
  // worker.
  if (send_visible_state_changed_update) {
    OnVisibleStateChanged();
  }
}

std::optional<CertProfile> CertProvisioningSchedulerImpl::GetOneCertProfile(
    const CertProfileId& cert_profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value& profile_list = pref_service_->GetValue(pref_name_);

  for (const base::Value& cur_profile : profile_list.GetList()) {
    const base::Value::Dict& cur_profile_dict = cur_profile.GetDict();
    const CertProfileId* id = cur_profile_dict.FindString(kCertProfileIdKey);
    if (!id || (*id != cert_profile_id)) {
      continue;
    }

    return CertProfile::MakeFromValue(cur_profile_dict);
  }

  return std::nullopt;
}

std::vector<CertProfile> CertProvisioningSchedulerImpl::GetCertProfiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value& profile_list = pref_service_->GetValue(pref_name_);

  std::vector<CertProfile> result_profiles;
  for (const base::Value& cur_profile : profile_list.GetList()) {
    std::optional<CertProfile> p =
        CertProfile::MakeFromValue(cur_profile.GetDict());
    if (!p) {
      LOG(WARNING) << "Failed to parse certificate profile";
      continue;
    }

    result_profiles.emplace_back(std::move(p.value()));
  }

  return result_profiles;
}

const WorkerMap& CertProvisioningSchedulerImpl::GetWorkers() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return workers_;
}

const base::flat_map<CertProfileId, FailedWorkerInfo>&
CertProvisioningSchedulerImpl::GetFailedCertProfileIds() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return failed_cert_profiles_;
}

base::CallbackListSubscription CertProvisioningSchedulerImpl::AddObserver(
    base::RepeatingClosure callback) {
  return observers_.Add(std::move(callback));
}

bool CertProvisioningSchedulerImpl::MaybeWaitForInternetConnection() {
  const NetworkState* network = network_state_handler_->DefaultNetwork();
  bool is_online = network && network->IsOnline();

  if (is_online) {
    is_waiting_for_online_ = false;
    return true;
  }

  WaitForInternetConnection();
  return false;
}

void CertProvisioningSchedulerImpl::WaitForInternetConnection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_waiting_for_online_) {
    return;
  }

  if (!workers_.empty()) {
    VLOG(0) << "Waiting for internet connection";
  }

  is_waiting_for_online_ = true;
  for (auto& kv : workers_) {
    auto& worker_ptr = kv.second;
    worker_ptr->Pause();
  }
}

void CertProvisioningSchedulerImpl::OnNetworkChange(
    const NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If waiting for connection and some network becomes online, try to continue.
  if (is_waiting_for_online_ && network && network->IsOnline()) {
    is_waiting_for_online_ = false;
    UpdateAllWorkers();
    return;
  }

  // If not waiting, check that after this network change some connection still
  // exists.
  if (!is_waiting_for_online_ && !workers_.empty()) {
    MaybeWaitForInternetConnection();
    return;
  }
}

void CertProvisioningSchedulerImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnNetworkChange(network);
}

void CertProvisioningSchedulerImpl::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnNetworkChange(network);
}

void CertProvisioningSchedulerImpl::UpdateFailedCertProfiles(
    const CertProvisioningWorker& worker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FailedWorkerInfo info;
  info.state_before_failure = worker.GetPreviousState();
  info.cert_profile_name = worker.GetCertProfile().name;
  info.public_key = worker.GetPublicKey();
  info.last_update_time = worker.GetLastUpdateTime();
  info.failure_message = worker.GetFailureMessage();

  failed_cert_profiles_[worker.GetCertProfile().profile_id] = std::move(info);
}

void CertProvisioningSchedulerImpl::OnPlatformKeysServiceShutDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The |platform_keys_service_| will only return errors going forward, so
  // stop using it. Shutdown all workers, as if this CertProvisioningScheduler
  // was destroyed, and stop pending tasks that may depend on
  // |platform_keys_service_|.
  workers_.clear();
  certs_with_ids_getter_.Cancel();
  cert_deleter_.Cancel();
  pref_change_registrar_.RemoveAll();
  weak_factory_.InvalidateWeakPtrs();

  scoped_platform_keys_service_observation_.Reset();
  platform_keys_service_ = nullptr;
}

void CertProvisioningSchedulerImpl::CancelWorkersWithoutPolicy(
    const std::vector<CertProfile>& profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (workers_.empty()) {
    return;
  }

  std::unordered_set<CertProfileId> cert_profile_ids;
  for (const CertProfile& profile : profiles) {
    cert_profile_ids.insert(profile.profile_id);
  }

  for (auto& kv : workers_) {
    auto& worker_ptr = kv.second;
    if (cert_profile_ids.find(worker_ptr->GetCertProfile().profile_id) ==
        cert_profile_ids.end()) {
      // This will trigger clean up (if any) in the worker and make it call its
      // callback.
      worker_ptr->Stop(CertProvisioningWorkerState::kCanceled);
    }
  }
}

void CertProvisioningSchedulerImpl::OnVisibleStateChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |notify_observers_pending_| prevents the scheduler from sending multiple
  // notifications from a single synchronous code execution sequence. Extra
  // notifications are discarded.
  if (observers_.empty() || notify_observers_pending_) {
    return;
  }

  // |hold_back_updates_timer_| prevents the scheduler from sending multiple
  // notifications within a specified time period from asynchronous tasks. Extra
  // notifications are combined into one and delayed.
  if (hold_back_updates_timer_.IsRunning()) {
    update_after_hold_back_ = true;
    return;
  }
  constexpr base::TimeDelta kTimeToHoldBackUpdates = base::Milliseconds(300);
  hold_back_updates_timer_.Start(
      FROM_HERE, kTimeToHoldBackUpdates,
      base::BindOnce(
          &CertProvisioningSchedulerImpl::OnHoldBackUpdatesTimerExpired,
          weak_factory_.GetWeakPtr()));

  notify_observers_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CertProvisioningSchedulerImpl::NotifyObserversVisibleStateChanged,
          weak_factory_.GetWeakPtr()));
}

void CertProvisioningSchedulerImpl::OnHoldBackUpdatesTimerExpired() {
  if (update_after_hold_back_) {
    update_after_hold_back_ = false;
    NotifyObserversVisibleStateChanged();
  }
}

void CertProvisioningSchedulerImpl::NotifyObserversVisibleStateChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notify_observers_pending_ = false;
  observers_.Notify();
}

}  // namespace ash::cert_provisioning
