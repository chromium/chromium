// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"

#include <string>

#include "base/notreached.h"
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {
namespace {

using MojomRmaState = ash::shimless_rma::mojom::State;
using ProtoRmadState = rmad::RmadState::StateCase;

using MojomRmadErrorCode = ash::shimless_rma::mojom::RmadErrorCode;
using ProtoRmadErrorCode = rmad::RmadErrorCode;

using MojomOsUpdateOperation = ash::shimless_rma::mojom::OsUpdateOperation;
using ProtoOsUpdateOperation = update_engine::Operation;

using MojomUpdateErrorCode = ash::shimless_rma::mojom::UpdateErrorCode;
using ProtoOsUpdateErrorCode = update_engine::ErrorCode;

using MojomComponentType = ash::shimless_rma::mojom::ComponentType;
using ProtoComponentType = rmad::RmadComponent;

using MojomComponentRepairState =
    ash::shimless_rma::mojom::ComponentRepairStatus;
using ProtoComponentRepairState =
    rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus;

using MojomWpDisableAction =
    ash::shimless_rma::mojom::WriteProtectDisableCompleteAction;
using ProtoWpDisableAction = rmad::WriteProtectDisableCompleteState::Action;

using MojomProvisioningStatus = ash::shimless_rma::mojom::ProvisioningStatus;
using ProtoProvisioningStatus = rmad::ProvisionStatus::Status;

using MojomProvisioningError = ash::shimless_rma::mojom::ProvisioningError;
using ProtoProvisioningError = rmad::ProvisionStatus::Error;

using MojomCalibrationInstruction =
    ash::shimless_rma::mojom::CalibrationSetupInstruction;
using ProtoCalibrationInstruction = rmad::CalibrationSetupInstruction;

using MojomCalibrationOverallStatus =
    ash::shimless_rma::mojom::CalibrationOverallStatus;
using ProtoCalibrationOverallStatus = rmad::CalibrationOverallStatus;

using MojomCalibrationStatus = ash::shimless_rma::mojom::CalibrationStatus;
using ProtoCalibrationStatus =
    rmad::CalibrationComponentStatus_CalibrationStatus;

using MojomFinalizationStatus = ash::shimless_rma::mojom::FinalizationStatus;
using ProtoFinalizationStatus = rmad::FinalizeStatus_Status;

using MojomFinalizationError = ash::shimless_rma::mojom::FinalizationError;
using ProtoFinalizationError = rmad::FinalizeStatus::Error;

using MojomUpdateRoFirmwareStatus =
    ash::shimless_rma::mojom::UpdateRoFirmwareStatus;
using ProtoUpdateRoFirmwaretatus = rmad::UpdateRoFirmwareStatus;

using MojomShutdownMethod = ash::shimless_rma::mojom::ShutdownMethod;
using ProtoShutdownMethod = rmad::RepairCompleteState::ShutdownMethod;

using MojomFeatureLevel = ash::shimless_rma::mojom::FeatureLevel;
using ProtoFeatureLevel = rmad::UpdateDeviceInfoState::FeatureLevel;

}  // namespace

// The rmad state does not map 1:1 with UI app state, the UI handles more states
// such as selecting wifi network and updating Chrome OS.
// Because some states do not exist in rmad a type mapping build rule cannot be
// added to automate conversion and ToMojom(rmad::RmadState::StateCase) must be
// used directly.
// FromMojom is not needed as state is not passed back from javascript.
MojomRmaState EnumTraits<MojomRmaState, ProtoRmadState>::ToMojom(
    ProtoRmadState state) {
  switch (state) {
    case ProtoRmadState::kWelcome:
      return MojomRmaState::kWelcomeScreen;
    case ProtoRmadState::kComponentsRepair:
      return MojomRmaState::kSelectComponents;
    case ProtoRmadState::kDeviceDestination:
      return MojomRmaState::kChooseDestination;
    case ProtoRmadState::kWipeSelection:
      return MojomRmaState::kChooseWipeDevice;
    case ProtoRmadState::kWpDisableMethod:
      return MojomRmaState::kChooseWriteProtectDisableMethod;
    case ProtoRmadState::kWpDisableRsu:
      return MojomRmaState::kEnterRSUWPDisableCode;
    case ProtoRmadState::kWpDisablePhysical:
      return MojomRmaState::kWaitForManualWPDisable;
    case ProtoRmadState::kWpDisableComplete:
      return MojomRmaState::kWPDisableComplete;
    case ProtoRmadState::kUpdateRoFirmware:
      return MojomRmaState::kUpdateRoFirmware;
    case ProtoRmadState::kRestock:
      return MojomRmaState::kRestock;
    case ProtoRmadState::kUpdateDeviceInfo:
      return MojomRmaState::kUpdateDeviceInformation;
    case ProtoRmadState::kCheckCalibration:
      return MojomRmaState::kCheckCalibration;
    case ProtoRmadState::kSetupCalibration:
      return MojomRmaState::kSetupCalibration;
    case ProtoRmadState::kRunCalibration:
      return MojomRmaState::kRunCalibration;
    case ProtoRmadState::kProvisionDevice:
      return MojomRmaState::kProvisionDevice;
    case ProtoRmadState::kWpEnablePhysical:
      return MojomRmaState::kWaitForManualWPEnable;
    case ProtoRmadState::kFinalize:
      return MojomRmaState::kFinalize;
    case ProtoRmadState::kRepairComplete:
      return MojomRmaState::kRepairComplete;

    case ProtoRmadState::STATE_NOT_SET:
    default:
      return MojomRmaState::kUnknown;
  }
}

// static
MojomRmadErrorCode EnumTraits<MojomRmadErrorCode, ProtoRmadErrorCode>::ToMojom(
    ProtoRmadErrorCode error) {
  switch (error) {
    case ProtoRmadErrorCode::RMAD_ERROR_OK:
      return MojomRmadErrorCode::kOk;
    case ProtoRmadErrorCode::RMAD_ERROR_WAIT:
      return MojomRmadErrorCode::kWait;
    case ProtoRmadErrorCode::RMAD_ERROR_EXPECT_REBOOT:
      return MojomRmadErrorCode::kExpectReboot;
    case ProtoRmadErrorCode::RMAD_ERROR_EXPECT_SHUTDOWN:
      return MojomRmadErrorCode::kExpectShutdown;
    case ProtoRmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED:
      return MojomRmadErrorCode::kRmaNotRequired;
    case ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_MISSING:
      return MojomRmadErrorCode::kStateHandlerMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_INITIALIZATION_FAILED:
      return MojomRmadErrorCode::kStateHandlerInitializationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_REQUEST_INVALID:
      return MojomRmadErrorCode::kRequestInvalid;
    case ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_MISSING:
      return MojomRmadErrorCode::kRequestArgsMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_VIOLATION:
      return MojomRmadErrorCode::kRequestArgsViolation;
    case ProtoRmadErrorCode::RMAD_ERROR_TRANSITION_FAILED:
      return MojomRmadErrorCode::kTransitionFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_ABORT_FAILED:
      return MojomRmadErrorCode::kAbortFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_MISSING_COMPONENT:
      return MojomRmadErrorCode::kMissingComponent;
    case ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_NO_CHALLENGE:
      return MojomRmadErrorCode::kWriteProtectDisableRsuNoChallenge;
    case ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_CODE_INVALID:
      return MojomRmadErrorCode::kWriteProtectDisableRsuCodeInvalid;
    case ProtoRmadErrorCode::
        RMAD_ERROR_WRITE_PROTECT_DISABLE_BATTERY_NOT_DISCONNECTED:
      return MojomRmadErrorCode::kWriteProtectDisableBatteryNotDisconnected;
    case ProtoRmadErrorCode::
        RMAD_ERROR_WRITE_PROTECT_DISABLE_SIGNAL_NOT_DETECTED:
      return MojomRmadErrorCode::kWriteProtectSignalNotDetected;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NO_NETWORK:
      return MojomRmadErrorCode::kReimagingDownloadNoNetwork;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NETWORK_ERROR:
      return MojomRmadErrorCode::kReimagingDownloadNetworkError;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_CANCELLED:
      return MojomRmadErrorCode::kReimagingDownloadCancelled;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_NOT_FOUND:
      return MojomRmadErrorCode::kReimagingUsbNotFound;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_TOO_MANY_FOUND:
      return MojomRmadErrorCode::kReimagingUsbTooManyFound;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_INVALID_IMAGE:
      return MojomRmadErrorCode::kReimagingUsbInvalidImage;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_IMAGING_FAILED:
      return MojomRmadErrorCode::kReimagingImagingFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE:
      return MojomRmadErrorCode::kReimagingUnknownFailure;
    case ProtoRmadErrorCode::RMAD_ERROR_DEVICE_INFO_INVALID:
      return MojomRmadErrorCode::kDeviceInfoInvalid;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_MISSING:
      return MojomRmadErrorCode::kCalibrationComponentMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_STATUS_MISSING:
      return MojomRmadErrorCode::kCalibrationStatusMissing;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_INVALID:
      return MojomRmadErrorCode::kCalibrationComponentInvalid;
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED:
      return MojomRmadErrorCode::kCalibrationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_PROVISIONING_FAILED:
      return MojomRmadErrorCode::kProvisioningFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_POWERWASH_FAILED:
      return MojomRmadErrorCode::kPowerwashFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_FINALIZATION_FAILED:
      return MojomRmadErrorCode::kFinalizationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CANNOT_CONNECT:
      return MojomRmadErrorCode::kLogUploadFtpServerCannotConnect;
    case ProtoRmadErrorCode::
        RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CONNECTION_REJECTED:
      return MojomRmadErrorCode::kLogUploadFtpServerConnectionRejected;
    case ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_TRANSFER_FAILED:
      return MojomRmadErrorCode::kLogUploadFtpServerTransferFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA:
      return MojomRmadErrorCode::kCannotCancelRma;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_GET_LOG:
      return MojomRmadErrorCode::kCannotGetLog;
    case ProtoRmadErrorCode::RMAD_ERROR_DAEMON_INITIALIZATION_FAILED:
      return MojomRmadErrorCode::kDaemonInitializationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_UPDATE_RO_FIRMWARE_FAILED:
      return MojomRmadErrorCode::kUpdateRoFirmwareFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_WP_ENABLED:
      return MojomRmadErrorCode::kWpEnabled;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_WRITE:
      return MojomRmadErrorCode::kCannotWrite;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_SAVE_LOG:
      return MojomRmadErrorCode::kCannotSaveLog;
    case ProtoRmadErrorCode::RMAD_ERROR_CANNOT_RECORD_BROWSER_ACTION:
      return MojomRmadErrorCode::kCannotRecordBrowserAction;
    case ProtoRmadErrorCode::RMAD_ERROR_USB_NOT_FOUND:
      return MojomRmadErrorCode::kUsbNotFound;

    case ProtoRmadErrorCode::RMAD_ERROR_NOT_SET:
    default:
      NOTREACHED();
  }
}

// static
ProtoRmadErrorCode
EnumTraits<MojomRmadErrorCode, ProtoRmadErrorCode>::FromMojom(
    MojomRmadErrorCode error) {
  switch (error) {
    case MojomRmadErrorCode::kOk:
      return ProtoRmadErrorCode::RMAD_ERROR_OK;
    case MojomRmadErrorCode::kWait:
      return ProtoRmadErrorCode::RMAD_ERROR_WAIT;
    case MojomRmadErrorCode::kExpectReboot:
      return ProtoRmadErrorCode::RMAD_ERROR_EXPECT_REBOOT;
    case MojomRmadErrorCode::kExpectShutdown:
      return ProtoRmadErrorCode::RMAD_ERROR_EXPECT_SHUTDOWN;
    case MojomRmadErrorCode::kRmaNotRequired:
      return ProtoRmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED;
    case MojomRmadErrorCode::kStateHandlerMissing:
      return ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_MISSING;
    case MojomRmadErrorCode::kStateHandlerInitializationFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_INITIALIZATION_FAILED;
    case MojomRmadErrorCode::kRequestInvalid:
      return ProtoRmadErrorCode::RMAD_ERROR_REQUEST_INVALID;
    case MojomRmadErrorCode::kRequestArgsMissing:
      return ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_MISSING;
    case MojomRmadErrorCode::kRequestArgsViolation:
      return ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_VIOLATION;
    case MojomRmadErrorCode::kTransitionFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_TRANSITION_FAILED;
    case MojomRmadErrorCode::kAbortFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_ABORT_FAILED;
    case MojomRmadErrorCode::kMissingComponent:
      return ProtoRmadErrorCode::RMAD_ERROR_MISSING_COMPONENT;
    case MojomRmadErrorCode::kWriteProtectDisableRsuNoChallenge:
      return ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_NO_CHALLENGE;
    case MojomRmadErrorCode::kWriteProtectDisableRsuCodeInvalid:
      return ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_CODE_INVALID;
    case MojomRmadErrorCode::kWriteProtectDisableBatteryNotDisconnected:
      return ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_BATTERY_NOT_DISCONNECTED;
    case MojomRmadErrorCode::kWriteProtectSignalNotDetected:
      return ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_SIGNAL_NOT_DETECTED;
    case MojomRmadErrorCode::kReimagingDownloadNoNetwork:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NO_NETWORK;
    case MojomRmadErrorCode::kReimagingDownloadNetworkError:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NETWORK_ERROR;
    case MojomRmadErrorCode::kReimagingDownloadCancelled:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_CANCELLED;
    case MojomRmadErrorCode::kReimagingUsbNotFound:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_NOT_FOUND;
    case MojomRmadErrorCode::kReimagingUsbTooManyFound:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_TOO_MANY_FOUND;
    case MojomRmadErrorCode::kReimagingUsbInvalidImage:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_INVALID_IMAGE;
    case MojomRmadErrorCode::kReimagingImagingFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_IMAGING_FAILED;
    case MojomRmadErrorCode::kReimagingUnknownFailure:
      return ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE;
    case MojomRmadErrorCode::kDeviceInfoInvalid:
      return ProtoRmadErrorCode::RMAD_ERROR_DEVICE_INFO_INVALID;
    case MojomRmadErrorCode::kCalibrationComponentMissing:
      return ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_MISSING;
    case MojomRmadErrorCode::kCalibrationStatusMissing:
      return ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_STATUS_MISSING;
    case MojomRmadErrorCode::kCalibrationComponentInvalid:
      return ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_INVALID;
    case MojomRmadErrorCode::kCalibrationFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED;
    case MojomRmadErrorCode::kProvisioningFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_PROVISIONING_FAILED;
    case MojomRmadErrorCode::kPowerwashFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_POWERWASH_FAILED;
    case MojomRmadErrorCode::kFinalizationFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_FINALIZATION_FAILED;
    case MojomRmadErrorCode::kLogUploadFtpServerCannotConnect:
      return ProtoRmadErrorCode::
          RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CANNOT_CONNECT;
    case MojomRmadErrorCode::kLogUploadFtpServerConnectionRejected:
      return ProtoRmadErrorCode::
          RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CONNECTION_REJECTED;
    case MojomRmadErrorCode::kLogUploadFtpServerTransferFailed:
      return ProtoRmadErrorCode::
          RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_TRANSFER_FAILED;
    case MojomRmadErrorCode::kCannotCancelRma:
      return ProtoRmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA;
    case MojomRmadErrorCode::kCannotGetLog:
      return ProtoRmadErrorCode::RMAD_ERROR_CANNOT_GET_LOG;
    case MojomRmadErrorCode::kDaemonInitializationFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_DAEMON_INITIALIZATION_FAILED;
    case MojomRmadErrorCode::kUpdateRoFirmwareFailed:
      return ProtoRmadErrorCode::RMAD_ERROR_UPDATE_RO_FIRMWARE_FAILED;
    case MojomRmadErrorCode::kWpEnabled:
      return ProtoRmadErrorCode::RMAD_ERROR_WP_ENABLED;
    case MojomRmadErrorCode::kCannotWrite:
      return ProtoRmadErrorCode::RMAD_ERROR_CANNOT_WRITE;
    case MojomRmadErrorCode::kCannotSaveLog:
      return ProtoRmadErrorCode::RMAD_ERROR_CANNOT_SAVE_LOG;
    case MojomRmadErrorCode::kCannotRecordBrowserAction:
      return ProtoRmadErrorCode::RMAD_ERROR_CANNOT_RECORD_BROWSER_ACTION;
    case MojomRmadErrorCode::kUsbNotFound:
      return ProtoRmadErrorCode::RMAD_ERROR_USB_NOT_FOUND;

    case MojomRmadErrorCode::kNotSet:
      NOTREACHED();
  }
  NOTREACHED();
}

MojomOsUpdateOperation
EnumTraits<MojomOsUpdateOperation, ProtoOsUpdateOperation>::ToMojom(
    ProtoOsUpdateOperation operation) {
  switch (operation) {
    case update_engine::IDLE:
      return MojomOsUpdateOperation::kIdle;
    case update_engine::CHECKING_FOR_UPDATE:
      return MojomOsUpdateOperation::kCheckingForUpdate;
    case update_engine::UPDATE_AVAILABLE:
      return MojomOsUpdateOperation::kUpdateAvailable;
    case update_engine::DOWNLOADING:
      return MojomOsUpdateOperation::kDownloading;
    case update_engine::VERIFYING:
      return MojomOsUpdateOperation::kVerifying;
    case update_engine::FINALIZING:
      return MojomOsUpdateOperation::kFinalizing;
    case update_engine::UPDATED_NEED_REBOOT:
      return MojomOsUpdateOperation::kUpdatedNeedReboot;
    case update_engine::REPORTING_ERROR_EVENT:
      return MojomOsUpdateOperation::kReportingErrorEvent;
    case update_engine::ATTEMPTING_ROLLBACK:
      return MojomOsUpdateOperation::kAttemptingRollback;
    case update_engine::DISABLED:
      return MojomOsUpdateOperation::kDisabled;
    case update_engine::NEED_PERMISSION_TO_UPDATE:
      return MojomOsUpdateOperation::kNeedPermissionToUpdate;
    case update_engine::CLEANUP_PREVIOUS_UPDATE:
      return MojomOsUpdateOperation::kCleanupPreviousUpdate;
    case update_engine::UPDATED_BUT_DEFERRED:
      return MojomOsUpdateOperation::kUpdatedButDeferred;
    case update_engine::ERROR:
    case update_engine::Operation_INT_MIN_SENTINEL_DO_NOT_USE_:
    case update_engine::Operation_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
ProtoOsUpdateOperation
EnumTraits<MojomOsUpdateOperation, ProtoOsUpdateOperation>::FromMojom(
    MojomOsUpdateOperation input) {
  switch (input) {
    case MojomOsUpdateOperation::kIdle:
      return update_engine::IDLE;
    case MojomOsUpdateOperation::kCheckingForUpdate:
      return update_engine::CHECKING_FOR_UPDATE;
    case MojomOsUpdateOperation::kUpdateAvailable:
      return update_engine::UPDATE_AVAILABLE;
    case MojomOsUpdateOperation::kDownloading:
      return update_engine::DOWNLOADING;
    case MojomOsUpdateOperation::kVerifying:
      return update_engine::VERIFYING;
    case MojomOsUpdateOperation::kFinalizing:
      return update_engine::FINALIZING;
    case MojomOsUpdateOperation::kUpdatedNeedReboot:
      return update_engine::UPDATED_NEED_REBOOT;
    case MojomOsUpdateOperation::kReportingErrorEvent:
      return update_engine::REPORTING_ERROR_EVENT;
    case MojomOsUpdateOperation::kAttemptingRollback:
      return update_engine::ATTEMPTING_ROLLBACK;
    case MojomOsUpdateOperation::kDisabled:
      return update_engine::DISABLED;
    case MojomOsUpdateOperation::kNeedPermissionToUpdate:
      return update_engine::NEED_PERMISSION_TO_UPDATE;
    case MojomOsUpdateOperation::kCleanupPreviousUpdate:
      return update_engine::CLEANUP_PREVIOUS_UPDATE;
    case MojomOsUpdateOperation::kUpdatedButDeferred:
      return update_engine::UPDATED_BUT_DEFERRED;
  }
  NOTREACHED();
}

MojomUpdateErrorCode
EnumTraits<MojomUpdateErrorCode, ProtoOsUpdateErrorCode>::ToMojom(
    ProtoOsUpdateErrorCode operation) {
  switch (operation) {
    case ProtoOsUpdateErrorCode::kSuccess:
      return MojomUpdateErrorCode::kSuccess;
    case ProtoOsUpdateErrorCode::kDownloadTransferError:
    case ProtoOsUpdateErrorCode::kOmahaErrorInHTTPResponse:
      return MojomUpdateErrorCode::kDownloadError;
    case ProtoOsUpdateErrorCode::kError:
    case ProtoOsUpdateErrorCode::kOmahaUpdateIgnoredPerPolicy:
    case ProtoOsUpdateErrorCode::kNoUpdate:
      return MojomUpdateErrorCode::kOtherError;
  }
}

// static
ProtoOsUpdateErrorCode
EnumTraits<MojomUpdateErrorCode, ProtoOsUpdateErrorCode>::FromMojom(
    MojomUpdateErrorCode input) {
  switch (input) {
    case MojomUpdateErrorCode::kSuccess:
      return ProtoOsUpdateErrorCode::kSuccess;
    case MojomUpdateErrorCode::kDownloadError:
      return ProtoOsUpdateErrorCode::kDownloadTransferError;
    case MojomUpdateErrorCode::kOtherError:
      return ProtoOsUpdateErrorCode::kError;
  }
}

// static
MojomComponentType EnumTraits<MojomComponentType, ProtoComponentType>::ToMojom(
    ProtoComponentType component) {
  switch (component) {
    case rmad::RmadComponent::RMAD_COMPONENT_AUDIO_CODEC:
      return MojomComponentType::kAudioCodec;
    case rmad::RmadComponent::RMAD_COMPONENT_BATTERY:
      return MojomComponentType::kBattery;
    case rmad::RmadComponent::RMAD_COMPONENT_STORAGE:
      return MojomComponentType::kStorage;
    case rmad::RmadComponent::RMAD_COMPONENT_VPD_CACHED:
      return MojomComponentType::kVpdCached;
    case rmad::RmadComponent::RMAD_COMPONENT_NETWORK:  // Obsolete in M91.
      return MojomComponentType::kNetwork;
    case rmad::RmadComponent::RMAD_COMPONENT_CAMERA:
      return MojomComponentType::kCamera;
    case rmad::RmadComponent::RMAD_COMPONENT_STYLUS:
      return MojomComponentType::kStylus;
    case rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD:
      return MojomComponentType::kTouchpad;
    case rmad::RmadComponent::RMAD_COMPONENT_TOUCHSCREEN:
      return MojomComponentType::kTouchsreen;
    case rmad::RmadComponent::RMAD_COMPONENT_DRAM:
      return MojomComponentType::kDram;
    case rmad::RmadComponent::RMAD_COMPONENT_DISPLAY_PANEL:
      return MojomComponentType::kDisplayPanel;
    case rmad::RmadComponent::RMAD_COMPONENT_CELLULAR:
      return MojomComponentType::kCellular;
    case rmad::RmadComponent::RMAD_COMPONENT_ETHERNET:
      return MojomComponentType::kEthernet;
    case rmad::RmadComponent::RMAD_COMPONENT_WIRELESS:
      return MojomComponentType::kWireless;

    // Additional rmad components.
    case rmad::RmadComponent::RMAD_COMPONENT_SCREEN:
      return MojomComponentType::kScreen;
    case rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER:
      return MojomComponentType::kBaseAccelerometer;
    case rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER:
      return MojomComponentType::kLidAccelerometer;
    case rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE:
      return MojomComponentType::kBaseGyroscope;
    case rmad::RmadComponent::RMAD_COMPONENT_LID_GYROSCOPE:
      return MojomComponentType::kLidGyroscope;

    case rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD:
      return MojomComponentType::kKeyboard;
    case rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON:
      return MojomComponentType::kPowerButton;

    case rmad::RmadComponent::RMAD_COMPONENT_UNKNOWN:
    default:
      NOTREACHED();
  }
}

// static
ProtoComponentType
EnumTraits<MojomComponentType, ProtoComponentType>::FromMojom(
    MojomComponentType component) {
  switch (component) {
    case MojomComponentType::kAudioCodec:
      return rmad::RmadComponent::RMAD_COMPONENT_AUDIO_CODEC;
    case MojomComponentType::kBattery:
      return rmad::RmadComponent::RMAD_COMPONENT_BATTERY;
    case MojomComponentType::kStorage:
      return rmad::RmadComponent::RMAD_COMPONENT_STORAGE;
    case MojomComponentType::kVpdCached:
      return rmad::RmadComponent::RMAD_COMPONENT_VPD_CACHED;
    case MojomComponentType::kNetwork:
      return rmad::RmadComponent::RMAD_COMPONENT_NETWORK;  // Obsolete in M91.
    case MojomComponentType::kCamera:
      return rmad::RmadComponent::RMAD_COMPONENT_CAMERA;
    case MojomComponentType::kStylus:
      return rmad::RmadComponent::RMAD_COMPONENT_STYLUS;
    case MojomComponentType::kTouchpad:
      return rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD;
    case MojomComponentType::kTouchsreen:
      return rmad::RmadComponent::RMAD_COMPONENT_TOUCHSCREEN;
    case MojomComponentType::kDram:
      return rmad::RmadComponent::RMAD_COMPONENT_DRAM;
    case MojomComponentType::kDisplayPanel:
      return rmad::RmadComponent::RMAD_COMPONENT_DISPLAY_PANEL;
    case MojomComponentType::kCellular:
      return rmad::RmadComponent::RMAD_COMPONENT_CELLULAR;
    case MojomComponentType::kEthernet:
      return rmad::RmadComponent::RMAD_COMPONENT_ETHERNET;
    case MojomComponentType::kWireless:
      return rmad::RmadComponent::RMAD_COMPONENT_WIRELESS;

      // Additional rmad components.
    case MojomComponentType::kScreen:
      return rmad::RmadComponent::RMAD_COMPONENT_SCREEN;
    case MojomComponentType::kBaseAccelerometer:
      return rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER;
    case MojomComponentType::kLidAccelerometer:
      return rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER;
    case MojomComponentType::kBaseGyroscope:
      return rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE;
    case MojomComponentType::kLidGyroscope:
      return rmad::RmadComponent::RMAD_COMPONENT_LID_GYROSCOPE;

    case MojomComponentType::kKeyboard:
      return rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD;
    case MojomComponentType::kPowerButton:
      return rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON;

    case MojomComponentType::kComponentUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
MojomComponentRepairState
EnumTraits<MojomComponentRepairState, ProtoComponentRepairState>::ToMojom(
    ProtoComponentRepairState state) {
  switch (state) {
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_ORIGINAL:
      return MojomComponentRepairState::kOriginal;
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_REPLACED:
      return MojomComponentRepairState::kReplaced;
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_MISSING:
      return MojomComponentRepairState::kMissing;
    case rmad::ComponentsRepairState_ComponentRepairStatus::
        RMAD_REPAIR_STATUS_UNKNOWN:
      return MojomComponentRepairState::kRepairUnknown;

    default:
      NOTREACHED();
  }
}

// static
ProtoComponentRepairState
EnumTraits<MojomComponentRepairState, ProtoComponentRepairState>::FromMojom(
    MojomComponentRepairState state) {
  switch (state) {
    case MojomComponentRepairState::kOriginal:
      return rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL;
    case MojomComponentRepairState::kReplaced:
      return rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED;
    case MojomComponentRepairState::kMissing:
      return rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_MISSING;
    case MojomComponentRepairState::kRepairUnknown:
      return rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_UNKNOWN;

    default:
      NOTREACHED();
  }
}

// static
const std::string&
StructTraits<ash::shimless_rma::mojom::ComponentDataView,
             rmad::ComponentsRepairState_ComponentRepairStatus>::
    identifier(
        const rmad::ComponentsRepairState_ComponentRepairStatus& component) {
  return component.identifier();
}

// static
MojomWpDisableAction
EnumTraits<MojomWpDisableAction, ProtoWpDisableAction>::ToMojom(
    ProtoWpDisableAction action) {
  switch (action) {
    case rmad::WriteProtectDisableCompleteState::
        RMAD_WP_DISABLE_SKIPPED_ASSEMBLE_DEVICE:
      return MojomWpDisableAction::kSkippedAssembleDevice;
    case rmad::WriteProtectDisableCompleteState::
        RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE:
      return MojomWpDisableAction::kCompleteAssembleDevice;
    case rmad::WriteProtectDisableCompleteState::
        RMAD_WP_DISABLE_COMPLETE_KEEP_DEVICE_OPEN:
      return MojomWpDisableAction::kCompleteKeepDeviceOpen;
    case rmad::WriteProtectDisableCompleteState::RMAD_WP_DISABLE_COMPLETE_NO_OP:
      return MojomWpDisableAction::kCompleteNoOp;
    case rmad::WriteProtectDisableCompleteState::RMAD_WP_DISABLE_UNKNOWN:
      return MojomWpDisableAction::kUnknown;

    default:
      NOTREACHED();
  }
}

// static
ProtoWpDisableAction
EnumTraits<MojomWpDisableAction, ProtoWpDisableAction>::FromMojom(
    MojomWpDisableAction input) {
  switch (input) {
    case MojomWpDisableAction::kSkippedAssembleDevice:
      return rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_SKIPPED_ASSEMBLE_DEVICE;
    case MojomWpDisableAction::kCompleteAssembleDevice:
      return rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE;
    case MojomWpDisableAction::kCompleteKeepDeviceOpen:
      return rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_COMPLETE_KEEP_DEVICE_OPEN;
    case MojomWpDisableAction::kCompleteNoOp:
      return rmad::WriteProtectDisableCompleteState::
          RMAD_WP_DISABLE_COMPLETE_NO_OP;

    case MojomWpDisableAction::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
MojomProvisioningStatus
EnumTraits<MojomProvisioningStatus, ProtoProvisioningStatus>::ToMojom(
    ProtoProvisioningStatus status) {
  switch (status) {
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS:
      return MojomProvisioningStatus::kInProgress;
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_COMPLETE:
      return MojomProvisioningStatus::kComplete;
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING:
      return MojomProvisioningStatus::kFailedBlocking;
    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_NON_BLOCKING:
      return MojomProvisioningStatus::kFailedNonBlocking;

    case rmad::ProvisionStatus::RMAD_PROVISION_STATUS_UNKNOWN:
    default:
      NOTREACHED();
  }
}

// static
ProtoProvisioningStatus
EnumTraits<MojomProvisioningStatus, ProtoProvisioningStatus>::FromMojom(
    MojomProvisioningStatus status) {
  switch (status) {
    case MojomProvisioningStatus::kInProgress:
      return rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS;
    case MojomProvisioningStatus::kComplete:
      return rmad::ProvisionStatus::RMAD_PROVISION_STATUS_COMPLETE;
    case MojomProvisioningStatus::kFailedBlocking:
      return rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING;
    case MojomProvisioningStatus::kFailedNonBlocking:
      return rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_NON_BLOCKING;
  }
  NOTREACHED();
}

// static
MojomProvisioningError
EnumTraits<MojomProvisioningError, ProtoProvisioningError>::ToMojom(
    ProtoProvisioningError error) {
  switch (error) {
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_UNKNOWN:
      return MojomProvisioningError::kUnknown;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_INTERNAL:
      return MojomProvisioningError::kInternal;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_WP_ENABLED:
      return MojomProvisioningError::kWpEnabled;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CANNOT_READ:
      return MojomProvisioningError::kCannotRead;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CANNOT_WRITE:
      return MojomProvisioningError::kCannotWrite;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_GENERATE_SECRET:
      return MojomProvisioningError::kGenerateSecret;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_MISSING_BASE_ACCELEROMETER:
      return MojomProvisioningError::kMissingBaseAccelerometer;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_MISSING_LID_ACCELEROMETER:
      return MojomProvisioningError::kMissingLidAccelerometer;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_MISSING_BASE_GYROSCOPE:
      return MojomProvisioningError::kMissingBaseGyroscope;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_MISSING_LID_GYROSCOPE:
      return MojomProvisioningError::kMissingLidGyroscope;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CR50:
      return MojomProvisioningError::kCr50;
    case rmad::ProvisionStatus::RMAD_PROVISION_ERROR_GBB:
      return MojomProvisioningError::kGbb;

    default:
      NOTREACHED();
  }
}

// static
ProtoProvisioningError
EnumTraits<MojomProvisioningError, ProtoProvisioningError>::FromMojom(
    MojomProvisioningError error) {
  switch (error) {
    case MojomProvisioningError::kUnknown:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_UNKNOWN;
    case MojomProvisioningError::kInternal:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_INTERNAL;
    case MojomProvisioningError::kWpEnabled:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_WP_ENABLED;
    case MojomProvisioningError::kCannotRead:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CANNOT_READ;
    case MojomProvisioningError::kCannotWrite:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CANNOT_WRITE;
    case MojomProvisioningError::kGenerateSecret:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_GENERATE_SECRET;
    case MojomProvisioningError::kMissingBaseAccelerometer:
      return rmad::ProvisionStatus::
          RMAD_PROVISION_ERROR_MISSING_BASE_ACCELEROMETER;
    case MojomProvisioningError::kMissingLidAccelerometer:
      return rmad::ProvisionStatus::
          RMAD_PROVISION_ERROR_MISSING_LID_ACCELEROMETER;
    case MojomProvisioningError::kMissingBaseGyroscope:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_MISSING_BASE_GYROSCOPE;
    case MojomProvisioningError::kMissingLidGyroscope:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_MISSING_LID_GYROSCOPE;
    case MojomProvisioningError::kCr50:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_CR50;
    case MojomProvisioningError::kGbb:
      return rmad::ProvisionStatus::RMAD_PROVISION_ERROR_GBB;
  }
  NOTREACHED();
}

bool StructTraits<ash::shimless_rma::mojom::ComponentDataView,
                  rmad::ComponentsRepairState_ComponentRepairStatus>::
    Read(ash::shimless_rma::mojom::ComponentDataView data,
         rmad::ComponentsRepairState_ComponentRepairStatus* out) {
  rmad::RmadComponent component;
  rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus repair_status;
  std::string identifier;
  if (data.ReadComponent(&component) && data.ReadState(&repair_status) &&
      data.ReadIdentifier(&identifier)) {
    out->set_component(component);
    out->set_repair_status(repair_status);
    out->set_identifier(identifier);
    return true;
  }
  return false;
}

// static
MojomCalibrationInstruction
EnumTraits<MojomCalibrationInstruction, ProtoCalibrationInstruction>::ToMojom(
    ProtoCalibrationInstruction step) {
  switch (step) {
    case ProtoCalibrationInstruction::RMAD_CALIBRATION_INSTRUCTION_UNKNOWN:
      return MojomCalibrationInstruction::kCalibrationInstructionUnknown;
    case ProtoCalibrationInstruction::
        RMAD_CALIBRATION_INSTRUCTION_PLACE_BASE_ON_FLAT_SURFACE:
      return MojomCalibrationInstruction::
          kCalibrationInstructionPlaceBaseOnFlatSurface;
    case ProtoCalibrationInstruction::
        RMAD_CALIBRATION_INSTRUCTION_PLACE_LID_ON_FLAT_SURFACE:
      return MojomCalibrationInstruction::
          kCalibrationInstructionPlaceLidOnFlatSurface;
    case ProtoCalibrationInstruction::
        RMAD_CALIBRATION_INSTRUCTION_NO_NEED_CALIBRATION:
    default:
      NOTREACHED();
  }
}

// static
ProtoCalibrationInstruction
EnumTraits<MojomCalibrationInstruction, ProtoCalibrationInstruction>::FromMojom(
    MojomCalibrationInstruction step) {
  switch (step) {
    case MojomCalibrationInstruction::kCalibrationInstructionUnknown:
      return ProtoCalibrationInstruction::RMAD_CALIBRATION_INSTRUCTION_UNKNOWN;
    case MojomCalibrationInstruction::
        kCalibrationInstructionPlaceBaseOnFlatSurface:
      return ProtoCalibrationInstruction::
          RMAD_CALIBRATION_INSTRUCTION_PLACE_BASE_ON_FLAT_SURFACE;
    case MojomCalibrationInstruction::
        kCalibrationInstructionPlaceLidOnFlatSurface:
      return ProtoCalibrationInstruction::
          RMAD_CALIBRATION_INSTRUCTION_PLACE_LID_ON_FLAT_SURFACE;
  }
  NOTREACHED();
}

// static
MojomCalibrationOverallStatus
EnumTraits<MojomCalibrationOverallStatus, ProtoCalibrationOverallStatus>::
    ToMojom(ProtoCalibrationOverallStatus step) {
  switch (step) {
    case ProtoCalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_COMPLETE:
      return MojomCalibrationOverallStatus::kCalibrationOverallComplete;
    case ProtoCalibrationOverallStatus::
        RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE:
      return MojomCalibrationOverallStatus::
          kCalibrationOverallCurrentRoundComplete;
    case ProtoCalibrationOverallStatus::
        RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_FAILED:
      return MojomCalibrationOverallStatus::
          kCalibrationOverallCurrentRoundFailed;
    case ProtoCalibrationOverallStatus::
        RMAD_CALIBRATION_OVERALL_INITIALIZATION_FAILED:
      return MojomCalibrationOverallStatus::
          kCalibrationOverallInitializationFailed;

    case ProtoCalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_UNKNOWN:
    default:
      NOTREACHED();
  }
}

// static
ProtoCalibrationOverallStatus
EnumTraits<MojomCalibrationOverallStatus, ProtoCalibrationOverallStatus>::
    FromMojom(MojomCalibrationOverallStatus step) {
  switch (step) {
    case MojomCalibrationOverallStatus::kCalibrationOverallComplete:
      return ProtoCalibrationOverallStatus::RMAD_CALIBRATION_OVERALL_COMPLETE;
    case MojomCalibrationOverallStatus::kCalibrationOverallCurrentRoundComplete:
      return ProtoCalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_COMPLETE;
    case MojomCalibrationOverallStatus::kCalibrationOverallCurrentRoundFailed:
      return ProtoCalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_CURRENT_ROUND_FAILED;
    case MojomCalibrationOverallStatus::kCalibrationOverallInitializationFailed:
      return ProtoCalibrationOverallStatus::
          RMAD_CALIBRATION_OVERALL_INITIALIZATION_FAILED;
  }
  NOTREACHED();
}

// static
MojomCalibrationStatus
EnumTraits<MojomCalibrationStatus, ProtoCalibrationStatus>::ToMojom(
    ProtoCalibrationStatus step) {
  switch (step) {
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING:
      return MojomCalibrationStatus::kCalibrationWaiting;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS:
      return MojomCalibrationStatus::kCalibrationInProgress;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_COMPLETE:
      return MojomCalibrationStatus::kCalibrationComplete;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_FAILED:
      return MojomCalibrationStatus::kCalibrationFailed;
    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP:
      return MojomCalibrationStatus::kCalibrationSkip;

    case rmad::CalibrationComponentStatus::RMAD_CALIBRATION_UNKNOWN:
    default:
      NOTREACHED();
  }
}

// static
ProtoCalibrationStatus
EnumTraits<MojomCalibrationStatus, ProtoCalibrationStatus>::FromMojom(
    MojomCalibrationStatus step) {
  switch (step) {
    case MojomCalibrationStatus::kCalibrationWaiting:
      return rmad::CalibrationComponentStatus::RMAD_CALIBRATION_WAITING;
    case MojomCalibrationStatus::kCalibrationInProgress:
      return rmad::CalibrationComponentStatus::RMAD_CALIBRATION_IN_PROGRESS;
    case MojomCalibrationStatus::kCalibrationComplete:
      return rmad::CalibrationComponentStatus::RMAD_CALIBRATION_COMPLETE;
    case MojomCalibrationStatus::kCalibrationFailed:
      return rmad::CalibrationComponentStatus::RMAD_CALIBRATION_FAILED;
    case MojomCalibrationStatus::kCalibrationSkip:
      return rmad::CalibrationComponentStatus::RMAD_CALIBRATION_SKIP;
  }
  NOTREACHED();
}

// static// static
MojomFinalizationStatus
EnumTraits<MojomFinalizationStatus, ProtoFinalizationStatus>::ToMojom(
    ProtoFinalizationStatus step) {
  switch (step) {
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS:
      return MojomFinalizationStatus::kInProgress;
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE:
      return MojomFinalizationStatus::kComplete;
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING:
      return MojomFinalizationStatus::kFailedBlocking;
    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_NON_BLOCKING:
      return MojomFinalizationStatus::kFailedNonBlocking;

    case rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_UNKNOWN:
    default:
      NOTREACHED();
  }
}

ProtoFinalizationStatus
EnumTraits<MojomFinalizationStatus, ProtoFinalizationStatus>::FromMojom(
    MojomFinalizationStatus step) {
  switch (step) {
    case MojomFinalizationStatus::kInProgress:
      return rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS;
    case MojomFinalizationStatus::kComplete:
      return rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE;
    case MojomFinalizationStatus::kFailedBlocking:
      return rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING;
    case MojomFinalizationStatus::kFailedNonBlocking:
      return rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_NON_BLOCKING;
  }
  NOTREACHED();
}

// static
MojomFinalizationError
EnumTraits<MojomFinalizationError, ProtoFinalizationError>::ToMojom(
    ProtoFinalizationError error) {
  switch (error) {
    case rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_UNKNOWN:
      return MojomFinalizationError::kUnknown;
    case rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_INTERNAL:
      return MojomFinalizationError::kInternal;
    case rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CANNOT_ENABLE_HWWP:
      return MojomFinalizationError::kCannotEnableHardwareWp;
    case rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CANNOT_ENABLE_SWWP:
      return MojomFinalizationError::kCannotEnableSoftwareWp;
    case rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CR50:
      return MojomFinalizationError::kCr50;
    case rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_GBB:
      return MojomFinalizationError::kGbb;

    default:
      NOTREACHED();
  }
}

// static
ProtoFinalizationError
EnumTraits<MojomFinalizationError, ProtoFinalizationError>::FromMojom(
    MojomFinalizationError error) {
  switch (error) {
    case MojomFinalizationError::kUnknown:
      return rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_UNKNOWN;
    case MojomFinalizationError::kInternal:
      return rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_INTERNAL;
    case MojomFinalizationError::kCannotEnableHardwareWp:
      return rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CANNOT_ENABLE_HWWP;
    case MojomFinalizationError::kCannotEnableSoftwareWp:
      return rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CANNOT_ENABLE_SWWP;
    case MojomFinalizationError::kCr50:
      return rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_CR50;
    case MojomFinalizationError::kGbb:
      return rmad::FinalizeStatus::RMAD_FINALIZE_ERROR_GBB;
  }
  NOTREACHED();
}

bool StructTraits<ash::shimless_rma::mojom::CalibrationComponentStatusDataView,
                  rmad::CalibrationComponentStatus>::
    Read(ash::shimless_rma::mojom::CalibrationComponentStatusDataView data,
         rmad::CalibrationComponentStatus* out) {
  rmad::RmadComponent component;
  rmad::CalibrationComponentStatus::CalibrationStatus status;
  if (data.ReadComponent(&component) && data.ReadStatus(&status)) {
    out->set_component(component);
    out->set_status(status);
    out->set_progress(data.progress());
    return true;
  }
  return false;
}

// static// static
MojomUpdateRoFirmwareStatus
EnumTraits<MojomUpdateRoFirmwareStatus, ProtoUpdateRoFirmwaretatus>::ToMojom(
    ProtoUpdateRoFirmwaretatus step) {
  switch (step) {
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_WAIT_USB:
      return MojomUpdateRoFirmwareStatus::kWaitUsb;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_FILE_NOT_FOUND:
      return MojomUpdateRoFirmwareStatus::kFileNotFound;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING:
      return MojomUpdateRoFirmwareStatus::kDownloading;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING:
      return MojomUpdateRoFirmwareStatus::kUpdating;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING:
      return MojomUpdateRoFirmwareStatus::kRebooting;
    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_COMPLETE:
      return MojomUpdateRoFirmwareStatus::kComplete;

    case ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_UNKNOWN:
    default:
      NOTREACHED();
  }
}

ProtoUpdateRoFirmwaretatus
EnumTraits<MojomUpdateRoFirmwareStatus, ProtoUpdateRoFirmwaretatus>::FromMojom(
    MojomUpdateRoFirmwareStatus step) {
  switch (step) {
    case MojomUpdateRoFirmwareStatus::kWaitUsb:
      return ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_WAIT_USB;
    case MojomUpdateRoFirmwareStatus::kFileNotFound:
      return ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_FILE_NOT_FOUND;
    case MojomUpdateRoFirmwareStatus::kDownloading:
      return ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING;
    case MojomUpdateRoFirmwareStatus::kUpdating:
      return ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING;
    case MojomUpdateRoFirmwareStatus::kRebooting:
      return ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING;
    case MojomUpdateRoFirmwareStatus::kComplete:
      return ProtoUpdateRoFirmwaretatus::RMAD_UPDATE_RO_FIRMWARE_COMPLETE;
    case MojomUpdateRoFirmwareStatus::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
MojomShutdownMethod
EnumTraits<MojomShutdownMethod, ProtoShutdownMethod>::ToMojom(
    ProtoShutdownMethod shutdown_method) {
  switch (shutdown_method) {
    case rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_UNKNOWN:
      return MojomShutdownMethod::kUnknown;
    case rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_REBOOT:
      return MojomShutdownMethod::kReboot;
    case rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN:
      return MojomShutdownMethod::kShutdown;
    case rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_BATTERY_CUTOFF:
      return MojomShutdownMethod::kBatteryCutoff;

    default:
      NOTREACHED();
  }
}

// static
ProtoShutdownMethod
EnumTraits<MojomShutdownMethod, ProtoShutdownMethod>::FromMojom(
    MojomShutdownMethod shutdown_method) {
  switch (shutdown_method) {
    case MojomShutdownMethod::kUnknown:
      return rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_UNKNOWN;
    case MojomShutdownMethod::kReboot:
      return rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_REBOOT;
    case MojomShutdownMethod::kShutdown:
      return rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN;
    case MojomShutdownMethod::kBatteryCutoff:
      return rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_BATTERY_CUTOFF;
  }
  NOTREACHED();
}

// static
MojomFeatureLevel EnumTraits<MojomFeatureLevel, ProtoFeatureLevel>::ToMojom(
    ProtoFeatureLevel feature_level) {
  switch (feature_level) {
    case rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_UNSUPPORTED:
      return MojomFeatureLevel::kRmadFeatureLevelUnsupported;
    case rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_UNKNOWN:
      return MojomFeatureLevel::kRmadFeatureLevelUnknown;
    case rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_0:
      return MojomFeatureLevel::kRmadFeatureLevel0;
    case rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_1:
      return MojomFeatureLevel::kRmadFeatureLevel1;

    default:
      NOTREACHED();
  }
}

// static
ProtoFeatureLevel EnumTraits<MojomFeatureLevel, ProtoFeatureLevel>::FromMojom(
    MojomFeatureLevel feature_level) {
  switch (feature_level) {
    case MojomFeatureLevel::kRmadFeatureLevelUnsupported:
      return rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_UNSUPPORTED;
    case MojomFeatureLevel::kRmadFeatureLevelUnknown:
      return rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_UNKNOWN;
    case MojomFeatureLevel::kRmadFeatureLevel0:
      return rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_0;
    case MojomFeatureLevel::kRmadFeatureLevel1:
      return rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_1;
  }
  NOTREACHED();
}

}  // namespace mojo
