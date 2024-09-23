// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/ash/attestation/enrollment_id_upload_manager.h"
#include "chrome/browser/ash/attestation/machine_certificate_uploader_impl.h"
#include "chrome/browser/ash/login/reporting/lock_unlock_reporter.h"
#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/policy_pref_names.h"
#include "chrome/browser/ash/policy/core/reporting_user_tracker.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/networking/euicc_status_uploader.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/policy/remote_commands/device_commands_factory_ash.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/policy/reporting/os_updates/os_updates_reporter.h"
#include "chrome/browser/ash/policy/reporting/user_added_removed/user_added_removed_reporter.h"
#include "chrome/browser/ash/policy/rsu/lookup_key_uploader.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/policy/uploading/heartbeat_scheduler.h"
#include "chrome/browser/ash/policy/uploading/status_uploader.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// Default frequency for uploading enterprise status reports. Can be overriden
// by Device Policy.
// Keep the default value in sync with device_status_frequency in
// DeviceReportingProto in components/policy/proto/chrome_device_policy.proto.
constexpr base::TimeDelta kDeviceStatusUploadFrequency = base::Hours(3);

}  // namespace

DeviceCloudPolicyManagerAsh::DeviceCloudPolicyManagerAsh(
    std::unique_ptr<DeviceCloudPolicyStoreAsh> device_store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ServerBackedStateKeysBroker* state_keys_broker,
    StartCrdSessionJobDelegate& crd_delegate)
    : CloudPolicyManager(
          dm_protocol::kChromeDevicePolicyType,
          std::string(),
          std::move(device_store),
          task_runner,
          base::BindRepeating(&content::GetNetworkConnectionTracker)),
      device_store_(static_cast<DeviceCloudPolicyStoreAsh*>(store())),
      external_data_manager_(std::move(external_data_manager)),
      state_keys_broker_(state_keys_broker),
      crd_delegate_(&crd_delegate),
      task_runner_(task_runner),
      local_state_(nullptr),
      helper_(std::make_unique<reporting::UserEventReporterHelper>(
          reporting::Destination::UNDEFINED_DESTINATION)) {}

DeviceCloudPolicyManagerAsh::~DeviceCloudPolicyManagerAsh() = default;

void DeviceCloudPolicyManagerAsh::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);

  store()->AddObserver(this);

  // If the underlying store is already initialized, pretend it was loaded now.
  // Note: It is not enough to just copy OnStoreLoaded's contents here because
  // subclasses can override it.
  if (store()->is_initialized()) {
    OnStoreLoaded(store());
  }
}

void DeviceCloudPolicyManagerAsh::Initialize(PrefService* local_state) {
  CHECK(local_state);

  local_state_ = local_state;

  // If FRE is enabled, we'll want to know about re-enrollment state keys.
  if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
    state_keys_update_subscription_ =
        state_keys_broker_->RegisterUpdateCallback(base::BindRepeating(
            &DeviceCloudPolicyManagerAsh::OnStateKeysUpdated,
            base::Unretained(this)));
  }
}

void DeviceCloudPolicyManagerAsh::AddDeviceCloudPolicyManagerObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceCloudPolicyManagerAsh::RemoveDeviceCloudPolicyManagerObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Keep clean up order as the reversed creation order.
void DeviceCloudPolicyManagerAsh::Shutdown() {
  os_updates_reporter_.reset();
  metric_reporting_manager_.reset();
  lock_unlock_reporter_.reset();
  login_logout_reporter_.reset();
  user_added_removed_reporter_.reset();
  heartbeat_scheduler_.reset();
  syslog_uploader_.reset();
  status_uploader_.reset();
  crd_delegate_ = nullptr;
  state_keys_broker_ = nullptr;
  managed_session_service_.reset();
  euicc_status_uploader_.reset();
  core()->Disconnect();
  machine_certificate_uploader_.reset();
  external_data_manager_->Disconnect();
  state_keys_update_subscription_ = {};
  CloudPolicyManager::Shutdown();
  signin_profile_forwarding_schema_registry_.reset();
}

// static
void DeviceCloudPolicyManagerAsh::RegisterPrefs(PrefRegistrySimple* registry) {
  ReportingUserTracker::RegisterPrefs(registry);

  registry->RegisterDictionaryPref(::prefs::kServerBackedDeviceState);
  registry->RegisterBooleanPref(::prefs::kRemoveUsersRemoteCommand, false);
  registry->RegisterStringPref(::prefs::kLastRsuDeviceIdUploaded,
                               std::string());
  registry->RegisterListPref(prefs::kStoreLogStatesAcrossReboots);
}

void DeviceCloudPolicyManagerAsh::StartConnection(
    std::unique_ptr<CloudPolicyClient> client_to_connect,
    ash::InstallAttributes* install_attributes) {
  CHECK(!service());

  // Set state keys here so the first policy fetch submits them to the server.
  if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
    client_to_connect->SetStateKeysToUpload(state_keys_broker_->state_keys());
  }

  // Create the component cloud policy service for fetching, caching and
  // exposing policy for extensions.
  if (!component_policy_disabled_for_testing_) {
    const base::FilePath component_policy_cache_dir =
        base::PathService::CheckedGet(ash::DIR_SIGNIN_PROFILE_COMPONENT_POLICY);
    CHECK(signin_profile_forwarding_schema_registry_);
    CreateComponentCloudPolicyService(
        dm_protocol::kChromeSigninExtensionPolicyType,
        component_policy_cache_dir, client_to_connect.get(),
        signin_profile_forwarding_schema_registry_.get());
  }

  core()->Connect(std::move(client_to_connect));
  core()->StartRefreshScheduler();
  core()->RefreshSoon(PolicyFetchReason::kBrowserStart);
  core()->TrackRefreshDelayPref(local_state_,
                                ::prefs::kDevicePolicyRefreshRate);

  external_data_manager_->Connect(
      g_browser_process->shared_url_loader_factory());

  enrollment_certificate_uploader_ =
      std::make_unique<ash::attestation::EnrollmentCertificateUploaderImpl>(
          client());
  enrollment_id_upload_manager_ =
      std::make_unique<ash::attestation::EnrollmentIdUploadManager>(
          client(), enrollment_certificate_uploader_.get());
  lookup_key_uploader_ = std::make_unique<LookupKeyUploader>(
      device_store(), g_browser_process->local_state(),
      enrollment_certificate_uploader_.get());
  euicc_status_uploader_ = std::make_unique<EuiccStatusUploader>(
      client(), g_browser_process->local_state());

  // Don't create a MachineCertificateUploader if machine cert requests are
  // disabled.
  if (!(base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kDisableMachineCertRequest))) {
    machine_certificate_uploader_ =
        std::make_unique<ash::attestation::MachineCertificateUploaderImpl>(
            client());
    machine_certificate_uploader_->UploadCertificateIfNeeded(base::DoNothing());
  }

  // Start remote commands services now that we have setup everything they need.
  core()->StartRemoteCommandsService(
      std::make_unique<DeviceCommandsFactoryAsh>(
          machine_certificate_uploader_.get(), *crd_delegate_),
      PolicyInvalidationScope::kDevice);

  // Enable device reporting and status monitoring for cloud managed devices. We
  // want to create these objects even if monitoring is currently inactive, in
  // case monitoring is turned back on in a future policy fetch - the classes
  // themselves track the current state of the monitoring settings and only
  // perform monitoring if it is active.
  if (install_attributes->IsCloudManaged()) {
    CreateManagedSessionServiceAndReporters();
    CreateStatusUploader(managed_session_service_.get());
    syslog_uploader_ =
        std::make_unique<SystemLogUploader>(nullptr, task_runner_);
    if (base::FeatureList::IsEnabled(
            chromeos::features::kKioskHeartbeatsViaERP)) {
      // Do nothing as heartbeats go over ERP.
    } else {
      // Initialize legacy GCM heartbeat (default behaviour)
      heartbeat_scheduler_ = std::make_unique<HeartbeatScheduler>(
          g_browser_process->gcm_driver(), client(), device_store_.get(),
          install_attributes->GetDeviceId(), task_runner_);
    }
    metric_reporting_manager_ = reporting::MetricReportingManager::Create(
        managed_session_service_.get());
    os_updates_reporter_ = reporting::OsUpdatesReporter::Create();
  }

  NotifyConnected();
}

void DeviceCloudPolicyManagerAsh::OnPolicyStoreReady(
    ash::InstallAttributes* install_attributes) {
  if (!install_attributes->IsCloudManaged()) {
    return;
  }
  CreateManagedSessionServiceAndReporters();
}

void DeviceCloudPolicyManagerAsh::SetSigninProfileSchemaRegistry(
    SchemaRegistry* schema_registry) {
  DCHECK(!signin_profile_forwarding_schema_registry_);
  signin_profile_forwarding_schema_registry_ =
      std::make_unique<ForwardingSchemaRegistry>(schema_registry);
  NotifyGotRegistry();
}

void DeviceCloudPolicyManagerAsh::OnUserManagerCreated(
    user_manager::UserManager* user_manager) {
  user_manager_observation_.Observe(user_manager);
  reporting_user_tracker_ =
      std::make_unique<ReportingUserTracker>(user_manager);
}

void DeviceCloudPolicyManagerAsh::OnUserManagerWillBeDestroyed() {
  // DeviceStatusCollector internally holds the reference to the
  // ReportingUserTracker instance, so should be released via Shutdown()
  // before this is reached.
  DCHECK(!status_uploader_);
  reporting_user_tracker_.reset();
  user_manager_observation_.Reset();
}

void DeviceCloudPolicyManagerAsh::OnUserToBeRemoved(
    const AccountId& account_id) {
  // This logic needs to be consistent with UserAddedRemovedReporter.
  if (!helper_->ReportingEnabled(ash::kReportDeviceLoginLogout)) {
    return;
  }
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user || user->IsKioskType() ||
      user->GetType() == user_manager::UserType::kPublicAccount ||
      user->GetType() == user_manager::UserType::kGuest) {
    return;
  }

  const std::string email = account_id.GetUserEmail();
  users_to_be_removed_.insert_or_assign(
      account_id, reporting_user_tracker_->ShouldReportUser(email));
}

void DeviceCloudPolicyManagerAsh::OnUserRemoved(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  // This logic needs to be consistent with UserAddedRemovedReporter.
  auto it = users_to_be_removed_.find(account_id);
  if (it == users_to_be_removed_.end()) {
    return;
  }

  bool is_affiliated_user = it->second;
  users_to_be_removed_.erase(it);

  // Check the pref, after we update the tracking map.
  if (!helper_->ReportingEnabled(ash::kReportDeviceLoginLogout)) {
    return;
  }

  // Unlike UserAddedRemovedReporter, instead of reporting, we cache
  // the removed user here.
  // They're going to be reported, once the reporter instance is created.
  removed_users_.emplace_back(
      RemovedUser{is_affiliated_user ? account_id.GetUserEmail() : "", reason});
}

void DeviceCloudPolicyManagerAsh::OnStateKeysUpdated() {
  // TODO(b/181140445): If we had a separate state keys upload request to DM
  // Server we should call it here.
  if (client()) {
    client()->SetStateKeysToUpload(state_keys_broker_->state_keys());
  }
}

void DeviceCloudPolicyManagerAsh::NotifyConnected() {
  for (auto& observer : observers_) {
    observer.OnDeviceCloudPolicyManagerConnected();
  }
}

void DeviceCloudPolicyManagerAsh::NotifyGotRegistry() {
  for (auto& observer : observers_) {
    observer.OnDeviceCloudPolicyManagerGotRegistry();
  }
}

void DeviceCloudPolicyManagerAsh::CreateStatusUploader(
    ManagedSessionService* managed_session_service) {
  CHECK(reporting_user_tracker_.get());
  auto collector = std::make_unique<DeviceStatusCollector>(
      local_state_, reporting_user_tracker_.get(),
      ash::system::StatisticsProvider::GetInstance(), managed_session_service);

  status_uploader_ = std::make_unique<StatusUploader>(
      client(), std::move(collector), task_runner_,
      kDeviceStatusUploadFrequency);
}

void DeviceCloudPolicyManagerAsh::CreateManagedSessionServiceAndReporters() {
  if (managed_session_service_) {
    return;
  }

  if (auto* user_manager = user_manager::UserManager::Get()) {
    user_manager->RemoveObserver(this);
  }

  managed_session_service_ = std::make_unique<ManagedSessionService>();
  login_logout_reporter_ = ash::reporting::LoginLogoutReporter::Create(
      managed_session_service_.get());

  user_added_removed_reporter_ = ::reporting::UserAddedRemovedReporter::Create(
      std::move(users_to_be_removed_), managed_session_service_.get());
  for (const auto& [user_email, reason] : removed_users_) {
    user_added_removed_reporter_->ProcessRemovedUser(user_email, reason);
  }
  removed_users_.clear();

  lock_unlock_reporter_ = ash::reporting::LockUnlockReporter::Create(
      managed_session_service_.get());
}

HeartbeatScheduler*
DeviceCloudPolicyManagerAsh::GetHeartbeatSchedulerForTesting() const {
  return heartbeat_scheduler_.get();
}

reporting::OsUpdatesReporter*
DeviceCloudPolicyManagerAsh::GetOsUpdatesReporter() const {
  return os_updates_reporter_.get();
}

reporting::MetricReportingManager*
DeviceCloudPolicyManagerAsh::GetMetricReportingManager() {
  return metric_reporting_manager_.get();
}
}  // namespace policy
