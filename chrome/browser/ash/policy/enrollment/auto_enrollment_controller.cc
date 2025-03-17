// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client_impl.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// This is used for logs that may not be strictly necessary but are of great use
// because they will log whether determinations are needed or not, along with
// some context. The information used to be logged using VLOG(1), and therefore
// was not available in customer logs. Because the only other logs have some
// ambiguity (e.g. there will not be a log if the device decides it does not
// need to make a determination), troubleshooting is difficult. If this changes,
// this can be made VLOG(1) again.
//
// We use LOG(WARNING) to guarantee that the messages will be into feedback
// reports.
#define LOG_DETERMINATION() LOG(WARNING)

namespace policy {
namespace {

// Maximum time to wait for the auto-enrollment check to reach a decision.
// Note that this encompasses all steps `AutoEnrollmentController` performs in
// order to determine if the device should be auto-enrolled.
// If `kSafeguardTimeout` after `Start()` has been called,
// `AutoEnrollmentController::state()` is still AutoEnrollmentState::kPending,
// the AutoEnrollmentController will switch to
// `AutoEnrollmentResult::kNoEnrollment` or
// `AutoEnrollmentSafeguardTimeoutError` (see
// `AutoEnrollmentController::Timeout`). Note that this timeout should not be
// too short, because one of the steps `AutoEnrollmentController` performs -
// downloading identifier hash buckets - can be non-negligible, especially on 2G
// connections.
constexpr base::TimeDelta kSafeguardTimeout = base::Seconds(90);

enum class AutoEnrollmentControllerTimeoutReport {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kTimeoutCancelled = 0,
  // kTimeoutFRE_Obsolete = 1,
  kTimeout = 2,
  kTimeoutUnified = 3,
  kMaxValue = kTimeoutUnified
};

void ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport report) {
  base::UmaHistogramEnumeration("Enterprise.AutoEnrollmentControllerTimeout",
                                report);
}

bool IsFinalAutoEnrollmentState(AutoEnrollmentState state) {
  return state.has_value();
}

}  // namespace

AutoEnrollmentController::AutoEnrollmentController(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : AutoEnrollmentController(
          ash::DeviceSettingsService::Get(),
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->device_management_service(),
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->GetStateKeysBroker(),
          ash::NetworkHandler::Get()->network_state_handler(),
          base::BindRepeating(&policy::psm::RlweDmserverClientImpl::Create),
          base::BindRepeating(EnrollmentStateFetcher::Create),
          shared_url_loader_factory) {}

AutoEnrollmentController::AutoEnrollmentController(
    ash::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service,
    ServerBackedStateKeysBroker* state_keys_broker,
    ash::NetworkStateHandler* network_state_handler,
    RlweClientFactory psm_rlwe_client_factory,
    EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : device_settings_service_(device_settings_service),
      device_management_service_(device_management_service),
      state_keys_broker_(state_keys_broker),
      psm_rlwe_client_factory_(std::move(psm_rlwe_client_factory)),
      enrollment_state_fetcher_factory_(
          std::move(enrollment_state_fetcher_factory)),
      shared_url_loader_factory_(shared_url_loader_factory),
      network_state_handler_(network_state_handler) {}

AutoEnrollmentController::~AutoEnrollmentController() = default;

void AutoEnrollmentController::Start() {
  if (state_.has_value() && IsFinalAutoEnrollmentState(state_.value())) {
    return;
  }

  if (!network_state_observation_.IsObserving()) {
    // The controller could have already subscribed on the start and now we're
    // restarting after an error.
    network_state_observation_.Observe(network_state_handler_);
  }

  if (IsInProgress()) {
    return;
  }

  // If Unified State Determination is turned off (e.g. by command line switch),
  // then enrollment will not be forced.
  if (!AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    UpdateState(AutoEnrollmentResult::kNoEnrollment);
    return;
  }

  // Arm the belts-and-suspenders timer to avoid hangs.
  safeguard_timer_.Start(FROM_HERE, kSafeguardTimeout,
                         base::BindOnce(&AutoEnrollmentController::Timeout,
                                        weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/353731379):
  // BrowserPolicyConnector::ScheduleServiceInitialization.
  if (device_management_service_) {
    device_management_service_->ScheduleInitialization(0);
  } else {
    CHECK_IS_TEST();
  }

  LOG(WARNING) << "Starting state determination";
  enrollment_state_fetcher_ = enrollment_state_fetcher_factory_.Run(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      g_browser_process->local_state(), psm_rlwe_client_factory_,
      device_management_service_, shared_url_loader_factory_,
      state_keys_broker_, device_settings_service_,
      ash::OobeConfiguration::Get());

  enrollment_state_fetcher_->Start();
  return;
}

base::CallbackListSubscription
AutoEnrollmentController::RegisterProgressCallback(
    const ProgressCallbackList::CallbackType& callback) {
  return progress_callbacks_.Add(callback);
}

void AutoEnrollmentController::PortalStateChanged(
    const ash::NetworkState* /*default_network*/,
    const ash::NetworkState::PortalState portal_state) {
  // It is safe to retry regardless of the current state: if the check is idle
  // or failed, we will restart the check process. If the check is in progress,
  // the retry call will be ignored.
  if (portal_state == ash::NetworkState::PortalState::kOnline) {
    Start();
  }
}

void AutoEnrollmentController::OnShuttingDown() {
  network_state_observation_.Reset();
}

void AutoEnrollmentController::SetRlweClientFactoryForTesting(
    RlweClientFactory test_factory) {
  CHECK_IS_TEST();
  psm_rlwe_client_factory_ = std::move(test_factory);
}

void AutoEnrollmentController::UpdateState(AutoEnrollmentState new_state) {
  LOG(WARNING) << "New auto-enrollment state: "
               << AutoEnrollmentStateToString(new_state);
  state_ = new_state;

  if (IsFinalAutoEnrollmentState(state_.value())) {
    network_state_observation_.Reset();
  }

  // Stop the safeguard timer once a result comes in.
  safeguard_timer_.Stop();
  // Reset enrollment state fetcher to allow restarting.
  enrollment_state_fetcher_.reset();
  ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutCancelled);

  // Device disabling mode is relying on device state stored in install
  // attributes. In case that file is corrupted, this should prevent device
  // re-enabling.
  if (state_ == AutoEnrollmentResult::kDisabled) {
    DeviceMode device_mode = ash::InstallAttributes::Get()->GetMode();
    if (device_mode == DeviceMode::DEVICE_MODE_PENDING ||
        device_mode == DeviceMode::DEVICE_MODE_NOT_SET) {
      device_settings_service_->SetDeviceMode(
          DeviceMode::DEVICE_MODE_ENTERPRISE);
    }
  }

  if (state_ == AutoEnrollmentResult::kNoEnrollment ||
      state_ == AutoEnrollmentResult::kSuggestedEnrollment) {
    StartCleanupForcedReEnrollment();
  } else {
    progress_callbacks_.Notify(state_.value());
  }
}

void AutoEnrollmentController::StartCleanupForcedReEnrollment() {
  // D-Bus services may not be available yet, so we call
  // WaitForServiceToBeAvailable. See https://crbug.com/841627.
  ash::InstallAttributesClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(
          &AutoEnrollmentController::StartRemoveFirmwareManagementParameters,
          weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartRemoveFirmwareManagementParameters(
    bool service_is_ready) {
  DCHECK(state_ == AutoEnrollmentResult::kNoEnrollment ||
         state_ == AutoEnrollmentResult::kSuggestedEnrollment);
  if (!service_is_ready) {
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";
    progress_callbacks_.Notify(state_.value());
    return;
  }

  device_management::RemoveFirmwareManagementParametersRequest request;
  ash::InstallAttributesClient::Get()->RemoveFirmwareManagementParameters(
      request,
      base::BindOnce(
          &AutoEnrollmentController::OnFirmwareManagementParametersRemoved,
          weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnFirmwareManagementParametersRemoved(
    std::optional<device_management::RemoveFirmwareManagementParametersReply>
        reply) {
  if (!reply.has_value() || reply->error() !=
                                device_management::DeviceManagementErrorCode::
                                    DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to remove firmware management parameters.";
  }

  // D-Bus services may not be available yet, so we call
  // WaitForServiceToBeAvailable. See https://crbug.com/841627.
  ash::SessionManagerClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&AutoEnrollmentController::StartClearBlockDevmodeVpd,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartClearBlockDevmodeVpd(
    bool service_is_ready) {
  DCHECK(state_ == AutoEnrollmentResult::kNoEnrollment ||
         state_ == AutoEnrollmentResult::kSuggestedEnrollment);
  if (!service_is_ready) {
    LOG(ERROR)
        << "Failed waiting for session_manager D-Bus service availability.";
    progress_callbacks_.Notify(state_.value());
    return;
  }

  // This clears block_devmode in the VPD.
  ash::SessionManagerClient::Get()->ClearBlockDevmodeVpd(
      base::BindOnce(&AutoEnrollmentController::OnBlockDevmodeClearedVpd,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnBlockDevmodeClearedVpd(bool succeeded) {
  if (!succeeded) {
    LOG(ERROR) << "Failed to clear forced re-enrollment flags in RW VPD.";
  }

  progress_callbacks_.Notify(state_.value());
}

void AutoEnrollmentController::Timeout() {
  // The timer is only started if Unified State Determination is enabled.
  CHECK(AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled());

  // This can actually happen in some cases, for example when state key
  // generation is waiting for time sync or the server just doesn't reply and
  // keeps the connection open.
  LOG(ERROR) << "EnrollmentStateFetcher didn't complete within time limit.";
  UpdateState(base::unexpected(AutoEnrollmentSafeguardTimeoutError{}));
  ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutUnified);
}

bool AutoEnrollmentController::IsInProgress() const {
  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    if (enrollment_state_fetcher_) {
      // If a fetcher has already been created, bail out.
      VLOG(1) << "Enrollment state fetcher is already running.";
      return true;
    }
  }

  return false;
}

void AutoEnrollmentController::SetEnrollmentStateFetcherFactoryForTesting(
    EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory) {
  CHECK_IS_TEST();
  if (enrollment_state_fetcher_factory) {
    enrollment_state_fetcher_factory_ = enrollment_state_fetcher_factory;
  } else {
    enrollment_state_fetcher_factory_ =
        base::BindRepeating(EnrollmentStateFetcher::Create);
  }
}

bool AutoEnrollmentController::IsGuestSigninAllowed() const {
  return state_ == AutoEnrollmentResult::kNoEnrollment ||
         state_ == AutoEnrollmentResult::kSuggestedEnrollment;
}

}  // namespace policy
