// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/chromeos/attestation/enrollment_policy_observer.h"
#include "chrome/browser/chromeos/attestation/machine_certificate_uploader_impl.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/heartbeat_scheduler.h"
#include "chrome/browser/chromeos/policy/policy_pref_names.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_commands_factory_chromeos.h"
#include "chrome/browser/chromeos/policy/rsu/lookup_key_uploader.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_constants.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/sha2.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Well-known requisition types.
const char kNoRequisition[] = "none";
const char kRemoraRequisition[] = "remora";
const char kSharkRequisition[] = "shark";
const char kRialtoRequisition[] = "rialto";

// Zero-touch enrollment flag values.

const char kZeroTouchEnrollmentForced[] = "forced";
const char kZeroTouchEnrollmentHandsOff[] = "hands-off";

// Default frequency for uploading enterprise status reports. Can be overriden
// by Device Policy.
constexpr base::TimeDelta kDeviceStatusUploadFrequency =
    base::TimeDelta::FromHours(3);

// Fetches a machine statistic value from StatisticsProvider, returns an empty
// string on failure.
std::string GetMachineStatistic(const std::string& key) {
  std::string value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(key, &value))
    return std::string();

  return value;
}

// Gets a machine flag from StatisticsProvider, returns the given
// |default_value| if not present.
bool GetMachineFlag(const std::string& key, bool default_value) {
  bool value = default_value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineFlag(key, &value))
    return default_value;

  return value;
}

// Checks whether forced re-enrollment is enabled.
bool ForcedReEnrollmentEnabled() {
  return chromeos::AutoEnrollmentController::IsFREEnabled();
}

}  // namespace

DeviceCloudPolicyManagerChromeOS::DeviceCloudPolicyManagerChromeOS(
    std::unique_ptr<DeviceCloudPolicyStoreChromeOS> store,
    std::unique_ptr<CloudExternalDataManager> external_data_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ServerBackedStateKeysBroker* state_keys_broker)
    : CloudPolicyManager(
          dm_protocol::kChromeDevicePolicyType,
          std::string(),
          store.get(),
          task_runner,
          base::BindRepeating(&content::GetNetworkConnectionTracker)),
      device_store_(std::move(store)),
      external_data_manager_(std::move(external_data_manager)),
      state_keys_broker_(state_keys_broker),
      task_runner_(task_runner),
      local_state_(nullptr) {}

DeviceCloudPolicyManagerChromeOS::~DeviceCloudPolicyManagerChromeOS() {}

void DeviceCloudPolicyManagerChromeOS::Initialize(PrefService* local_state) {
  CHECK(local_state);

  local_state_ = local_state;

  state_keys_update_subscription_ = state_keys_broker_->RegisterUpdateCallback(
      base::Bind(&DeviceCloudPolicyManagerChromeOS::OnStateKeysUpdated,
                 base::Unretained(this)));

  InitializeRequisition();
}

void DeviceCloudPolicyManagerChromeOS::AddDeviceCloudPolicyManagerObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceCloudPolicyManagerChromeOS::RemoveDeviceCloudPolicyManagerObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::string DeviceCloudPolicyManagerChromeOS::GetDeviceRequisition() const {
  std::string requisition;
  const PrefService::Preference* pref = local_state_->FindPreference(
      prefs::kDeviceEnrollmentRequisition);
  if (!pref->IsDefaultValue())
    pref->GetValue()->GetAsString(&requisition);

  if (requisition == kNoRequisition)
    requisition.clear();

  return requisition;
}

void DeviceCloudPolicyManagerChromeOS::SetDeviceRequisition(
    const std::string& requisition) {
  VLOG(1) << "SetDeviceRequisition " << requisition;
  if (local_state_) {
    if (requisition.empty()) {
      local_state_->ClearPref(prefs::kDeviceEnrollmentRequisition);
      local_state_->ClearPref(prefs::kDeviceEnrollmentAutoStart);
      local_state_->ClearPref(prefs::kDeviceEnrollmentCanExit);
    } else {
      local_state_->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
      if (requisition == kNoRequisition) {
        local_state_->ClearPref(prefs::kDeviceEnrollmentAutoStart);
        local_state_->ClearPref(prefs::kDeviceEnrollmentCanExit);
      } else {
        SetDeviceEnrollmentAutoStart();
      }
    }
  }
}

bool DeviceCloudPolicyManagerChromeOS::IsRemoraRequisition() const {
  return GetDeviceRequisition() == kRemoraRequisition;
}

bool DeviceCloudPolicyManagerChromeOS::IsSharkRequisition() const {
  return GetDeviceRequisition() == kSharkRequisition;
}

std::string DeviceCloudPolicyManagerChromeOS::GetSubOrganization() const {
  if (!local_state_)
    return std::string();
  std::string sub_organization;
  const PrefService::Preference* pref =
      local_state_->FindPreference(prefs::kDeviceEnrollmentSubOrganization);
  if (!pref->IsDefaultValue())
    pref->GetValue()->GetAsString(&sub_organization);
  return sub_organization;
}

void DeviceCloudPolicyManagerChromeOS::SetSubOrganization(
    const std::string& sub_organization) {
  if (!local_state_)
    return;
  if (sub_organization.empty())
    local_state_->ClearPref(prefs::kDeviceEnrollmentSubOrganization);
  else
    local_state_->SetString(prefs::kDeviceEnrollmentSubOrganization,
                            sub_organization);
}

void DeviceCloudPolicyManagerChromeOS::SetDeviceEnrollmentAutoStart() {
  if (local_state_) {
    local_state_->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
    local_state_->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
  }
}

void DeviceCloudPolicyManagerChromeOS::Shutdown() {
  status_uploader_.reset();
  syslog_uploader_.reset();
  heartbeat_scheduler_.reset();
  state_keys_update_subscription_.reset();
  external_data_manager_->Disconnect();
  CloudPolicyManager::Shutdown();
  signin_profile_forwarding_schema_registry_.reset();
}

// static
void DeviceCloudPolicyManagerChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterStringPref(prefs::kDeviceEnrollmentSubOrganization,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentCanExit, true);
  registry->RegisterDictionaryPref(prefs::kServerBackedDeviceState);
  registry->RegisterBooleanPref(prefs::kRemoveUsersRemoteCommand, false);
  registry->RegisterStringPref(prefs::kLastRsuDeviceIdUploaded, std::string());
  registry->RegisterListPref(prefs::kStoreLogStatesAcrossReboots);
}

// static
ZeroTouchEnrollmentMode
DeviceCloudPolicyManagerChromeOS::GetZeroTouchEnrollmentMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          chromeos::switches::kEnterpriseEnableZeroTouchEnrollment)) {
    return ZeroTouchEnrollmentMode::DISABLED;
  }

  std::string value = command_line->GetSwitchValueASCII(
      chromeos::switches::kEnterpriseEnableZeroTouchEnrollment);
  if (value == kZeroTouchEnrollmentForced) {
    return ZeroTouchEnrollmentMode::FORCED;
  }
  if (value == kZeroTouchEnrollmentHandsOff) {
    return ZeroTouchEnrollmentMode::HANDS_OFF;
  }
  if (value.empty()) {
    return ZeroTouchEnrollmentMode::ENABLED;
  }
  LOG(WARNING) << "Malformed value \"" << value << "\" for switch --"
               << chromeos::switches::kEnterpriseEnableZeroTouchEnrollment
               << ". Ignoring switch.";
  return ZeroTouchEnrollmentMode::DISABLED;
}

void DeviceCloudPolicyManagerChromeOS::StartConnection(
    std::unique_ptr<CloudPolicyClient> client_to_connect,
    chromeos::InstallAttributes* install_attributes) {
  CHECK(!service());

  // Set state keys here so the first policy fetch submits them to the server.
  if (ForcedReEnrollmentEnabled())
    client_to_connect->SetStateKeysToUpload(state_keys_broker_->state_keys());

  // Create the component cloud policy service for fetching, caching and
  // exposing policy for extensions.
  if (!component_policy_disabled_for_testing_) {
    base::FilePath component_policy_cache_dir;
    CHECK(base::PathService::Get(chromeos::DIR_SIGNIN_PROFILE_COMPONENT_POLICY,
                                 &component_policy_cache_dir));
    CHECK(signin_profile_forwarding_schema_registry_);
    CreateComponentCloudPolicyService(
        dm_protocol::kChromeSigninExtensionPolicyType,
        component_policy_cache_dir, POLICY_SOURCE_CLOUD,
        client_to_connect.get(),
        signin_profile_forwarding_schema_registry_.get());
  }

  core()->Connect(std::move(client_to_connect));
  core()->StartRefreshScheduler();
  core()->RefreshSoon();
  core()->TrackRefreshDelayPref(local_state_,
                                prefs::kDevicePolicyRefreshRate);

  external_data_manager_->Connect(
      g_browser_process->shared_url_loader_factory());

  enrollment_certificate_uploader_.reset(
      new chromeos::attestation::EnrollmentCertificateUploaderImpl(client()));
  enrollment_policy_observer_.reset(
      new chromeos::attestation::EnrollmentPolicyObserver(client()));
  lookup_key_uploader_.reset(
      new LookupKeyUploader(device_store(), g_browser_process->local_state(),
                            enrollment_certificate_uploader_.get()));

  // Don't create a MachineCertificateUploader or start the
  // AttestationPolicyObserver if machine cert requests are disabled.
  if (!(base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableMachineCertRequest))) {
    machine_certificate_uploader_.reset(
        new chromeos::attestation::MachineCertificateUploaderImpl(client()));
    attestation_policy_observer_.reset(
        new chromeos::attestation::AttestationPolicyObserver(
            machine_certificate_uploader_.get()));
  }

  // Start remote commands services now that we have setup everything they need.
  core()->StartRemoteCommandsService(
      std::make_unique<DeviceCommandsFactoryChromeOS>(this));

  // Enable device reporting and status monitoring for cloud managed devices. We
  // want to create these objects even if monitoring is currently inactive, in
  // case monitoring is turned back on in a future policy fetch - the classes
  // themselves track the current state of the monitoring settings and only
  // perform monitoring if it is active.
  if (install_attributes->IsCloudManaged()) {
    CreateStatusUploader();
    syslog_uploader_.reset(new SystemLogUploader(nullptr, task_runner_));
    heartbeat_scheduler_.reset(new HeartbeatScheduler(
        g_browser_process->gcm_driver(), client(),
        install_attributes->GetDomain(), install_attributes->GetDeviceId(),
        task_runner_));
  }

  NotifyConnected();
}

void DeviceCloudPolicyManagerChromeOS::Unregister(
    const UnregisterCallback& callback) {
  if (!service()) {
    LOG(ERROR) << "Tried to unregister but DeviceCloudPolicyManagerChromeOS is "
               << "not connected.";
    callback.Run(false);
    return;
  }

  service()->Unregister(callback);
}

void DeviceCloudPolicyManagerChromeOS::Disconnect() {
  status_uploader_.reset();
  syslog_uploader_.reset();
  heartbeat_scheduler_.reset();
  core()->Disconnect();

  NotifyDisconnected();
}

void DeviceCloudPolicyManagerChromeOS::SetSigninProfileSchemaRegistry(
    SchemaRegistry* schema_registry) {
  DCHECK(!signin_profile_forwarding_schema_registry_);
  signin_profile_forwarding_schema_registry_.reset(
      new ForwardingSchemaRegistry(schema_registry));
}

void DeviceCloudPolicyManagerChromeOS::OnStateKeysUpdated() {
  if (client() && ForcedReEnrollmentEnabled())
    client()->SetStateKeysToUpload(state_keys_broker_->state_keys());
}

void DeviceCloudPolicyManagerChromeOS::InitializeRequisition() {
  // OEM statistics are only loaded when OOBE is not completed.
  if (chromeos::StartupUtils::IsOobeCompleted())
    return;

  // Demo requisition may have been set in a prior enrollment attempt that was
  // interrupted.
  chromeos::DemoSetupController::ClearDemoRequisition(this);
  const PrefService::Preference* pref = local_state_->FindPreference(
      prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    std::string requisition =
        GetMachineStatistic(chromeos::system::kOemDeviceRequisitionKey);

    if (!requisition.empty()) {
      local_state_->SetString(prefs::kDeviceEnrollmentRequisition,
                              requisition);
      if (requisition == kRemoraRequisition ||
          requisition == kSharkRequisition ||
          requisition == kRialtoRequisition) {
        SetDeviceEnrollmentAutoStart();
      } else {
        local_state_->SetBoolean(
            prefs::kDeviceEnrollmentAutoStart,
            GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey,
                           false));
        local_state_->SetBoolean(
            prefs::kDeviceEnrollmentCanExit,
            GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                           false));
      }
    }
  }
}

void DeviceCloudPolicyManagerChromeOS::NotifyConnected() {
  for (auto& observer : observers_)
    observer.OnDeviceCloudPolicyManagerConnected();
}

void DeviceCloudPolicyManagerChromeOS::NotifyDisconnected() {
  for (auto& observer : observers_)
    observer.OnDeviceCloudPolicyManagerDisconnected();
}

void DeviceCloudPolicyManagerChromeOS::CreateStatusUploader() {
  status_uploader_.reset(new StatusUploader(
      client(),
      std::make_unique<DeviceStatusCollector>(
          local_state_, chromeos::system::StatisticsProvider::GetInstance(),
          DeviceStatusCollector::VolumeInfoFetcher(),
          DeviceStatusCollector::CPUStatisticsFetcher(),
          DeviceStatusCollector::CPUTempFetcher(),
          DeviceStatusCollector::AndroidStatusFetcher(),
          DeviceStatusCollector::TpmStatusFetcher(),
          DeviceStatusCollector::EMMCLifetimeFetcher(),
          DeviceStatusCollector::StatefulPartitionInfoFetcher(),
          DeviceStatusCollector::CrosHealthdDataFetcher()),
      task_runner_, kDeviceStatusUploadFrequency));
}

}  // namespace policy
