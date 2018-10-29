// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/configuration_keys.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_uma.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/pairing/controller_pairing_controller.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

using namespace pairing_chromeos;
using policy::EnrollmentConfig;

// Do not change the UMA histogram parameters without renaming the histograms!
#define UMA_ENROLLMENT_TIME(histogram_name, elapsed_timer)                   \
  do {                                                                       \
    UMA_HISTOGRAM_CUSTOM_TIMES(                                              \
        (histogram_name), (elapsed_timer)->Elapsed(),                        \
        base::TimeDelta::FromMilliseconds(100) /* min */,                    \
        base::TimeDelta::FromMinutes(15) /* max */, 100 /* bucket_count */); \
  } while (0)

namespace {

const char* const kMetricEnrollmentTimeCancel =
    "Enterprise.EnrollmentTime.Cancel";
const char* const kMetricEnrollmentTimeFailure =
    "Enterprise.EnrollmentTime.Failure";
const char* const kMetricEnrollmentTimeSuccess =
    "Enterprise.EnrollmentTime.Success";

const char* const kLicenseTypePerpetual = "perpetual";
const char* const kLicenseTypeAnnual = "annual";
const char* const kLicenseTypeKiosk = "kiosk";

// Retry policy constants.
constexpr int kInitialDelayMS = 4 * 1000;  // 4 seconds
constexpr double kMultiplyFactor = 1.5;
constexpr double kJitterFactor = 0.1;           // +/- 10% jitter
constexpr int64_t kMaxDelayMS = 8 * 60 * 1000;  // 8 minutes

::policy::LicenseType GetLicenseTypeById(const std::string& id) {
  if (id == kLicenseTypePerpetual)
    return ::policy::LicenseType::PERPETUAL;
  if (id == kLicenseTypeAnnual)
    return ::policy::LicenseType::ANNUAL;
  if (id == kLicenseTypeKiosk)
    return ::policy::LicenseType::KIOSK;
  return ::policy::LicenseType::UNKNOWN;
}

std::string GetLicenseIdByType(::policy::LicenseType type) {
  switch (type) {
    case ::policy::LicenseType::PERPETUAL:
      return kLicenseTypePerpetual;
    case ::policy::LicenseType::ANNUAL:
      return kLicenseTypeAnnual;
    case ::policy::LicenseType::KIOSK:
      return kLicenseTypeKiosk;
    default:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace

namespace chromeos {

// static
EnrollmentScreen* EnrollmentScreen::Get(ScreenManager* manager) {
  return static_cast<EnrollmentScreen*>(
      manager->GetScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT));
}

EnrollmentScreen::EnrollmentScreen(BaseScreenDelegate* base_screen_delegate,
                                   EnrollmentScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_OOBE_ENROLLMENT),
      view_(view),
      weak_ptr_factory_(this) {
  retry_policy_.num_errors_to_ignore = 0;
  retry_policy_.initial_delay_ms = kInitialDelayMS;
  retry_policy_.multiply_factor = kMultiplyFactor;
  retry_policy_.jitter_factor = kJitterFactor;
  retry_policy_.maximum_backoff_ms = kMaxDelayMS;
  retry_policy_.entry_lifetime_ms = -1;
  retry_policy_.always_use_initial_delay = true;
  retry_backoff_.reset(new net::BackoffEntry(&retry_policy_));
}

EnrollmentScreen::~EnrollmentScreen() {
  DCHECK(!enrollment_helper_ || g_browser_process->IsShuttingDown() ||
         browser_shutdown::IsTryingToQuit() ||
         DBusThreadManager::Get()->IsUsingFakes());
}

void EnrollmentScreen::SetParameters(
    const policy::EnrollmentConfig& enrollment_config,
    pairing_chromeos::ControllerPairingController* shark_controller) {
  enrollment_config_ = enrollment_config;
  switch (enrollment_config_.auth_mechanism) {
    case EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE:
      current_auth_ = AUTH_OAUTH;
      last_auth_ = AUTH_OAUTH;
      break;
    case EnrollmentConfig::AUTH_MECHANISM_ATTESTATION:
      current_auth_ = AUTH_ATTESTATION;
      last_auth_ = AUTH_ATTESTATION;
      break;
    case EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE:
      current_auth_ = AUTH_ATTESTATION;
      last_auth_ = enrollment_config_.should_enroll_interactively()
                       ? AUTH_OAUTH
                       : AUTH_ATTESTATION;
      break;
    default:
      NOTREACHED();
      break;
  }
  shark_controller_ = shark_controller;
  SetConfig();
}

void EnrollmentScreen::SetConfig() {
  config_ = enrollment_config_;
  if (current_auth_ == AUTH_OAUTH &&
      enrollment_config_.mode ==
          policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED) {
    config_.mode = policy::EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK;
  } else if (current_auth_ == AUTH_ATTESTATION &&
             !enrollment_config_.is_mode_attestation()) {
    config_.mode = enrollment_config_.is_attestation_forced()
                       ? policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED
                       : policy::EnrollmentConfig::MODE_ATTESTATION;
  }
  view_->SetParameters(this, config_);
  enrollment_helper_ = nullptr;
}

bool EnrollmentScreen::AdvanceToNextAuth() {
  if (current_auth_ != last_auth_ && current_auth_ == AUTH_ATTESTATION) {
    current_auth_ = last_auth_;
    SetConfig();
    return true;
  }
  return false;
}

void EnrollmentScreen::CreateEnrollmentHelper() {
  if (!enrollment_helper_) {
    enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
        this, this, config_, enrolling_user_domain_);
  }
}

void EnrollmentScreen::ClearAuth(const base::Closure& callback) {
  if (!enrollment_helper_) {
    callback.Run();
    return;
  }
  enrollment_helper_->ClearAuth(base::Bind(&EnrollmentScreen::OnAuthCleared,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           callback));
}

void EnrollmentScreen::OnAuthCleared(const base::Closure& callback) {
  enrollment_helper_ = nullptr;
  callback.Run();
}

void EnrollmentScreen::Show() {
  UMA(policy::kMetricEnrollmentTriggered);
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

void EnrollmentScreen::ShowInteractiveScreen() {
  ClearAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                       weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::Hide() {
  view_->Hide();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EnrollmentScreen::AuthenticateUsingAttestation() {
  VLOG(1) << "Authenticating using attestation.";
  elapsed_timer_.reset(new base::ElapsedTimer());
  view_->Show();
  CreateEnrollmentHelper();
  enrollment_helper_->EnrollUsingAttestation();
}

void EnrollmentScreen::OnLoginDone(const std::string& user,
                                   const std::string& auth_code) {
  LOG_IF(ERROR, auth_code.empty()) << "Auth code is empty.";
  elapsed_timer_.reset(new base::ElapsedTimer());
  enrolling_user_domain_ = gaia::ExtractDomainName(user);
  UMA(enrollment_failed_once_ ? policy::kMetricEnrollmentRestarted
                              : policy::kMetricEnrollmentStarted);

  view_->ShowEnrollmentSpinnerScreen();
  CreateEnrollmentHelper();
  enrollment_helper_->EnrollUsingAuthCode(
      auth_code, shark_controller_ != nullptr /* fetch_additional_token */);
}

void EnrollmentScreen::OnLicenseTypeSelected(const std::string& license_type) {
  view_->ShowEnrollmentSpinnerScreen();
  const ::policy::LicenseType license = GetLicenseTypeById(license_type);
  CHECK(license != ::policy::LicenseType::UNKNOWN)
      << "license_type = " << license_type;
  enrollment_helper_->UseLicenseType(license);
}

void EnrollmentScreen::OnRetry() {
  retry_task_.Cancel();
  ProcessRetry();
}

void EnrollmentScreen::AutomaticRetry() {
  retry_backoff_->InformOfRequest(false);
  retry_task_.Reset(base::Bind(&EnrollmentScreen::ProcessRetry,
                               weak_ptr_factory_.GetWeakPtr()));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, retry_task_.callback(), retry_backoff_->GetTimeUntilRelease());
}

void EnrollmentScreen::ProcessRetry() {
  ++num_retries_;
  LOG(WARNING) << "Enrollment retries: " << num_retries_
               << ", current_auth_: " << current_auth_;
  Show();
}

void EnrollmentScreen::OnCancel() {
  if (AdvanceToNextAuth()) {
    Show();
    return;
  }

  if (enrollment_succeeded_) {
    // Cancellation is the same to confirmation after the successful enrollment.
    OnConfirmationClosed();
    return;
  }

  on_joined_callback_.Reset();
  if (authpolicy_login_helper_)
    authpolicy_login_helper_->CancelRequestsAndRestart();

  UMA(policy::kMetricEnrollmentCancelled);
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeCancel, elapsed_timer_);

  const ScreenExitCode exit_code =
      config_.is_forced() ? ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK
                          : ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED;
  ClearAuth(
      base::Bind(&EnrollmentScreen::Finish, base::Unretained(this), exit_code));
}

void EnrollmentScreen::OnConfirmationClosed() {
  ClearAuth(base::Bind(&EnrollmentScreen::Finish, base::Unretained(this),
                       ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED));
  // Restart browser to switch from DeviceCloudPolicyManagerChromeOS to
  // DeviceActiveDirectoryPolicyManager.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsActiveDirectoryManaged()) {
    // TODO(tnagel): Refactor BrowserPolicyConnectorChromeOS so that device
    // policy providers are only registered after enrollment has finished and
    // thus the correct one can be picked without restarting the browser.
    chrome::AttemptRestart();
  }
}

void EnrollmentScreen::OnAuthError(const GoogleServiceAuthError& error) {
  RecordEnrollmentErrorMetrics();
  view_->ShowAuthError(error);
}

void EnrollmentScreen::OnMultipleLicensesAvailable(
    const EnrollmentLicenseMap& licenses) {
  if (GetConfiguration()) {
    auto* license_type_value = GetConfiguration()->FindKeyOfType(
        configuration::kEnrollmentLicenseType, base::Value::Type::STRING);
    if (license_type_value) {
      const std::string& license_type = license_type_value->GetString();
      for (const auto& it : licenses) {
        if (license_type == GetLicenseIdByType(it.first) && it.second > 0) {
          VLOG(1) << "Using License type from configuration " << license_type;
          OnLicenseTypeSelected(license_type);
          return;
        }
      }
      VLOG(1) << "No licenses for License type from configuration "
              << license_type;
    }
  }
  base::DictionaryValue license_dict;
  for (const auto& it : licenses)
    license_dict.SetInteger(GetLicenseIdByType(it.first), it.second);
  view_->ShowLicenseTypeSelectionScreen(license_dict);
}

void EnrollmentScreen::OnEnrollmentError(policy::EnrollmentStatus status) {
  RecordEnrollmentErrorMetrics();
  // If the DM server does not have a device pre-provisioned for attestation-
  // based enrollment and we have a fallback authentication, show it.
  if (status.status() == policy::EnrollmentStatus::REGISTRATION_FAILED &&
      status.client_status() == policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND &&
      current_auth_ == AUTH_ATTESTATION && AdvanceToNextAuth()) {
    Show();
  } else {
    view_->ShowEnrollmentStatus(status);
    if (WizardController::UsingHandsOffEnrollment())
      AutomaticRetry();
  }
}

void EnrollmentScreen::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  RecordEnrollmentErrorMetrics();
  view_->ShowOtherError(error);
  if (WizardController::UsingHandsOffEnrollment())
    AutomaticRetry();
}

void EnrollmentScreen::OnDeviceEnrolled(const std::string& additional_token) {
  enrollment_succeeded_ = true;
  if (!additional_token.empty())
    SendEnrollmentAuthToken(additional_token);

  enrollment_helper_->GetDeviceAttributeUpdatePermission();
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
  if (granted && !WizardController::skip_enrollment_prompts()) {
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
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->GetDeviceCloudPolicyManager()->core()->RefreshSoon();
    view_->ShowEnrollmentStatus(
        policy::EnrollmentStatus::ForStatus(policy::EnrollmentStatus::SUCCESS));
  } else {
    view_->ShowEnrollmentStatus(policy::EnrollmentStatus::ForStatus(
        policy::EnrollmentStatus::ATTRIBUTE_UPDATE_FAILED));
  }
}

void EnrollmentScreen::ShowAttributePromptScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();

  std::string asset_id;
  std::string location;

  if (GetConfiguration()) {
    auto* asset_id_value = GetConfiguration()->FindKeyOfType(
        configuration::kEnrollmentAssetId, base::Value::Type::STRING);
    if (asset_id_value) {
      VLOG(1) << "Using Asset ID from configuration "
              << asset_id_value->GetString();
      asset_id = asset_id_value->GetString();
    }
    auto* location_value = GetConfiguration()->FindKeyOfType(
        configuration::kEnrollmentLocation, base::Value::Type::STRING);
    if (location_value) {
      VLOG(1) << "Using Location from configuration "
              << location_value->GetString();
      location = location_value->GetString();
    }
  }

  policy::CloudPolicyStore* store = policy_manager->core()->store();

  const enterprise_management::PolicyData* policy = store->policy();

  if (policy) {
    asset_id = policy->annotated_asset_id();
    location = policy->annotated_location();
  }

  if (GetConfiguration()) {
    auto* auto_attributes = GetConfiguration()->FindKeyOfType(
        configuration::kEnrollmentAutoAttributes, base::Value::Type::BOOLEAN);
    if (auto_attributes && auto_attributes->GetBool()) {
      VLOG(1) << "Automatically accept attributes";
      OnDeviceAttributeProvided(asset_id, location);
      return;
    }
  }

  view_->ShowAttributePromptScreen(asset_id, location);
}

void EnrollmentScreen::SendEnrollmentAuthToken(const std::string& token) {
  DCHECK(shark_controller_);
  shark_controller_->OnAuthenticationDone(enrolling_user_domain_, token);
}

void EnrollmentScreen::ShowEnrollmentStatusOnSuccess() {
  retry_backoff_->InformOfRequest(true);
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeSuccess, elapsed_timer_);
  if (WizardController::UsingHandsOffEnrollment() ||
      WizardController::skip_enrollment_prompts()) {
    OnConfirmationClosed();
  } else {
    view_->ShowEnrollmentStatus(
        policy::EnrollmentStatus::ForStatus(policy::EnrollmentStatus::SUCCESS));
  }
}

void EnrollmentScreen::UMA(policy::MetricEnrollment sample) {
  EnrollmentUMA(sample, config_.mode);
}

void EnrollmentScreen::ShowSigninScreen() {
  view_->Show();
  view_->ShowSigninScreen();
}

void EnrollmentScreen::RecordEnrollmentErrorMetrics() {
  enrollment_failed_once_ = true;
  //  TODO(drcrash): Maybe create multiple metrics (http://crbug.com/640313)?
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeFailure, elapsed_timer_);
}

void EnrollmentScreen::JoinDomain(const std::string& dm_token,
                                  const std::string& domain_join_config,
                                  OnDomainJoinedCallback on_joined_callback) {
  if (!authpolicy_login_helper_)
    authpolicy_login_helper_ = std::make_unique<AuthPolicyLoginHelper>();
  authpolicy_login_helper_->set_dm_token(dm_token);
  on_joined_callback_ = std::move(on_joined_callback);
  view_->ShowActiveDirectoryScreen(
      domain_join_config, std::string() /* machine_name */,
      std::string() /* username */, authpolicy::ERROR_NONE);
}

void EnrollmentScreen::OnActiveDirectoryJoined(
    const std::string& machine_name,
    const std::string& username,
    authpolicy::ErrorType error,
    const std::string& machine_domain) {
  if (error == authpolicy::ERROR_NONE) {
    view_->ShowEnrollmentSpinnerScreen();
    std::move(on_joined_callback_).Run(machine_domain);
    return;
  }
  view_->ShowActiveDirectoryScreen(std::string() /* domain_join_config */,
                                   machine_name, username, error);
}

}  // namespace chromeos
