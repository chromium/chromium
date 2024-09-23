// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"

#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/demo_mode/utils/dimensions_utils.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using ErrorCode = DemoSetupController::DemoSetupError::ErrorCode;
using RecoveryMethod = DemoSetupController::DemoSetupError::RecoveryMethod;

constexpr char kDemoSetupDownloadDurationHistogram[] =
    "DemoMode.Setup.DownloadDuration";
constexpr char kDemoSetupEnrollDurationHistogram[] =
    "DemoMode.Setup.EnrollDuration";
constexpr char kDemoSetupLoadingDurationHistogram[] =
    "DemoMode.Setup.LoadingDuration";
constexpr char kDemoSetupNumRetriesHistogram[] = "DemoMode.Setup.NumRetries";
constexpr char kDemoSetupComponentInitialLoadingResultHistogram[] =
    "DemoMode.Setup.ComponentInitialLoadingResult";
constexpr char kDemoSetupComponentLoadingRetryResultHistogram[] =
    "DemoMode.Setup.ComponentLoadingRetryResult";
constexpr char kDemoSetupErrorHistogram[] = "DemoMode.Setup.Error";

struct DemoSetupStepInfo {
  DemoSetupController::DemoSetupStep step;
  const int step_index;
};

base::span<const DemoSetupStepInfo> GetDemoSetupStepsInfo() {
  static const DemoSetupStepInfo kDemoModeSetupStepsInfo[] = {
      {DemoSetupController::DemoSetupStep::kDownloadResources, 0},
      {DemoSetupController::DemoSetupStep::kEnrollment, 1},
      {DemoSetupController::DemoSetupStep::kComplete, 2}};
  return kDemoModeSetupStepsInfo;
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
    case policy::DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
    case policy::DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
    case policy::DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
    case policy::DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK:
      return DemoSetupController::DemoSetupError(ErrorCode::kDemoAccountError,
                                                 RecoveryMethod::kUnknown,
                                                 debug_message);
    case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
    case policy::DM_STATUS_SERVICE_DEVICE_NEEDS_RESET:
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
    case policy::DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
      return DemoSetupController::DemoSetupError(
          ErrorCode::kTooManyRequestsError, RecoveryMethod::kRetry,
          debug_message);
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
  NOTREACHED_IN_MIGRATION()
      << "Demo mode setup received unsupported client status";
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
  NOTREACHED_IN_MIGRATION()
      << "Demo mode setup received unsupported lock status";
  return DemoSetupController::DemoSetupError(
      ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown, debug_message);
}

}  //  namespace

// static
DemoSetupController::DemoSetupError
DemoSetupController::DemoSetupError::CreateFromEnrollmentStatus(
    const policy::EnrollmentStatus& status) {
  const std::string debug_message = base::StringPrintf(
      "EnrollmentError: (code: %d, client_status: %d, store_status: %d, "
      "validation_status: %d, lock_status: %d)",
      static_cast<int>(status.enrollment_code()), status.client_status(),
      status.store_status(), status.validation_status(), status.lock_status());

  switch (status.enrollment_code()) {
    case policy::EnrollmentStatus::Code::kSuccess:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::Code::kNoStateKeys:
      return DemoSetupError(ErrorCode::kNoStateKeys, RecoveryMethod::kReboot,
                            debug_message);
    case policy::EnrollmentStatus::Code::kRegistrationFailed:
      return CreateFromClientStatus(status.client_status(), debug_message);
    case policy::EnrollmentStatus::Code::kRobotAuthFetchFailed:
    case policy::EnrollmentStatus::Code::kRobotRefreshFetchFailed:
      return DemoSetupError(ErrorCode::kRobotFetchError,
                            RecoveryMethod::kCheckNetwork, debug_message);
    case policy::EnrollmentStatus::Code::kRobotRefreshStoreFailed:
      return DemoSetupError(ErrorCode::kRobotStoreError,
                            RecoveryMethod::kReboot, debug_message);
    case policy::EnrollmentStatus::Code::kRegistrationBadMode:
      return DemoSetupError(ErrorCode::kBadMode, RecoveryMethod::kRetry,
                            debug_message);
    case policy::EnrollmentStatus::Code::kRegistrationCertFetchFailed:
      return DemoSetupError(ErrorCode::kCertFetchError, RecoveryMethod::kRetry,
                            debug_message);
    case policy::EnrollmentStatus::Code::kPolicyFetchFailed:
      return DemoSetupError(ErrorCode::kPolicyFetchError,
                            RecoveryMethod::kRetry, debug_message);
    case policy::EnrollmentStatus::Code::kValidationFailed:
      return DemoSetupError(ErrorCode::kPolicyValidationError,
                            RecoveryMethod::kRetry, debug_message);
    case policy::EnrollmentStatus::Code::kLockError:
      return CreateFromLockStatus(status.lock_status(), debug_message);
    case policy::EnrollmentStatus::Code::kStoreError:
      return DemoSetupError(ErrorCode::kOnlineStoreError,
                            RecoveryMethod::kRetry, debug_message);
    case policy::EnrollmentStatus::Code::kAttributeUpdateFailed:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::Code::kNoMachineIdentification:
      return DemoSetupError(ErrorCode::kMachineIdentificationError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::Code::kDmTokenStoreFailed:
      return DemoSetupError(ErrorCode::kDMTokenStoreError,
                            RecoveryMethod::kUnknown, debug_message);
    case policy::EnrollmentStatus::Code::kMayNotBlockDevMode:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
  }
  NOTREACHED_IN_MIGRATION()
      << "Demo mode setup received unsupported enrollment status";
  return DemoSetupError(ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown,
                        debug_message);
}

// static
DemoSetupController::DemoSetupError
DemoSetupController::DemoSetupError::CreateFromOtherEnrollmentError(
    EnrollmentLauncher::OtherError error) {
  const std::string debug_message =
      base::StringPrintf("Other error: %d", error);
  switch (error) {
    case EnrollmentLauncher::OTHER_ERROR_DOMAIN_MISMATCH:
      return DemoSetupError(ErrorCode::kAlreadyLocked,
                            RecoveryMethod::kPowerwash, debug_message);
    case EnrollmentLauncher::OTHER_ERROR_FATAL:
      return DemoSetupError(ErrorCode::kUnexpectedError,
                            RecoveryMethod::kUnknown, debug_message);
  }
  NOTREACHED_IN_MIGRATION()
      << "Demo mode setup received unsupported enrollment error";
  return DemoSetupError(ErrorCode::kUnexpectedError, RecoveryMethod::kUnknown,
                        debug_message);
}

// static
DemoSetupController::DemoSetupError
DemoSetupController::DemoSetupError::CreateFromComponentError(
    component_updater::ComponentManagerAsh::Error error,
    std::string component_name) {
  const std::string debug_message =
      base::StringPrintf("Failed to load '%s' CrOS component with error: %d",
                         component_name.c_str(), static_cast<int>(error));
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

std::u16string DemoSetupController::DemoSetupError::GetLocalizedErrorMessage()
    const {
  switch (error_code_) {
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
    case ErrorCode::kTooManyRequestsError:
      return l10n_util::GetStringUTF16(IDS_DEMO_SETUP_TOO_MANY_REQUESTS_ERROR);
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
    case ErrorCode::kSuccess:
      // We don't display the success message. It's for recording the UMA
      // metrics only.
      return std::u16string();
  }
  NOTREACHED_IN_MIGRATION()
      << "No localized error message available for demo setup error.";
  return std::u16string();
}

std::u16string
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
  NOTREACHED_IN_MIGRATION()
      << "No localized error message available for demo setup recovery method.";
  return std::u16string();
}

std::string DemoSetupController::DemoSetupError::GetDebugDescription() const {
  return base::StringPrintf("DemoSetupError (code: %d, recovery: %d) : %s",
                            static_cast<int>(error_code_),
                            static_cast<int>(recovery_method_),
                            debug_message_.c_str());
}

void DemoSetupController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDemoModeConfig,
      static_cast<int>(DemoSession::DemoModeConfig::kNone));
}

// static
void DemoSetupController::ClearDemoRequisition() {
  if (policy::EnrollmentRequisitionManager::GetDeviceRequisition() ==
      policy::EnrollmentRequisitionManager::kDemoRequisition) {
    policy::EnrollmentRequisitionManager::SetDeviceRequisition(std::string());
    // If device requisition is `kDemoRequisition`, it means the sub
    // organization was also set by the demo setup controller, so remove it as
    // well.
    policy::EnrollmentRequisitionManager::SetSubOrganization(std::string());
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
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);

  if (command_line->HasSwitch(switches::kDemoModeEnrollingUsername)) {
    std::string customUser =
        command_line->GetSwitchValueASCII(switches::kDemoModeEnrollingUsername);
    if (!customUser.empty()) {
      std::string email = customUser + "@" + policy::kDemoModeDomain;
      VLOG(1) << "Enrolling into Demo Mode with user: " << email;
      return email;
    }
  }

  const std::string country =
      g_browser_process->local_state()->GetString(prefs::kDemoModeCountry);

  std::string country_uppercase = base::ToUpperASCII(country);
  std::string country_lowercase = base::ToLowerASCII(country);

  // Exclude US as it is the default country.
  if (base::Contains(DemoSession::kSupportedCountries, country_uppercase)) {
    if (chromeos::features::IsCloudGamingDeviceEnabled()) {
      return base::StringPrintf("admin-%s-blazey@%s", country_lowercase.c_str(),
                                policy::kDemoModeDomain);
    }

    return "admin-" + country_lowercase + "@" + policy::kDemoModeDomain;
  }
  return std::string();
}

// static
base::Value::Dict DemoSetupController::GetDemoSetupSteps() {
  base::Value::Dict setup_steps_dict;
  for (auto entry : GetDemoSetupStepsInfo()) {
    setup_steps_dict.Set(GetDemoSetupStepString(entry.step), entry.step_index);
  }

  return setup_steps_dict;
}

// static
std::string DemoSetupController::GetDemoSetupStepString(
    const DemoSetupStep step_enum) {
  switch (step_enum) {
    case DemoSetupStep::kDownloadResources:
      return "downloadResources";
    case DemoSetupStep::kEnrollment:
      return "enrollment";
    case DemoSetupStep::kComplete:
      return "complete";
  }

  NOTREACHED_IN_MIGRATION();
}

DemoSetupController::DemoSetupController() = default;

DemoSetupController::~DemoSetupController() = default;

void DemoSetupController::SetAndCanonicalizeRetailerName(
    const std::string& retailer_name) {
  retailer_name_ = ash::demo_mode::CanonicalizeDimension(retailer_name);
}

void DemoSetupController::Enroll(
    OnSetupSuccess on_setup_success,
    OnSetupError on_setup_error,
    const OnSetCurrentSetupStep& set_current_setup_step) {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone)
      << "Demo config needs to be explicitly set before calling Enroll()";
  DCHECK(!enrollment_launcher_);

  set_current_setup_step_ = set_current_setup_step;
  on_setup_success_ = std::move(on_setup_success);
  on_setup_error_ = std::move(on_setup_error);

  VLOG(1) << "Starting demo setup "
          << DemoSession::DemoConfigToString(demo_config_);

  SetCurrentSetupStep(DemoSetupStep::kDownloadResources);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kDemoModeRetailerId, retailer_name_);
  prefs->SetString(prefs::kDemoModeStoreId, store_number_);

  switch (demo_config_) {
    case DemoSession::DemoModeConfig::kOnline:
      LoadDemoComponents();
      return;
    case DemoSession::DemoModeConfig::kNone:
    case DemoSession::DemoModeConfig::kOfflineDeprecated:
      NOTREACHED_IN_MIGRATION() << "No valid demo mode config specified";
  }
}

void DemoSetupController::LoadDemoComponents() {
  VLOG(1) << "Loading demo resources and demo app components";

  download_start_time_ = base::TimeTicks::Now();

  if (!demo_components_)
    demo_components_ = std::make_unique<DemoComponents>(demo_config_);

  // Simulate loading demo components completed for unit tests.
  if (DBusThreadManager::Get()->IsUsingFakes() &&
      !load_real_components_for_test_) {
    demo_components_->SetCrOSComponentLoadedForTesting(
        base::FilePath(), component_error_for_tests_);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DemoSetupController::OnDemoComponentsLoaded,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  auto is_growth_campaigns_enabled_in_demo_mode =
      features::IsGrowthCampaignsInDemoModeEnabled();
  base::OnceClosure load_callback =
      base::BindOnce(&DemoSetupController::OnDemoComponentsLoaded,
                     weak_ptr_factory_.GetWeakPtr());
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(is_growth_campaigns_enabled_in_demo_mode ? 3 : 2,
                           std::move(load_callback));
  demo_components_->LoadResourcesComponent(barrier_closure);
  demo_components_->LoadAppComponent(barrier_closure);
  if (is_growth_campaigns_enabled_in_demo_mode) {
    // Growth campaign is enabled in demo mode, also load growth campaigns
    // component.
    growth::CampaignsManager::Get()->LoadCampaigns(barrier_closure,
                                                   /*in_oobe=*/true);
  }
}

void DemoSetupController::OnDemoComponentsLoaded() {
  DCHECK_EQ(demo_config_, DemoSession::DemoModeConfig::kOnline);

  base::TimeDelta download_duration =
      base::TimeTicks::Now() - download_start_time_;
  base::UmaHistogramLongTimes100(kDemoSetupDownloadDurationHistogram,
                                 download_duration);
  SetCurrentSetupStep(DemoSetupStep::kEnrollment);

  auto resources_component_error =
      demo_components_->resources_component_error().value_or(
          component_updater::ComponentManagerAsh::Error::NOT_FOUND);
  auto app_component_error = demo_components_->app_component_error().value_or(
      component_updater::ComponentManagerAsh::Error::NOT_FOUND);
  // We determine it's an initial loading or retry based on the
  // `num_setup_retries_` count.
  const std::string kDemoSetupComponentlLoadingResultHistogram =
      num_setup_retries_ == 0 ? kDemoSetupComponentInitialLoadingResultHistogram
                              : kDemoSetupComponentLoadingRetryResultHistogram;

  if (resources_component_error !=
      component_updater::ComponentManagerAsh::Error::NONE) {
    // Reporting the corresponding enum based on the app component error.
    base::UmaHistogramEnumeration(
        kDemoSetupComponentlLoadingResultHistogram,
        app_component_error ==
                component_updater::ComponentManagerAsh::Error::NONE
            ? DemoSetupComponentLoadingResult::kAppSuccessResourcesFailure
            : DemoSetupComponentLoadingResult::kAppFailureResourcesFailure);

    SetupFailed(DemoSetupError::CreateFromComponentError(
        resources_component_error,
        DemoComponents::kDemoModeResourcesComponentName));
    return;
  }

  if (app_component_error !=
      component_updater::ComponentManagerAsh::Error::NONE) {
    // There should be no error on the resources component loading if we've got
    // to this point. It should've been handled in the previous "if block".
    DCHECK(resources_component_error ==
           component_updater::ComponentManagerAsh::Error::NONE);
    base::UmaHistogramEnumeration(
        kDemoSetupComponentlLoadingResultHistogram,
        DemoSetupComponentLoadingResult::kAppFailureResourcesSuccess);

    SetupFailed(DemoSetupError::CreateFromComponentError(
        app_component_error, DemoComponents::kDemoModeAppComponentName));
    return;
  }

  // There should be no error on both the app and the resources components
  // loading if we've got to this point. It should've been handled in the
  // previous two "if blocks".
  base::UmaHistogramEnumeration(
      kDemoSetupComponentlLoadingResultHistogram,
      DemoSetupComponentLoadingResult::kAppSuccessResourcesSuccess);

  VLOG(1) << "Starting online enrollment";

  enroll_start_time_ = base::TimeTicks::Now();

  DCHECK(policy::EnrollmentRequisitionManager::GetDeviceRequisition().empty());
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      policy::EnrollmentRequisitionManager::kDemoRequisition);
  policy::EnrollmentRequisitionManager::SetSubOrganization(
      GetSubOrganizationEmail());
  policy::EnrollmentConfig config =
      policy::EnrollmentConfig::GetDemoModeEnrollmentConfig();

  enrollment_launcher_ =
      EnrollmentLauncher::Create(this, config, policy::kDemoModeDomain);
  enrollment_launcher_->EnrollUsingAttestation();
}

void DemoSetupController::OnAuthError(const GoogleServiceAuthError& error) {
  NOTREACHED_IN_MIGRATION();
}

void DemoSetupController::OnEnrollmentError(policy::EnrollmentStatus status) {
  SetupFailed(DemoSetupError::CreateFromEnrollmentStatus(status));
}

void DemoSetupController::OnOtherError(EnrollmentLauncher::OtherError error) {
  SetupFailed(DemoSetupError::CreateFromOtherEnrollmentError(error));
}

void DemoSetupController::OnDeviceEnrolled() {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone);

  // `enroll_start_time_` is only set for online enrollment.
  if (!enroll_start_time_.is_null()) {
    base::TimeDelta enroll_duration =
        base::TimeTicks::Now() - enroll_start_time_;
    base::UmaHistogramLongTimes100(kDemoSetupEnrollDurationHistogram,
                                   enroll_duration);
  }
  UMA_HISTOGRAM_ENUMERATION(kDemoSetupErrorHistogram, ErrorCode::kSuccess);
  VLOG(1) << "Marking device registered";
  StartupUtils::MarkDeviceRegistered(
      base::BindOnce(&DemoSetupController::OnDeviceRegistered,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupController::OnDeviceAttributeUploadCompleted(bool success) {
  NOTREACHED_IN_MIGRATION();
}

void DemoSetupController::OnDeviceAttributeUpdatePermission(bool granted) {
  NOTREACHED_IN_MIGRATION();
}

void DemoSetupController::SetCrOSComponentLoadErrorForTest(
    component_updater::ComponentManagerAsh::Error error) {
  component_error_for_tests_ = error;
}

void DemoSetupController::EnableLoadRealComponentsForTest() {
  load_real_components_for_test_ = true;
}

void DemoSetupController::OnDeviceRegistered() {
  VLOG(1) << "Demo mode setup finished successfully.";

  if (demo_config_ == DemoSession::DemoModeConfig::kOnline) {
    base::TimeDelta loading_duration =
        base::TimeTicks::Now() - download_start_time_;
    // A similar metric can be found at OOBE.StepCompletionTime.Demo-setup,
    // however this is only useful for durations up to three minutes. Demo mode
    // setup typically takes longer than this, so we use a LONG_TIMES metric
    // here to capture metrics of up to one hour.
    base::UmaHistogramLongTimes100(kDemoSetupLoadingDurationHistogram,
                                   loading_duration);
  }

  base::UmaHistogramCounts100(kDemoSetupNumRetriesHistogram,
                              num_setup_retries_);

  SetCurrentSetupStep(DemoSetupStep::kComplete);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kDemoModeConfig, static_cast<int>(demo_config_));
  prefs->CommitPendingWrite();
  Reset();
  if (!on_setup_success_.is_null())
    std::move(on_setup_success_).Run();
}

void DemoSetupController::SetCurrentSetupStep(DemoSetupStep current_step) {
  if (!set_current_setup_step_.is_null())
    set_current_setup_step_.Run(current_step);
}

void DemoSetupController::SetupFailed(const DemoSetupError& error) {
  num_setup_retries_++;
  Reset();
  LOG(ERROR) << error.GetDebugDescription();
  if (!on_setup_error_.is_null())
    std::move(on_setup_error_).Run(error);
  UMA_HISTOGRAM_ENUMERATION(kDemoSetupErrorHistogram, error.error_code());
}

void DemoSetupController::Reset() {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone);

  // `demo_config_` is not reset here, because it is needed for retrying setup.
  enrollment_launcher_.reset();
  ClearDemoRequisition();
}

}  //  namespace ash
