// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/login/active_directory_migration_utils.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/install_attributes_util.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

using ::policy::AccountStatusCheckFetcher;
using ::policy::EnrollmentConfig;

// Do not change the UMA histogram parameters without renaming the histograms!
#define UMA_ENROLLMENT_TIME(histogram_name, elapsed_timer)                   \
  do {                                                                       \
    UMA_HISTOGRAM_CUSTOM_TIMES((histogram_name), (elapsed_timer)->Elapsed(), \
                               base::Milliseconds(100) /* min */,            \
                               base::Minutes(15) /* max */,                  \
                               100 /* bucket_count */);                      \
  } while (0)

const char* const kMetricEnrollmentTimeCancel =
    "Enterprise.EnrollmentTime.Cancel";
const char* const kMetricEnrollmentTimeFailure =
    "Enterprise.EnrollmentTime.Failure";
const char* const kMetricEnrollmentTimeSuccess =
    "Enterprise.EnrollmentTime.Success";

// Retry policy constants.
constexpr int kInitialDelayMS = 4 * 1000;  // 4 seconds
constexpr double kMultiplyFactor = 1.5;
constexpr double kJitterFactor = 0.1;           // +/- 10% jitter
constexpr int64_t kMaxDelayMS = 8 * 60 * 1000;  // 8 minutes

constexpr char kUserActionCancelTPMCheck[] = "cancel-tpm-check";
constexpr char kUserActionSkipDialogConfirmation[] = "skip-confirmation";

// Max number of retries to check install attributes state.
constexpr int kMaxInstallAttributesStateCheckRetries = 60;

bool ShouldAttemptRestart() {
  // Restart browser to switch from DeviceCloudPolicyManagerAsh to
  // DeviceActiveDirectoryPolicyManager.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsActiveDirectoryManaged()) {
    // TODO(tnagel): Refactor BrowserPolicyConnectorAsh so that device
    // policy providers are only registered after enrollment has finished and
    // thus the correct one can be picked without restarting the browser.
    return true;
  }

  return false;
}

// Returns the manager of the domain (either the domain name or the email of the
// admin of the domain) after enrollment, or an empty string.
std::string GetEnterpriseDomainManager() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetEnterpriseDomainManager();
}

bool IsOnEnrollmentScreen() {
  return LoginDisplayHost::default_host()->GetOobeUI()->current_screen() ==
         EnrollmentScreenView::kScreenId;
}

bool TestForcesManualEnrollment() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kEnterpriseForceManualEnrollmentInTestBuilds)) {
    // Forcing manual enrollment is only used in integration/manual tests. Crash
    // if we're not on a test image.
    base::SysInfo::CrashIfChromeOSNonTestImage();
    return true;
  }
  return false;
}

}  // namespace

// static
// The return value of this function is recorded as histogram. If you change
// it, make sure to change all relevant histogram suffixes accordingly.
std::string EnrollmentScreen::GetResultString(Result result) {
  switch (result) {
    case Result::COMPLETED:
      return "Completed";
    case Result::BACK:
      return "Back";
    case Result::SKIPPED_FOR_TESTS:
      return BaseScreen::kNotApplicable;
    case Result::TPM_ERROR:
      return "TpmError";
    case Result::TPM_DBUS_ERROR:
      return "TpmDbusError";
    case Result::BACK_TO_AUTO_ENROLLMENT_CHECK:
      return "BackToAutoEnrollmentCheck";
  }
}

// static
EnrollmentScreen* EnrollmentScreen::Get(ScreenManager* manager) {
  return static_cast<EnrollmentScreen*>(
      manager->GetScreen(EnrollmentScreenView::kScreenId));
}

EnrollmentScreen::EnrollmentScreen(base::WeakPtr<EnrollmentScreenView> view,
                                   ErrorScreen* error_screen,
                                   const ScreenExitCallback& exit_callback)
    : BaseScreen(EnrollmentScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(
          ErrorScreensHistogramHelper::ErrorParentScreen::kEnrollment) {
  retry_policy_.num_errors_to_ignore = 0;
  retry_policy_.initial_delay_ms = kInitialDelayMS;
  retry_policy_.multiply_factor = kMultiplyFactor;
  retry_policy_.jitter_factor = kJitterFactor;
  retry_policy_.maximum_backoff_ms = kMaxDelayMS;
  retry_policy_.entry_lifetime_ms = -1;
  retry_policy_.always_use_initial_delay = true;
  retry_backoff_ = std::make_unique<net::BackoffEntry>(&retry_policy_);

  ad_migration_utils::CheckChromadMigrationOobeFlow(
      base::BindOnce(&EnrollmentScreen::UpdateChromadMigrationOobeFlow,
                     weak_ptr_factory_.GetWeakPtr()));

  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
}

EnrollmentScreen::~EnrollmentScreen() {
  scoped_network_observation_.Reset();
  DCHECK(!enrollment_helper_ || g_browser_process->IsShuttingDown() ||
         browser_shutdown::IsTryingToQuit() ||
         DBusThreadManager::Get()->IsUsingFakes());
}

void EnrollmentScreen::SetEnrollmentConfig(
    const policy::EnrollmentConfig& enrollment_config) {
  enrollment_config_ = enrollment_config;
  switch (enrollment_config_.auth_mechanism) {
    case EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE:
      current_auth_ = AUTH_OAUTH;
      next_auth_ = AUTH_OAUTH;
      break;
    case EnrollmentConfig::AUTH_MECHANISM_ATTESTATION:
      current_auth_ = AUTH_ATTESTATION;
      next_auth_ = AUTH_ATTESTATION;
      break;
    case EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE:
      if (TestForcesManualEnrollment()) {
        current_auth_ = AUTH_OAUTH;
        next_auth_ = AUTH_OAUTH;
        break;
      }
      current_auth_ = AUTH_ATTESTATION;
      next_auth_ = enrollment_config_.should_enroll_interactively()
                       ? AUTH_OAUTH
                       : AUTH_ATTESTATION;
      break;
    default:
      NOTREACHED();
      break;
  }
  SetConfig();
}

void EnrollmentScreen::SetConfig() {
  config_ = enrollment_config_;
  if (current_auth_ == AUTH_OAUTH &&
      config_.is_mode_attestation_with_manual_fallback()) {
    config_.mode =
        policy::EnrollmentConfig::GetManualFallbackMode(config_.mode);
  } else if (current_auth_ == AUTH_ATTESTATION &&
             !enrollment_config_.is_mode_attestation()) {
    config_.mode = config_.is_attestation_auth_forced()
                       ? policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED
                       : policy::EnrollmentConfig::MODE_ATTESTATION;
  }
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "EnrollmentScreen::SetConfig()"
               << " config_.mode = " << static_cast<int>(config_.mode)
               << ", config_.auth_mechanism = "
               << static_cast<int>(config_.auth_mechanism);
  if (view_)
    view_->SetEnrollmentConfig(config_);
  enrollment_helper_ = nullptr;
}

bool EnrollmentScreen::AdvanceToNextAuth() {
  if (current_auth_ != next_auth_ && current_auth_ == AUTH_ATTESTATION) {
    LOG(WARNING) << "User stopped using auth: " << current_auth_
                 << ", current auth: " << next_auth_ << ".";
    current_auth_ = next_auth_;
    SetConfig();
    return true;
  }
  return false;
}

void EnrollmentScreen::CreateEnrollmentHelper() {
  if (!enrollment_helper_) {
    enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
        this, this, config_, enrolling_user_domain_, license_type_to_use_);
  }
}

void EnrollmentScreen::ClearAuth(base::OnceClosure callback) {
  if (switches::IsTpmDynamic()) {
    wait_state_timer_.Stop();
    install_state_retries_ = 0;
  }
  if (!enrollment_helper_) {
    std::move(callback).Run();
    return;
  }
  enrollment_helper_->ClearAuth(base::BindOnce(&EnrollmentScreen::OnAuthCleared,
                                               weak_ptr_factory_.GetWeakPtr(),
                                               std::move(callback)));
}

void EnrollmentScreen::OnAuthCleared(base::OnceClosure callback) {
  enrollment_helper_ = nullptr;
  std::move(callback).Run();
}

void EnrollmentScreen::ShowSkipEnrollmentDialogue() {
  DCHECK(config_.is_license_packaged_with_device);
  if (view_) {
    view_->ShowSkipConfirmationDialog();
  }
}

bool EnrollmentScreen::MaybeSkip(WizardContext& context) {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "EnrollmentScreen::MaybeSkip("
               << "config_.is_forced = " << config_.is_forced()
               << ", skip_to_login_for_tests = "
               << context.skip_to_login_for_tests << ").";
  if (context.skip_to_login_for_tests && !config_.is_forced()) {
    exit_callback_.Run(Result::SKIPPED_FOR_TESTS);
    return true;
  }
  return false;
}

void EnrollmentScreen::UpdateFlowType() {
  if (!view_)
    return;
  if (features::IsLicensePackagedOobeFlowEnabled() &&
      config_.license_type == policy::LicenseType::kEnterprise &&
      config_.is_license_packaged_with_device) {
    view_->SetFlowType(EnrollmentScreenView::FlowType::kEnterpriseLicense);
    view_->SetGaiaButtonsType(EnrollmentScreenView::GaiaButtonsType::kDefault);
    return;
  }
  if (features::IsEducationEnrollmentOobeFlowEnabled() &&
      config_.license_type == policy::LicenseType::kEducation &&
      config_.is_license_packaged_with_device) {
    view_->SetFlowType(EnrollmentScreenView::FlowType::kEducationLicense);
    view_->SetGaiaButtonsType(EnrollmentScreenView::GaiaButtonsType::kDefault);
    return;
  }

  const bool cfm = policy::EnrollmentRequisitionManager::IsRemoraRequisition();
  if (cfm) {
    view_->SetFlowType(EnrollmentScreenView::FlowType::kCFM);
    view_->SetGaiaButtonsType(EnrollmentScreenView::GaiaButtonsType::kDefault);
  } else {
    view_->SetFlowType(EnrollmentScreenView::FlowType::kEnterprise);
    if (!features::IsKioskEnrollmentInOobeEnabled()) {
      view_->SetGaiaButtonsType(
          EnrollmentScreenView::GaiaButtonsType::kDefault);
      return;
    }
    if (context()->enrollment_preference_ ==
        WizardContext::EnrollmentPreference::kKiosk) {
      view_->SetGaiaButtonsType(
          EnrollmentScreenView::GaiaButtonsType::kKioskPreffered);
    } else {
      view_->SetGaiaButtonsType(
          EnrollmentScreenView::GaiaButtonsType::kEnterprisePreffered);
    }
  }
}

void EnrollmentScreen::ShowImpl() {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Show enrollment screen";
  histogram_helper_.OnScreenShow();
  if (!scoped_network_observation_.IsObserving()) {
    scoped_network_observation_.Observe(network_state_informer_.get());
  }
  is_rollback_flow_ = IsRollbackFlow(*context());
  if (view_)
    view_->SetEnrollmentController(this);
  // Block enrollment on liveboot (OS isn't installed yet and this is trial
  // flow).
  if (switches::IsOsInstallAllowed()) {
    if (view_)
      view_->ShowEnrollmentDuringTrialNotAllowedError();
    return;
  }
  // If TPM can be dynamically configured: show spinner and try taking
  // ownership.
  if (!tpm_checked_ && switches::IsTpmDynamic()) {
    if (view_)
      view_->ShowEnrollmentTPMCheckingScreen();
    TakeTpmOwnership();
    return;
  }

  UMA(policy::kMetricEnrollmentTriggered);
  UpdateFlowType();
  switch (current_auth_) {
    case AUTH_OAUTH:
      ShowInteractiveScreen();
      break;
    case AUTH_ATTESTATION:
      AuthenticateUsingAttestation();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void EnrollmentScreen::TakeTpmOwnership() {
  // This is used in browsertest to test cancel button.
  if (tpm_ownership_callback_for_testing_.has_value()) {
    chromeos::TpmManagerClient::Get()->TakeOwnership(
        ::tpm_manager::TakeOwnershipRequest(),
        std::move(tpm_ownership_callback_for_testing_.value()));
    return;
  }
  chromeos::TpmManagerClient::Get()->TakeOwnership(
      ::tpm_manager::TakeOwnershipRequest(),
      base::BindOnce(&EnrollmentScreen::OnTpmStatusResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::OnTpmStatusResponse(
    const ::tpm_manager::TakeOwnershipReply& reply) {
  if (is_hidden() || tpm_checked_)
    return;
  if (reply.status() == ::tpm_manager::STATUS_SUCCESS) {
    CheckInstallAttributesState();
    return;
  }
  tpm_checked_ = true;

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "OnTpmStatusResponse: status=" << reply.status();
  switch (reply.status()) {
    case ::tpm_manager::STATUS_NOT_AVAILABLE:
      ShowImpl();
      break;
    case ::tpm_manager::STATUS_DEVICE_ERROR:
      ClearAuth(base::BindOnce(exit_callback_, Result::TPM_ERROR));
      break;
    case ::tpm_manager::STATUS_DBUS_ERROR:
      ClearAuth(base::BindOnce(exit_callback_, Result::TPM_DBUS_ERROR));
      break;
    default:
      NOTREACHED();
  }
}

void EnrollmentScreen::CheckInstallAttributesState() {
  if (install_state_retries_++ >= kMaxInstallAttributesStateCheckRetries) {
    tpm_checked_ = true;
    ClearAuth(base::BindOnce(exit_callback_, Result::TPM_DBUS_ERROR));
    return;
  }
  user_data_auth::InstallAttributesState state =
      install_attributes_util::InstallAttributesGetStatus();

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "InstallAttributesState: state = " << static_cast<int>(state);
  if (state == user_data_auth::InstallAttributesState::TPM_NOT_OWNED) {
    // There may be some processes running in the background, we need to try
    // again and set a reasonable timeout here to show an error if nothing
    // changes.
    wait_state_timer_.Start(FROM_HERE, base::Seconds(1), this,
                            &EnrollmentScreen::CheckInstallAttributesState);
    return;
  }
  tpm_checked_ = true;
  switch (state) {
    case user_data_auth::InstallAttributesState::UNKNOWN:
      // This means that some interprocess communication error may occur and we
      // suggest a reboot.
      ClearAuth(base::BindOnce(exit_callback_, Result::TPM_DBUS_ERROR));
      break;
    case user_data_auth::InstallAttributesState::FIRST_INSTALL:
      // This means that TPM is ready to write and we are good to go.
      ShowImpl();
      break;
    case user_data_auth::InstallAttributesState::VALID:
      // Valid to read, but can't rewrite. Need to clear the TPM.
    case user_data_auth::InstallAttributesState::INVALID:
      // Invalid to read. Need to clear the TPM.
      ClearAuth(base::BindOnce(exit_callback_, Result::TPM_ERROR));
      break;
    default:
      NOTREACHED();
  }
}

void EnrollmentScreen::ShowInteractiveScreen() {
  ClearAuth(base::BindOnce(&EnrollmentScreen::ShowSigninScreen,
                           weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::HideImpl() {
  scoped_network_observation_.Reset();
  if (view_)
    view_->Hide();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EnrollmentScreen::AuthenticateUsingAttestation() {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Authenticating using attestation.";
  elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
  if (features::IsAutoEnrollmentKioskInOobeEnabled()) {
    license_type_to_use_ = config_.license_type;
  }

  if (view_)
    view_->Show();
  CreateEnrollmentHelper();
  enrollment_helper_->EnrollUsingAttestation();
}

void EnrollmentScreen::OnLoginDone(const std::string& user,
                                   int license_type,
                                   const std::string& auth_code) {
  LOG_IF(ERROR, auth_code.empty()) << "Auth code is empty.";
  scoped_network_observation_.Reset();
  elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
  enrolling_user_domain_ = gaia::ExtractDomainName(user);
  license_type_to_use_ = static_cast<policy::LicenseType>(license_type);
  UMA(enrollment_failed_once_ ? policy::kMetricEnrollmentRestarted
                              : policy::kMetricEnrollmentStarted);

  if (view_)
    view_->ShowEnrollmentWorkingScreen();
  CreateEnrollmentHelper();
  enrollment_helper_->EnrollUsingAuthCode(auth_code);
}

void EnrollmentScreen::OnRetry() {
  retry_task_.Cancel();
  ProcessRetry();
}

void EnrollmentScreen::AutomaticRetry() {
  retry_backoff_->InformOfRequest(false);
  retry_task_.Reset(base::BindOnce(&EnrollmentScreen::ProcessRetry,
                                   weak_ptr_factory_.GetWeakPtr()));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, retry_task_.callback(), retry_backoff_->GetTimeUntilRelease());
}

void EnrollmentScreen::ProcessRetry() {
  ++num_retries_;
  LOG(WARNING) << "Enrollment retries: " << num_retries_
               << ", current auth: " << current_auth_ << ".";
  Show(context());
}

bool EnrollmentScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kCancelScreenAction) {
    if (config_.is_license_packaged_with_device && !config_.is_forced() &&
        (!(enrollment_helper_ && enrollment_helper_->InProgress()))) {
      ShowSkipEnrollmentDialogue();
      return true;
    } else {
      OnCancel();
      return true;
    }
  }
  return false;
}

void EnrollmentScreen::OnCancel() {
  if (enrollment_succeeded_) {
    // Cancellation is the same to confirmation after the successful enrollment.
    OnConfirmationClosed();
    return;
  }

  if (enrollment_helper_ && enrollment_helper_->InProgress()) {
    // Don't allow cancellation while enrollment is in progress.
    return;
  }

  // Record cancellation for that one enrollment mode.
  UMA(policy::kMetricEnrollmentCancelled);

  if (AdvanceToNextAuth()) {
    Show(context());
    return;
  }

  // Record the total time for all auth attempts until final cancellation.
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeCancel, elapsed_timer_);

  on_joined_callback_.Reset();
  if (authpolicy_login_helper_)
    authpolicy_login_helper_->CancelRequestsAndRestart();

  // The callback passed to ClearAuth is either called immediately or gets
  // wrapped in a callback bound to a weak pointer from `weak_ptr_factory_` - in
  // either case, passing exit_callback_ directly should be safe.
  ClearAuth(base::BindRepeating(exit_callback_,
                                config_.is_forced()
                                    ? Result::BACK_TO_AUTO_ENROLLMENT_CHECK
                                    : Result::BACK));
}

void EnrollmentScreen::OnConfirmationClosed() {
  if (features::IsOobeConsolidatedConsentEnabled())
    StartupUtils::MarkEulaAccepted();

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Confirmation closed.";
  // The callback passed to ClearAuth is either called immediately or gets
  // wrapped in a callback bound to a weak pointer from `weak_ptr_factory_` - in
  // either case, passing exit_callback_ directly should be safe.
  ClearAuth(base::BindRepeating(exit_callback_, Result::COMPLETED));

  if (ShouldAttemptRestart())
    chrome::AttemptRestart();
}

void EnrollmentScreen::OnAuthError(const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Auth error: " << error.state();
  RecordEnrollmentErrorMetrics();
  if (view_)
    view_->ShowAuthError(error);
}

void EnrollmentScreen::OnEnrollmentError(policy::EnrollmentStatus status) {
  LOG(ERROR) << "Enrollment error: " << status.status();
  RecordEnrollmentErrorMetrics();
  // If the DM server does not have a device pre-provisioned for attestation-
  // based enrollment and we have a fallback authentication, show it.
  if (status.status() == policy::EnrollmentStatus::REGISTRATION_FAILED &&
      status.client_status() == policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND &&
      current_auth_ == AUTH_ATTESTATION) {
    UMA(policy::kMetricEnrollmentDeviceNotPreProvisioned);
    if (AdvanceToNextAuth()) {
      Show(context());
      return;
    }
  }

  if (view_)
    view_->ShowEnrollmentStatus(status);
  if (IsAutomaticEnrollmentFlow())
    AutomaticRetry();
}

void EnrollmentScreen::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  LOG(ERROR) << "Other enrollment error: " << error;
  RecordEnrollmentErrorMetrics();
  if (view_)
    view_->ShowOtherError(error);
  if (IsAutomaticEnrollmentFlow())
    AutomaticRetry();
}

void EnrollmentScreen::OnDeviceEnrolled() {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Device enrolled.";
  enrollment_succeeded_ = true;
  // Some info to be shown on the success screen.
  if (view_)
    view_->SetEnterpriseDomainInfo(GetEnterpriseDomainManager(),
                                   ui::GetChromeOSDeviceName());

  enrollment_helper_->GetDeviceAttributeUpdatePermission();

  // Evaluates device policy TPMFirmwareUpdateSettings and updates the TPM if
  // the policy is set to auto-update vulnerable TPM firmware at enrollment.
  g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetTPMAutoUpdateModePolicyHandler()
      ->UpdateOnEnrollmentIfNeeded();
}

void EnrollmentScreen::OnIdentifierEntered(const std::string& email) {
  auto callback = base::BindOnce(&EnrollmentScreen::OnAccountStatusFetched,
                                 base::Unretained(this), email);
  status_checker_.reset();
  status_checker_ = std::make_unique<policy::AccountStatusCheckFetcher>(email);
  status_checker_->Fetch(std::move(callback));
}

void EnrollmentScreen::OnFirstShow() {
  UpdateStateInternal(NetworkError::ERROR_REASON_UPDATE, true);
}

void EnrollmentScreen::OnFrameLoadingCompleted() {
  if (network_state_informer_->state() != NetworkStateInformer::ONLINE) {
    return;
  }
  UpdateState(NetworkError::ERROR_REASON_UPDATE);
}

void EnrollmentScreen::OnAccountStatusFetched(
    const std::string& email,
    bool result,
    policy::AccountStatusCheckFetcher::AccountStatus status) {
  if (!view_)
    return;

  if (status == AccountStatusCheckFetcher::AccountStatus::kDasher ||
      status == AccountStatusCheckFetcher::AccountStatus::kUnknown ||
      result == false) {
    view_->ShowSigninScreen();
    return;
  }

  if (status == AccountStatusCheckFetcher::AccountStatus::
                    kConsumerWithConsumerDomain ||
      status == AccountStatusCheckFetcher::AccountStatus::
                    kConsumerWithBusinessDomain) {
    view_->ShowUserError(email);
    return;
  }

  // For all other types just show signin screen.
  view_->ShowSigninScreen();
}

void EnrollmentScreen::OnActiveDirectoryCredsProvided(
    const std::string& machine_name,
    const std::string& distinguished_name,
    int encryption_types,
    const std::string& username,
    const std::string& password) {
  DCHECK(authpolicy_login_helper_);
  authpolicy_login_helper_->JoinAdDomain(
      machine_name, distinguished_name, encryption_types, username, password,
      base::BindOnce(&EnrollmentScreen::OnActiveDirectoryJoined,
                     weak_ptr_factory_.GetWeakPtr(), machine_name, username));
}

void EnrollmentScreen::OnDeviceAttributeProvided(const std::string& asset_id,
                                                 const std::string& location) {
  enrollment_helper_->UpdateDeviceAttributes(asset_id, location);
}

void EnrollmentScreen::OnDeviceAttributeUpdatePermission(bool granted) {
  // If user is permitted to update device attributes
  // Show attribute prompt screen
  if (granted && !WizardController::skip_enrollment_prompts_for_testing()) {
    StartupUtils::MarkDeviceRegistered(
        base::BindOnce(&EnrollmentScreen::ShowAttributePromptScreen,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    StartupUtils::MarkDeviceRegistered(
        base::BindOnce(&EnrollmentScreen::ShowEnrollmentStatusOnSuccess,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void EnrollmentScreen::OnDeviceAttributeUploadCompleted(bool success) {
  if (success) {
    // If the device attributes have been successfully uploaded, fetch policy.
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    connector->GetDeviceCloudPolicyManager()->core()->RefreshSoon();
    if (view_) {
      view_->ShowEnrollmentStatus(policy::EnrollmentStatus::ForStatus(
          policy::EnrollmentStatus::SUCCESS));
    }
  } else if (view_) {
    view_->ShowEnrollmentStatus(policy::EnrollmentStatus::ForStatus(
        policy::EnrollmentStatus::ATTRIBUTE_UPDATE_FAILED));
  }
}

void EnrollmentScreen::ShowAttributePromptScreen() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();

  std::string asset_id;
  std::string location;

  if (!context()->configuration.empty()) {
    auto* asset_id_value =
        context()->configuration.FindString(configuration::kEnrollmentAssetId);
    if (asset_id_value) {
      // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's
      // preserved in the logs.
      LOG(WARNING) << "Using Asset ID from configuration " << *asset_id_value;
      asset_id = *asset_id_value;
    }
    auto* location_value =
        context()->configuration.FindString(configuration::kEnrollmentLocation);
    if (location_value) {
      LOG(WARNING) << "Using Location from configuration " << *location_value;
      location = *location_value;
    }
  }

  policy::CloudPolicyStore* store = policy_manager->core()->store();

  const enterprise_management::PolicyData* policy = store->policy();

  if (policy) {
    asset_id = policy->annotated_asset_id();
    location = policy->annotated_location();
  }

  if (!context()->configuration.empty()) {
    bool auto_attributes =
        context()
            ->configuration.FindBool(configuration::kEnrollmentAutoAttributes)
            .value_or(false);
    if (auto_attributes) {
      LOG(WARNING) << "Automatically accept attributes";
      OnDeviceAttributeProvided(asset_id, location);
      return;
    }
  }

  if (view_)
    view_->ShowAttributePromptScreen(asset_id, location);
}

void EnrollmentScreen::ShowEnrollmentStatusOnSuccess() {
  retry_backoff_->InformOfRequest(true);
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeSuccess, elapsed_timer_);
  if (IsAutomaticEnrollmentFlow() ||
      WizardController::skip_enrollment_prompts_for_testing()) {
    OnConfirmationClosed();
  } else if (view_) {
    view_->ShowEnrollmentStatus(
        policy::EnrollmentStatus::ForStatus(policy::EnrollmentStatus::SUCCESS));
  }
}

void EnrollmentScreen::UMA(policy::MetricEnrollment sample) {
  EnrollmentUMA(sample, config_.mode);
}

void EnrollmentScreen::ShowSigninScreen() {
  if (view_)
    view_->Show();
}

void EnrollmentScreen::RecordEnrollmentErrorMetrics() {
  enrollment_failed_once_ = true;
  //  TODO(crbug.com/896793): Have other metrics for each auth mechanism.
  if (elapsed_timer_ && current_auth_ == next_auth_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeFailure, elapsed_timer_);
}

void EnrollmentScreen::JoinDomain(
    const std::string& dm_token,
    const std::string& domain_join_config,
    policy::OnDomainJoinedCallback on_joined_callback) {
  if (!authpolicy_login_helper_)
    authpolicy_login_helper_ = std::make_unique<AuthPolicyHelper>();
  authpolicy_login_helper_->set_dm_token(dm_token);
  on_joined_callback_ = std::move(on_joined_callback);
  if (view_) {
    scoped_network_observation_.Reset();
    view_->ShowActiveDirectoryScreen(
        domain_join_config, std::string() /* machine_name */,
        std::string() /* username */, authpolicy::ERROR_NONE);
  }
}

void EnrollmentScreen::OnBrowserRestart() {
  // When the browser is restarted, renderers are shutdown and the `view_`
  // wants to know in order to stop trying to use the soon-invalid renderers.
  if (view_)
    view_->Shutdown();
}

void EnrollmentScreen::OnActiveDirectoryJoined(
    const std::string& machine_name,
    const std::string& username,
    authpolicy::ErrorType error,
    const std::string& machine_domain) {
  if (error == authpolicy::ERROR_NONE) {
    // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
    // in the logs.
    LOG(WARNING) << "Joined active directory";
    if (view_)
      view_->ShowEnrollmentWorkingScreen();
    std::move(on_joined_callback_).Run(machine_domain);
    return;
  }
  LOG(ERROR) << "Active directory join error: " << error;
  if (view_) {
    scoped_network_observation_.Reset();
    view_->ShowActiveDirectoryScreen(std::string() /* domain_join_config */,
                                     machine_name, username, error);
  }
}

void EnrollmentScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancelTPMCheck) {
    OnCancel();
    return;
  }
  if (action_id == kUserActionSkipDialogConfirmation) {
    OnCancel();
    return;
  }
  BaseScreen::OnUserAction(args);
}

void EnrollmentScreen::UpdateChromadMigrationOobeFlow(bool exists) {
  is_chromad_migration_oobe_flow_ = exists;
}

bool EnrollmentScreen::IsAutomaticEnrollmentFlow() {
  return is_chromad_migration_oobe_flow_ ||
         WizardController::IsZeroTouchHandsOffOobeFlow() || is_rollback_flow_;
}

bool EnrollmentScreen::IsEnrollmentScreenHiddenByError() {
  return (LoginDisplayHost::default_host()->GetOobeUI()->current_screen() ==
              ErrorScreenView::kScreenId &&
          error_screen_->GetParentScreen() == EnrollmentScreenView::kScreenId);
}

void EnrollmentScreen::UpdateState(NetworkError::ErrorReason reason) {
  UpdateStateInternal(reason, false);
}

// TODO(rsorokin): This function is mostly copied from SigninScreenHandler and
// should be refactored in the future.
void EnrollmentScreen::UpdateStateInternal(NetworkError::ErrorReason reason,
                                           bool force_update) {
  if (!force_update && !IsOnEnrollmentScreen() &&
      !IsEnrollmentScreenHiddenByError()) {
    return;
  }

  if (!force_update && !scoped_network_observation_.IsObserving()) {
    return;
  }

  NetworkStateInformer::State state = network_state_informer_->state();
  const bool is_online = (state == NetworkStateInformer::ONLINE);
  const bool is_behind_captive_portal =
      (state == NetworkStateInformer::CAPTIVE_PORTAL);
  const bool is_frame_error = reason == NetworkError::ERROR_REASON_FRAME_ERROR;

  LOG(WARNING) << "EnrollmentScreen::UpdateStateInternal(): "
               << "state=" << state << ", "
               << "reason=" << NetworkError::ErrorReasonString(reason);

  if (is_online || !is_behind_captive_portal) {
    error_screen_->HideCaptivePortal();
  }

  if (is_frame_error) {
    LOG(WARNING) << "Retry page load";
    // TODO(rsorokin): Too many consecutive reloads.
    view_->ReloadSigninScreen();
  }

  if (!is_online || is_frame_error) {
    SetupAndShowOfflineMessage(state, reason);
  } else {
    HideOfflineMessage(state, reason);
  }
}

void EnrollmentScreen::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal =
      NetworkStateInformer::IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = NetworkStateInformer::IsProxyError(state, reason);
  const bool is_frame_error = reason == NetworkError::ERROR_REASON_FRAME_ERROR;

  if (is_proxy_error) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                 std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsOnEnrollmentScreen() ||
        (error_screen_->GetErrorState() != NetworkError::ERROR_STATE_PORTAL)) {
      error_screen_->FixCaptivePortal();
    }
    const std::string network_name =
        NetworkStateInformer::GetNetworkName(network_path);
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                 network_name);
  } else if (is_frame_error) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT,
                                 std::string());
  } else {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                 std::string());
  }

  if (LoginDisplayHost::default_host()->GetOobeUI()->current_screen() !=
      ErrorScreenView::kScreenId) {
    error_screen_->SetUIState(NetworkError::UI_STATE_SIGNIN);
    error_screen_->SetParentScreen(EnrollmentScreenView::kScreenId);
    error_screen_->SetHideCallback(
        base::BindOnce(&EnrollmentScreenView::Show, view_));
    error_screen_->Show(nullptr);
    histogram_helper_.OnErrorShow(error_screen_->GetErrorState());
  }
}

void EnrollmentScreen::HideOfflineMessage(NetworkStateInformer::State state,
                                          NetworkError::ErrorReason reason) {
  if (IsEnrollmentScreenHiddenByError()) {
    error_screen_->Hide();
  }
  histogram_helper_.OnErrorHide();
}

}  // namespace ash
