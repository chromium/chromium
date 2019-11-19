// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_resources.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/arc/arc_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

using ErrorCode = DemoSetupController::DemoSetupError::ErrorCode;
using RecoveryMethod = DemoSetupController::DemoSetupError::RecoveryMethod;

constexpr char kDemoRequisition[] = "cros-demo-mode";
constexpr char kOfflinePolicyDirectoryName[] = "policy";
constexpr char kOfflineDevicePolicyFileName[] = "device_policy";
constexpr char kOfflineDeviceLocalAccountPolicyFileName[] =
    "local_account_policy";

// Get the DeviceLocalAccountPolicyStore for the account_id.
policy::CloudPolicyStore* GetDeviceLocalAccountPolicyStore(
    const std::string& account_id) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return nullptr;

  policy::DeviceLocalAccountPolicyService* local_account_service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!local_account_service)
    return nullptr;

  const std::string user_id = policy::GenerateDeviceLocalAccountUserId(
      account_id, policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
  policy::DeviceLocalAccountPolicyBroker* broker =
      local_account_service->GetBrokerForUser(user_id);
  if (!broker)
    return nullptr;

  return broker->core()->store();
}

// A utility function of base::ReadFileToString which returns an optional
// string.
// TODO(mukai): move this to base/files.
base::Optional<std::string> ReadFileToOptionalString(
    const base::FilePath& file_path) {
  std::string content;
  base::Optional<std::string> result;
  if (base::ReadFileToString(file_path, &content))
    result = std::move(content);
  return result;
}

// Returns whether online FRE check is required.
bool IsOnlineFreCheckRequired() {
  AutoEnrollmentController::FRERequirement fre_requirement =
      AutoEnrollmentController::GetFRERequirement();
  bool enrollment_check_required =
      fre_requirement !=
          AutoEnrollmentController::FRERequirement::kExplicitlyNotRequired &&
      fre_requirement !=
          AutoEnrollmentController::FRERequirement::kNotRequired &&
      AutoEnrollmentController::IsFREEnabled();

  if (!enrollment_check_required)
    return false;

  std::string block_dev_mode_value;
  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  provider->GetMachineStatistic(system::kBlockDevModeKey,
                                &block_dev_mode_value);

  return block_dev_mode_value == "1";
}

DemoSetupController::DemoSetupError CreateFromClientStatus(
    policy::DeviceManagementStatus status,
    const std::string& debug_message) {
  switch (status) {
    case policy::DM_STATUS_SUCCESS:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_REQUEST_INVALID:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kInvalidRequest, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_REQUEST_FAILED:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kRequestNetworkError, RecoveryMethod::kCheckNetwork,
          debug_message);
    case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kTemporaryUnavailable, RecoveryMethod::kRetry,
          debug_message);
    case policy::DM_STATUS_HTTP_STATUS_ERROR:
    case policy::DM_STATUS_REQUEST_TOO_LARGE:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kResponseError, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_RESPONSE_DECODING_ERROR:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kResponseDecodingError, RecoveryMethod::kUnknown,
          debug_message);
    case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
    case policy::DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
    case policy::DM_STATUS_SERVICE_ACTIVATION_PENDING:
      return DemoSetupController::DemoSetupError(ErrorCode::kDemoAccountError,
                                                 RecoveryMethod::kUnknown,
                                                 debug_message);
    case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kDeviceNotFound, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kInvalidDMToken, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kInvalidSerialNumber, RecoveryMethod::kUnknown,
          debug_message);
    case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kDeviceIdError, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kLicenseError, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_SERVICE_DEPROVISIONED:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kDeviceDeprovisioned, RecoveryMethod::kUnknown,
          debug_message);
    case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kDomainMismatch, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_CANNOT_SIGN_REQUEST:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kSigningError, RecoveryMethod::kPowerwash, debug_message);
    case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kPolicyNotFound, RecoveryMethod::kUnknown, debug_message);
    case policy::DM_STATUS_SERVICE_ARC_DISABLED:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kArcError, RecoveryMethod::kUnknown, debug_message);
  }
  NOTREACHED() << "Demo mode setup received unsupported client status";
  return DemoSetupController::DemoSetupError(
      ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown, debug_message);
}

DemoSetupController::DemoSetupError CreateFromLockStatus(
    InstallAttributes::LockResult status,
    const std::string& debug_message) {
  switch (status) {
    case InstallAttributes::LOCK_SUCCESS:
    case InstallAttributes::LOCK_NOT_READY:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown, debug_message);
    case InstallAttributes::LOCK_TIMEOUT:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kLockTimeout, RecoveryMethod::kReboot, debug_message);
    case InstallAttributes::LOCK_BACKEND_INVALID:
    case InstallAttributes::LOCK_SET_ERROR:
    case InstallAttributes::LOCK_FINALIZE_ERROR:
    case InstallAttributes::LOCK_READBACK_ERROR:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kLockError, RecoveryMethod::kPowerwash, debug_message);
    case InstallAttributes::LOCK_ALREADY_LOCKED:
    case InstallAttributes::LOCK_WRONG_DOMAIN:
    case InstallAttributes::LOCK_WRONG_MODE:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kAlreadyLocked, RecoveryMethod::kPowerwash, debug_message);
  }
  NOTREACHED() << "Demo mode setup received unsupported lock status";
  return DemoSetupController::DemoSetupError(
      ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown, debug_message);
}

}  //  namespace

// static
DemoSetupController::DemoSetupError
DemoSetupController::DemoSetupError::CreateFromEnrollmentStatus(
    const policy::EnrollmentStatus& status) {
  const std::string debug_message = base::StringPrintf(
      "EnrollmentError: (status: %d, client_status: %d, store_status: %d, "
      "validation_status: %d, lock_status: %d)",
      status.status(), status.client_status(), status.store_status(),
      status.validation_status(), status.lock_status());

  switch (status.status()) {
    case policy::EnrollmentStatus::SUCCESS:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::NO_STATE_KEYS:
      return DemoSetupError(ErrorCode::kNoStateKeys, RecoveryMethod::kReboot,
                            debug_message);
    case policy::EnrollmentStatus::REGISTRATION_FAILED:
      return CreateFromClientStatus(status.client_status(), debug_message);
    case policy::EnrollmentStatus::ROBOT_AUTH_FETCH_FAILED:
    case policy::EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED:
      return DemoSetupError(ErrorCode::kRobotFetchError,
                            RecoveryMethod::kCheckNetwork, debug_message);
    case policy::EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED:
      return DemoSetupError(ErrorCode::kRobotStoreError,
                            RecoveryMethod::kReboot, debug_message);
    case policy::EnrollmentStatus::REGISTRATION_BAD_MODE:
      return DemoSetupError(ErrorCode::kBadMode, RecoveryMethod::kRetry,
                            debug_message);
    case policy::EnrollmentStatus::REGISTRATION_CERT_FETCH_FAILED:
      return DemoSetupError(ErrorCode::kCertFetchError, RecoveryMethod::kRetry,
                            debug_message);
    case policy::EnrollmentStatus::POLICY_FETCH_FAILED:
      return DemoSetupError(ErrorCode::kPolicyFetchError,
                            RecoveryMethod::kRetry, debug_message);
    case policy::EnrollmentStatus::VALIDATION_FAILED:
      return DemoSetupError(ErrorCode::kPolicyValidationError,
                            RecoveryMethod::kRetry, debug_message);
    case policy::EnrollmentStatus::LOCK_ERROR:
      return CreateFromLockStatus(status.lock_status(), debug_message);
    case policy::EnrollmentStatus::STORE_ERROR:
      return DemoSetupError(ErrorCode::kOnlineStoreError,
                            RecoveryMethod::kRetry, debug_message);
    case policy::EnrollmentStatus::ATTRIBUTE_UPDATE_FAILED:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::NO_MACHINE_IDENTIFICATION:
      return DemoSetupError(ErrorCode::kMachineIdentificationError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::ACTIVE_DIRECTORY_POLICY_FETCH_FAILED:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kReboot, debug_message);
    case policy::EnrollmentStatus::DM_TOKEN_STORE_FAILED:
      return DemoSetupError(ErrorCode::kDMTokenStoreError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::LICENSE_REQUEST_FAILED:
      return DemoSetupError(ErrorCode::kLicenseError, RecoveryMethod::kUnknown,
                            debug_message);
    case policy::EnrollmentStatus::OFFLINE_POLICY_LOAD_FAILED:
    case policy::EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED:
      return DemoSetupError(ErrorCode::kOfflinePolicyError,
                            RecoveryMethod::kOnlineOnly, debug_message);
  }
  NOTREACHED() << "Demo mode setup received unsupported enrollment status";
  return DemoSetupError(ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown,
                        debug_message);
}

// static
DemoSetupController::DemoSetupError
DemoSetupController::DemoSetupError::CreateFromOtherEnrollmentError(
    EnterpriseEnrollmentHelper::OtherError error) {
  const std::string debug_message =
      base::StringPrintf("Other error: %d", error);
  switch (error) {
    case EnterpriseEnrollmentHelper::OTHER_ERROR_DOMAIN_MISMATCH:
      return DemoSetupError(ErrorCode::kAlreadyLocked,
                            RecoveryMethod::kPowerwash, debug_message);
    case EnterpriseEnrollmentHelper::OTHER_ERROR_FATAL:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
  }
  NOTREACHED() << "Demo mode setup received unsupported enrollment error";
  return DemoSetupError(ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown,
                        debug_message);
}

// static
DemoSetupController::DemoSetupError
DemoSetupController::DemoSetupError::CreateFromComponentError(
    component_updater::CrOSComponentManager::Error error) {
  const std::string debug_message =
      "Failed to load demo resources CrOS component with error: " +
      std::to_string(static_cast<int>(error));
  return DemoSetupError(ErrorCode::kOnlineComponentError,
                        RecoveryMethod::kCheckNetwork, debug_message);
}

DemoSetupController::DemoSetupError::DemoSetupError(
    DemoSetupError::ErrorCode error_code,
    DemoSetupError::RecoveryMethod recovery_method)
    : error_code_(error_code), recovery_method_(recovery_method) {}

DemoSetupController::DemoSetupError::DemoSetupError(
    DemoSetupError::ErrorCode error_code,
    DemoSetupError::RecoveryMethod recovery_method,
    const std::string& debug_message)
    : error_code_(error_code),
      recovery_method_(recovery_method),
      debug_message_(debug_message) {}

DemoSetupController::DemoSetupError::~DemoSetupError() = default;

base::string16 DemoSetupController::DemoSetupError::GetLocalizedErrorMessage()
    const {
  switch (error_code_) {
    case ErrorCode::kOfflinePolicyError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_OFFLINE_POLICY_ERROR);
    case ErrorCode::kOfflinePolicyStoreError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_OFFLINE_STORE_ERROR);
    case ErrorCode::kOnlineFRECheckRequired:
      return l10n_util::GetStringUTF16(
          IDS_DEMO_SETUP_OFFLINE_UNAVAILABLE_ERROR);
    case ErrorCode::kOnlineComponentError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_COMPONENT_ERROR);
    case ErrorCode::kNoStateKeys:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_NO_STATE_KEYS_ERROR);
    case ErrorCode::kInvalidRequest:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_INVALID_REQUEST_ERROR);
    case ErrorCode::kRequestNetworkError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_NETWORK_ERROR);
    case ErrorCode::kTemporaryUnavailable:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_TEMPORARY_ERROR);
    case ErrorCode::kResponseError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RESPONSE_ERROR);
    case ErrorCode::kResponseDecodingError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RESPONSE_DECODING_ERROR);
    case ErrorCode::kDemoAccountError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_ACCOUNT_ERROR);
    case ErrorCode::kDeviceNotFound:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_DEVICE_NOT_FOUND_ERROR);
    case ErrorCode::kInvalidDMToken:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_INVALID_DM_TOKEN_ERROR);
    case ErrorCode::kInvalidSerialNumber:
      return l10n_util::GetStringUTF16(
          IDS_DEMO_SETUP_INVALID_SERIAL_NUMBER_ERROR);
    case ErrorCode::kDeviceIdError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_DEVICE_ID_ERROR);
    case ErrorCode::kLicenseError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_LICENSE_ERROR);
    case ErrorCode::kDeviceDeprovisioned:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_DEPROVISIONED_ERROR);
    case ErrorCode::kDomainMismatch:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_DOMAIN_MISMATCH_ERROR);
    case ErrorCode::kSigningError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_SIGNING_ERROR);
    case ErrorCode::kPolicyNotFound:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_POLICY_NOT_FOUND_ERROR);
    case ErrorCode::kArcError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_ARC_ERROR);
    case ErrorCode::kRobotFetchError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_ROBOT_ERROR);
    case ErrorCode::kRobotStoreError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_ROBOT_STORE_ERROR);
    case ErrorCode::kBadMode:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_BAD_MODE_ERROR);
    case ErrorCode::kCertFetchError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_CERT_FETCH_ERROR);
    case ErrorCode::kPolicyFetchError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_POLICY_FETCH_ERROR);
    case ErrorCode::kPolicyValidationError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_POLICY_VALIDATION_ERROR);
    case ErrorCode::kLockTimeout:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_LOCK_TIMEOUT_ERROR);
    case ErrorCode::kLockError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_LOCK_ERROR);
    case ErrorCode::kAlreadyLocked:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_ALREADY_LOCKED_ERROR);
    case ErrorCode::kOnlineStoreError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_ONLINE_STORE_ERROR);
    case ErrorCode::kMachineIdentificationError:
      return l10n_util::GetStringUTF16(
          IDS_DEMO_SETUP_NO_MACHINE_IDENTIFICATION_ERROR);
    case ErrorCode::kDMTokenStoreError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_DM_TOKEN_STORE_ERROR);
    case ErrorCode::kUnexpectedError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_UNEXPECTED_ERROR);
  }
  NOTREACHED() << "No localized error message available for demo setup error.";
  return base::string16();
}

base::string16
DemoSetupController::DemoSetupError::GetLocalizedRecoveryMessage() const {
  switch (recovery_method_) {
    case RecoveryMethod::kRetry:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RECOVERY_RETRY);
    case RecoveryMethod::kReboot:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RECOVERY_REBOOT);
    case RecoveryMethod::kPowerwash:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RECOVERY_POWERWASH);
    case RecoveryMethod::kCheckNetwork:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RECOVERY_CHECK_NETWORK);
    case RecoveryMethod::kOnlineOnly:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RECOVERY_OFFLINE_FATAL);
    case RecoveryMethod::kUnknown:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_RECOVERY_FATAL);
  }
  NOTREACHED()
      << "No localized error message available for demo setup recovery method.";
  return base::string16();
}

std::string DemoSetupController::DemoSetupError::GetDebugDescription() const {
  return base::StringPrintf("DemoSetupError (code: %d, recovery: %d) : %s",
                            error_code_, recovery_method_,
                            debug_message_.c_str());
}

void DemoSetupController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDemoModeConfig,
      static_cast<int>(DemoSession::DemoModeConfig::kNone));
}

// static
void DemoSetupController::ClearDemoRequisition(
    policy::DeviceCloudPolicyManagerChromeOS* policy_manager) {
  if (policy_manager->GetDeviceRequisition() == kDemoRequisition) {
    policy_manager->SetDeviceRequisition(std::string());
    // If device requisition is |kDemoRequisition|, it means the sub
    // organization was also set by the demo setup controller, so remove it as
    // well.
    policy_manager->SetSubOrganization(std::string());
  }
}

// static
bool DemoSetupController::IsDemoModeAllowed() {
  // Demo mode is only allowed on devices that support ARC++.
  return arc::IsArcAvailable();
}

// static
bool DemoSetupController::IsOobeDemoSetupFlowInProgress() {
  const WizardController* const wizard_controller =
      WizardController::default_controller();
  return wizard_controller &&
         wizard_controller->demo_setup_controller() != nullptr;
}

// static
std::string DemoSetupController::GetSubOrganizationEmail() {
  const std::string country =
      g_browser_process->local_state()->GetString(prefs::kDemoModeCountry);
  const base::flat_set<std::string> kCountriesWithCustomization(
      {"de", "dk", "fi", "fr", "jp", "nl", "no", "se"});
  if (kCountriesWithCustomization.contains(country))
    return "admin-" + country + "@" + policy::kDemoModeDomain;
  return std::string();
}

DemoSetupController::DemoSetupController() {}

DemoSetupController::~DemoSetupController() {
  if (device_local_account_policy_store_)
    device_local_account_policy_store_->RemoveObserver(this);
}

bool DemoSetupController::IsOfflineEnrollment() const {
  return demo_config_ == DemoSession::DemoModeConfig::kOffline;
}

void DemoSetupController::Enroll(OnSetupSuccess on_setup_success,
                                 OnSetupError on_setup_error) {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone)
      << "Demo config needs to be explicitly set before calling Enroll()";
  DCHECK(!enrollment_helper_);

  on_setup_success_ = std::move(on_setup_success);
  on_setup_error_ = std::move(on_setup_error);

  VLOG(1) << "Starting demo setup "
          << DemoSession::DemoConfigToString(demo_config_);

  switch (demo_config_) {
    case DemoSession::DemoModeConfig::kOnline:
      LoadDemoResourcesCrOSComponent();
      return;
    case DemoSession::DemoModeConfig::kOffline: {
      EnrollOffline();
      return;
    }
    case DemoSession::DemoModeConfig::kNone:
      NOTREACHED() << "No valid demo mode config specified";
  }
}

void DemoSetupController::TryMountPreinstalledDemoResources(
    HasPreinstalledDemoResourcesCallback callback) {
  if (!preinstalled_demo_resources_) {
    preinstalled_demo_resources_ =
        std::make_unique<DemoResources>(DemoSession::DemoModeConfig::kOffline);
  }

  if (DBusThreadManager::Get()->IsUsingFakes()) {
    preinstalled_demo_resources_
        ->SetPreinstalledOfflineResourcesLoadedForTesting(
            preinstalled_offline_resources_path_for_tests_);
  }
  preinstalled_demo_resources_->EnsureLoaded(
      base::BindOnce(&DemoSetupController::OnPreinstalledDemoResourcesLoaded,
                     base::Unretained(this), std::move(callback)));
}

base::FilePath DemoSetupController::GetPreinstalledDemoResourcesPath(
    const base::FilePath& relative_path) {
  if (preinstalled_demo_resources_)
    return preinstalled_demo_resources_->GetAbsolutePath(relative_path);
  return base::FilePath();
}

void DemoSetupController::LoadDemoResourcesCrOSComponent() {
  VLOG(1) << "Loading demo resources component";
  if (!demo_resources_)
    demo_resources_ = std::make_unique<DemoResources>(demo_config_);

  if (DBusThreadManager::Get()->IsUsingFakes()) {
    demo_resources_->SetCrOSComponentLoadedForTesting(
        base::FilePath(), component_error_for_tests_);

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DemoSetupController::OnDemoResourcesCrOSComponentLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  demo_resources_->EnsureLoaded(
      base::BindOnce(&DemoSetupController::OnDemoResourcesCrOSComponentLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupController::OnDemoResourcesCrOSComponentLoaded() {
  DCHECK_EQ(demo_config_, DemoSession::DemoModeConfig::kOnline);

  if (demo_resources_->component_error().value() !=
      component_updater::CrOSComponentManager::Error::NONE) {
    SetupFailed(DemoSetupError::CreateFromComponentError(
        demo_resources_->component_error().value()));
    return;
  }

  VLOG(1) << "Starting online enrollment";
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  DCHECK(policy_manager->GetDeviceRequisition().empty());
  policy_manager->SetDeviceRequisition(kDemoRequisition);
  policy_manager->SetSubOrganization(GetSubOrganizationEmail());
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION;
  config.management_domain = policy::kDemoModeDomain;

  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, nullptr, config, policy::kDemoModeDomain);
  enrollment_helper_->EnrollUsingAttestation();
}

void DemoSetupController::OnPreinstalledDemoResourcesLoaded(
    HasPreinstalledDemoResourcesCallback callback) {
  std::move(callback).Run(!preinstalled_demo_resources_->path().empty());
}

void DemoSetupController::EnrollOffline() {
  DCHECK_EQ(demo_config_, DemoSession::DemoModeConfig::kOffline);
  DCHECK(!preinstalled_demo_resources_->path().empty());

  const base::FilePath policy_dir =
      preinstalled_demo_resources_->GetAbsolutePath(
          base::FilePath(kOfflinePolicyDirectoryName));

  if (IsOnlineFreCheckRequired()) {
    SetupFailed(
        DemoSetupError(DemoSetupError::ErrorCode::kOnlineFRECheckRequired,
                       DemoSetupError::RecoveryMethod::kOnlineOnly,
                       "Cannot do offline demo mode setup, because online "
                       "FRE check is required."));
    return;
  }

  VLOG(1) << "Starting offline enrollment";
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_OFFLINE_DEMO;
  config.management_domain = policy::kDemoModeDomain;
  config.offline_policy_path =
      policy_dir.AppendASCII(kOfflineDevicePolicyFileName);
  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, nullptr /* ad_join_delegate */, config, policy::kDemoModeDomain);
  enrollment_helper_->EnrollForOfflineDemo();
}

void DemoSetupController::OnAuthError(const GoogleServiceAuthError& error) {
  NOTREACHED();
}

void DemoSetupController::OnEnrollmentError(policy::EnrollmentStatus status) {
  SetupFailed(DemoSetupError::CreateFromEnrollmentStatus(status));
}

void DemoSetupController::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  SetupFailed(DemoSetupError::CreateFromOtherEnrollmentError(error));
}

void DemoSetupController::OnDeviceEnrolled() {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone);

  // Try to load the policy for the device local account.
  if (demo_config_ == DemoSession::DemoModeConfig::kOffline) {
    VLOG(1) << "Loading offline policy";
    DCHECK(!preinstalled_demo_resources_->path().empty());

    const base::FilePath file_path =
        preinstalled_demo_resources_->GetAbsolutePath(
            base::FilePath(kOfflinePolicyDirectoryName)
                .AppendASCII(kOfflineDeviceLocalAccountPolicyFileName));
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&ReadFileToOptionalString, file_path),
        base::BindOnce(&DemoSetupController::OnDeviceLocalAccountPolicyLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  VLOG(1) << "Marking device registered";
  StartupUtils::MarkDeviceRegistered(
      base::BindOnce(&DemoSetupController::OnDeviceRegistered,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupController::OnMultipleLicensesAvailable(
    const EnrollmentLicenseMap& licenses) {
  NOTREACHED();
}

void DemoSetupController::OnDeviceAttributeUploadCompleted(bool success) {
  NOTREACHED();
}

void DemoSetupController::OnDeviceAttributeUpdatePermission(bool granted) {
  NOTREACHED();
}

void DemoSetupController::OnRestoreAfterRollbackCompleted() {
  NOTREACHED();
}

void DemoSetupController::SetCrOSComponentLoadErrorForTest(
    component_updater::CrOSComponentManager::Error error) {
  component_error_for_tests_ = error;
}

void DemoSetupController::SetPreinstalledOfflineResourcesPathForTesting(
    const base::FilePath& path) {
  preinstalled_offline_resources_path_for_tests_ = path;
}

void DemoSetupController::SetDeviceLocalAccountPolicyStoreForTest(
    policy::CloudPolicyStore* store) {
  device_local_account_policy_store_ = store;
}

void DemoSetupController::OnDeviceLocalAccountPolicyLoaded(
    base::Optional<std::string> blob) {
  if (!blob.has_value()) {
    // This is very unlikely to happen since the file existence is already
    // checked as CheckOfflinePolicyFilesExist.
    SetupFailed(
        DemoSetupError(DemoSetupError::ErrorCode::kOfflinePolicyError,
                       DemoSetupError::RecoveryMethod::kPowerwash,
                       "Policy file for the device local account not found"));
    return;
  }

  enterprise_management::PolicyFetchResponse policy;
  if (!policy.ParseFromString(blob.value())) {
    SetupFailed(DemoSetupError(DemoSetupError::ErrorCode::kOfflinePolicyError,
                               DemoSetupError::RecoveryMethod::kPowerwash,
                               "Error parsing local account policy blob."));
    return;
  }

  // Extract the account_id from the policy data.
  enterprise_management::PolicyData policy_data;
  if (policy.policy_data().empty() ||
      !policy_data.ParseFromString(policy.policy_data())) {
    SetupFailed(DemoSetupError(DemoSetupError::ErrorCode::kOfflinePolicyError,
                               DemoSetupError::RecoveryMethod::kPowerwash,
                               "Error parsing local account policy data."));
    return;
  }

  VLOG(1) << "Storing offline policy";
  // On the unittest, the device_local_account_policy_store_ is already
  // initialized. Otherwise attempts to get the store.
  if (!device_local_account_policy_store_) {
    device_local_account_policy_store_ =
        GetDeviceLocalAccountPolicyStore(policy_data.username());
  }

  if (!device_local_account_policy_store_) {
    SetupFailed(
        DemoSetupError(DemoSetupError::ErrorCode::kOfflinePolicyStoreError,
                       DemoSetupError::RecoveryMethod::kPowerwash,
                       "Can't find the store for the local account policy."));
    return;
  }
  device_local_account_policy_store_->AddObserver(this);
  device_local_account_policy_store_->Store(policy);
}

void DemoSetupController::OnDeviceRegistered() {
  VLOG(1) << "Demo mode setup finished successfully.";
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kDemoModeConfig, static_cast<int>(demo_config_));
  prefs->CommitPendingWrite();
  Reset();
  if (!on_setup_success_.is_null())
    std::move(on_setup_success_).Run();
}

void DemoSetupController::SetupFailed(const DemoSetupError& error) {
  Reset();
  LOG(ERROR) << error.GetDebugDescription();
  if (!on_setup_error_.is_null())
    std::move(on_setup_error_).Run(error);
}

void DemoSetupController::Reset() {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone);

  // |demo_config_| is not reset here, because it is needed for retrying setup.
  enrollment_helper_.reset();
  if (device_local_account_policy_store_) {
    device_local_account_policy_store_->RemoveObserver(this);
    device_local_account_policy_store_ = nullptr;
  }
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  ClearDemoRequisition(policy_manager);
}

void DemoSetupController::OnStoreLoaded(policy::CloudPolicyStore* store) {
  DCHECK_EQ(store, device_local_account_policy_store_);
  VLOG(1) << "Marking device registered";
  StartupUtils::MarkDeviceRegistered(
      base::BindOnce(&DemoSetupController::OnDeviceRegistered,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupController::OnStoreError(policy::CloudPolicyStore* store) {
  DCHECK_EQ(store, device_local_account_policy_store_);
  SetupFailed(
      DemoSetupError(DemoSetupError::ErrorCode::kOfflinePolicyStoreError,
                     DemoSetupError::RecoveryMethod::kPowerwash,
                     "Failed to store the local account policy"));
}

}  //  namespace chromeos
