// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"

#include "base/notreached.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {
namespace {

using MojomRmaState = ash::shimless_rma::mojom::RmaState;
using ProtoRmadState = rmad::RmadState::StateCase;

using MojomRmadErrorCode = ash::shimless_rma::mojom::RmadErrorCode;
using ProtoRmadErrorCode = rmad::RmadErrorCode;

using MojomComponentType = ash::shimless_rma::mojom::ComponentType;
using ProtoComponentType = rmad::RmadComponent;

using MojomComponentRepairState =
    ash::shimless_rma::mojom::ComponentRepairStatus;
using ProtoComponentRepairState =
    rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus;

using MojomCalibrationComponent =
    ash::shimless_rma::mojom::CalibrationComponent;
using ProtoCalibrationComponent =
    rmad::CheckCalibrationState::CalibrationStatus::Component;

using MojomProvisioningStep = ash::shimless_rma::mojom::ProvisioningStep;
using ProtoProvisioningStep = rmad::ProvisionDeviceState::ProvisioningStep;
}  // namespace

// static
MojomRmaState EnumTraits<MojomRmaState, ProtoRmadState>::ToMojom(
    ProtoRmadState state) {
  switch (state) {
    case ProtoRmadState::kWelcome:
      return MojomRmaState::kWelcomeScreen;
    case ProtoRmadState::kComponentsRepair:
      return MojomRmaState::kSelectComponents;
    case ProtoRmadState::kDeviceDestination:
      return MojomRmaState::kChooseDestination;
    case ProtoRmadState::kWpDisableMethod:
      return MojomRmaState::kChooseWriteProtectDisableMethod;
    case ProtoRmadState::kWpDisableRsu:
      return MojomRmaState::kEnterRSUWPDisableCode;
    case ProtoRmadState::kWpDisablePhysical:
      return MojomRmaState::kWaitForManualWPDisable;
    case ProtoRmadState::kWpDisableComplete:
      return MojomRmaState::kWPDisableComplete;
    case ProtoRmadState::kUpdateRoFirmware:
      return MojomRmaState::kChooseFirmwareReimageMethod;
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
    case ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED:
      return MojomRmadErrorCode::kCalibrationFailed;
    case ProtoRmadErrorCode::RMAD_ERROR_MISSING_CALIBRATION_COMPONENT:
      return MojomRmadErrorCode::kCalibrationMissingComponent;
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

    case ProtoRmadErrorCode::RMAD_ERROR_NOT_SET:
    default:
      NOTREACHED();
      return MojomRmadErrorCode::kNotSet;
  }
  NOTREACHED();
  return MojomRmadErrorCode::kNotSet;
}

// static
bool EnumTraits<MojomRmadErrorCode, ProtoRmadErrorCode>::FromMojom(
    MojomRmadErrorCode error,
    ProtoRmadErrorCode* out) {
  switch (error) {
    case MojomRmadErrorCode::kOk:
      *out = ProtoRmadErrorCode::RMAD_ERROR_OK;
      return true;
    case MojomRmadErrorCode::kWait:
      *out = ProtoRmadErrorCode::RMAD_ERROR_WAIT;
      return true;
    case MojomRmadErrorCode::kExpectReboot:
      *out = ProtoRmadErrorCode::RMAD_ERROR_EXPECT_REBOOT;
      return true;
    case MojomRmadErrorCode::kExpectShutdown:
      *out = ProtoRmadErrorCode::RMAD_ERROR_EXPECT_SHUTDOWN;
      return true;
    case MojomRmadErrorCode::kRmaNotRequired:
      *out = ProtoRmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED;
      return true;
    case MojomRmadErrorCode::kStateHandlerMissing:
      *out = ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_MISSING;
      return true;
    case MojomRmadErrorCode::kStateHandlerInitializationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_STATE_HANDLER_INITIALIZATION_FAILED;
      return true;
    case MojomRmadErrorCode::kRequestInvalid:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REQUEST_INVALID;
      return true;
    case MojomRmadErrorCode::kRequestArgsMissing:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_MISSING;
      return true;
    case MojomRmadErrorCode::kRequestArgsViolation:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REQUEST_ARGS_VIOLATION;
      return true;
    case MojomRmadErrorCode::kTransitionFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_TRANSITION_FAILED;
      return true;
    case MojomRmadErrorCode::kAbortFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_ABORT_FAILED;
      return true;
    case MojomRmadErrorCode::kMissingComponent:
      *out = ProtoRmadErrorCode::RMAD_ERROR_MISSING_COMPONENT;
      return true;
    case MojomRmadErrorCode::kWriteProtectDisableRsuNoChallenge:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_NO_CHALLENGE;
      return true;
    case MojomRmadErrorCode::kWriteProtectDisableRsuCodeInvalid:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_CODE_INVALID;
      return true;
    case MojomRmadErrorCode::kWriteProtectDisableBatteryNotDisconnected:
      *out = ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_BATTERY_NOT_DISCONNECTED;
      return true;
    case MojomRmadErrorCode::kWriteProtectSignalNotDetected:
      *out = ProtoRmadErrorCode::
          RMAD_ERROR_WRITE_PROTECT_DISABLE_SIGNAL_NOT_DETECTED;
      return true;
    case MojomRmadErrorCode::kReimagingDownloadNoNetwork:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NO_NETWORK;
      return true;
    case MojomRmadErrorCode::kReimagingDownloadNetworkError:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NETWORK_ERROR;
      return true;
    case MojomRmadErrorCode::kReimagingDownloadCancelled:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_CANCELLED;
      return true;
    case MojomRmadErrorCode::kReimagingUsbNotFound:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_NOT_FOUND;
      return true;
    case MojomRmadErrorCode::kReimagingUsbTooManyFound:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_TOO_MANY_FOUND;
      return true;
    case MojomRmadErrorCode::kReimagingUsbInvalidImage:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_USB_INVALID_IMAGE;
      return true;
    case MojomRmadErrorCode::kReimagingImagingFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_IMAGING_FAILED;
      return true;
    case MojomRmadErrorCode::kReimagingUnknownFailure:
      *out = ProtoRmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE;
      return true;
    case MojomRmadErrorCode::kDeviceInfoInvalid:
      *out = ProtoRmadErrorCode::RMAD_ERROR_DEVICE_INFO_INVALID;
      return true;
    case MojomRmadErrorCode::kCalibrationMissingComponent:
      *out = ProtoRmadErrorCode::RMAD_ERROR_MISSING_CALIBRATION_COMPONENT;
      return true;
    case MojomRmadErrorCode::kCalibrationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED;
      return true;
    case MojomRmadErrorCode::kProvisioningFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_PROVISIONING_FAILED;
      return true;
    case MojomRmadErrorCode::kPowerwashFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_POWERWASH_FAILED;
      return true;
    case MojomRmadErrorCode::kFinalizationFailed:
      *out = ProtoRmadErrorCode::RMAD_ERROR_FINALIZATION_FAILED;
      return true;
    case MojomRmadErrorCode::kLogUploadFtpServerCannotConnect:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CANNOT_CONNECT;
      return true;
    case MojomRmadErrorCode::kLogUploadFtpServerConnectionRejected:
      *out = ProtoRmadErrorCode::
          RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CONNECTION_REJECTED;
      return true;
    case MojomRmadErrorCode::kLogUploadFtpServerTransferFailed:
      *out =
          ProtoRmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_TRANSFER_FAILED;
      return true;
    case MojomRmadErrorCode::kCannotCancelRma:
      *out = ProtoRmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA;
      return true;

    case MojomRmadErrorCode::kNotSet:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
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
    case rmad::RmadComponent::RMAD_COMPONENT_GYROSCOPE:
      return MojomComponentType::kGyroscope;
    case rmad::RmadComponent::RMAD_COMPONENT_ACCELEROMETER:
      return MojomComponentType::kAccelerometer;
    case rmad::RmadComponent::RMAD_COMPONENT_SCREEN:
      return MojomComponentType::kScreen;

    case rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD:
      return MojomComponentType::kKeyboard;
    case rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON:
      return MojomComponentType::kPowerButton;

    case rmad::RmadComponent::RMAD_COMPONENT_UNKNOWN:
    default:
      NOTREACHED();
      return MojomComponentType::kComponentUnknown;
  }
  NOTREACHED();
  return MojomComponentType::kComponentUnknown;
}

// static
bool EnumTraits<MojomComponentType, ProtoComponentType>::FromMojom(
    MojomComponentType component,
    ProtoComponentType* out) {
  switch (component) {
    case MojomComponentType::kAudioCodec:
      *out = rmad::RmadComponent::RMAD_COMPONENT_AUDIO_CODEC;
      return true;
    case MojomComponentType::kBattery:
      *out = rmad::RmadComponent::RMAD_COMPONENT_BATTERY;
      return true;
    case MojomComponentType::kStorage:
      *out = rmad::RmadComponent::RMAD_COMPONENT_STORAGE;
      return true;
    case MojomComponentType::kVpdCached:
      *out = rmad::RmadComponent::RMAD_COMPONENT_VPD_CACHED;
      return true;
    case MojomComponentType::kNetwork:
      *out = rmad::RmadComponent::RMAD_COMPONENT_NETWORK;
      return true;  // Obsolete in M91.
    case MojomComponentType::kCamera:
      *out = rmad::RmadComponent::RMAD_COMPONENT_CAMERA;
      return true;
    case MojomComponentType::kStylus:
      *out = rmad::RmadComponent::RMAD_COMPONENT_STYLUS;
      return true;
    case MojomComponentType::kTouchpad:
      *out = rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD;
      return true;
    case MojomComponentType::kTouchsreen:
      *out = rmad::RmadComponent::RMAD_COMPONENT_TOUCHSCREEN;
      return true;
    case MojomComponentType::kDram:
      *out = rmad::RmadComponent::RMAD_COMPONENT_DRAM;
      return true;
    case MojomComponentType::kDisplayPanel:
      *out = rmad::RmadComponent::RMAD_COMPONENT_DISPLAY_PANEL;
      return true;
    case MojomComponentType::kCellular:
      *out = rmad::RmadComponent::RMAD_COMPONENT_CELLULAR;
      return true;
    case MojomComponentType::kEthernet:
      *out = rmad::RmadComponent::RMAD_COMPONENT_ETHERNET;
      return true;
    case MojomComponentType::kWireless:
      *out = rmad::RmadComponent::RMAD_COMPONENT_WIRELESS;
      return true;

      // Additional rmad components.
    case MojomComponentType::kGyroscope:
      *out = rmad::RmadComponent::RMAD_COMPONENT_GYROSCOPE;
      return true;
    case MojomComponentType::kAccelerometer:
      *out = rmad::RmadComponent::RMAD_COMPONENT_ACCELEROMETER;
      return true;
    case MojomComponentType::kScreen:
      *out = rmad::RmadComponent::RMAD_COMPONENT_SCREEN;
      return true;

    case MojomComponentType::kKeyboard:
      *out = rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD;
      return true;
    case MojomComponentType::kPowerButton:
      *out = rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON;
      return true;

    case MojomComponentType::kComponentUnknown:
    default:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
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
    default:
      NOTREACHED();
      return MojomComponentRepairState::kRepairUnknown;
  }
  NOTREACHED();
  return MojomComponentRepairState::kRepairUnknown;
}  // namespace mojo

// static
bool EnumTraits<MojomComponentRepairState, ProtoComponentRepairState>::
    FromMojom(MojomComponentRepairState state, ProtoComponentRepairState* out) {
  switch (state) {
    case MojomComponentRepairState::kOriginal:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_ORIGINAL;
      return true;
    case MojomComponentRepairState::kReplaced:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_REPLACED;
      return true;
    case MojomComponentRepairState::kMissing:
      *out = rmad::ComponentsRepairState_ComponentRepairStatus::
          RMAD_REPAIR_STATUS_MISSING;
      return true;

    case MojomComponentRepairState::kRepairUnknown:
    default:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}  // namespace mojo

// static
MojomCalibrationComponent
EnumTraits<MojomCalibrationComponent, ProtoCalibrationComponent>::ToMojom(
    ProtoCalibrationComponent component) {
  switch (component) {
    case rmad::CheckCalibrationState::CalibrationStatus::
        RMAD_CALIBRATION_COMPONENT_ACCELEROMETER:
      return MojomCalibrationComponent::kAccelerometer;
    case rmad::CheckCalibrationState::CalibrationStatus::
        RMAD_CALIBRATION_COMPONENT_GYROSCOPE:
      return MojomCalibrationComponent::kGyroscope;

    case rmad::CheckCalibrationState::CalibrationStatus::
        RMAD_CALIBRATION_COMPONENT_UNKNOWN:
    default:
      NOTREACHED();
      return MojomCalibrationComponent::kCalibrateUnknown;
  }
  NOTREACHED();
  return MojomCalibrationComponent::kCalibrateUnknown;
}

// static
bool EnumTraits<MojomCalibrationComponent, ProtoCalibrationComponent>::
    FromMojom(MojomCalibrationComponent component,
              ProtoCalibrationComponent* out) {
  switch (component) {
    case MojomCalibrationComponent::kAccelerometer:
      *out = rmad::CheckCalibrationState::CalibrationStatus::
          RMAD_CALIBRATION_COMPONENT_ACCELEROMETER;
      return true;
    case MojomCalibrationComponent::kGyroscope:
      *out = rmad::CheckCalibrationState::CalibrationStatus::
          RMAD_CALIBRATION_COMPONENT_GYROSCOPE;
      return true;

    case MojomCalibrationComponent::kCalibrateUnknown:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

// static
MojomProvisioningStep
EnumTraits<MojomProvisioningStep, ProtoProvisioningStep>::ToMojom(
    ProtoProvisioningStep step) {
  switch (step) {
    case rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_IN_PROGRESS:
      return MojomProvisioningStep::kInProgress;
    case rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_COMPLETE:
      return MojomProvisioningStep::kProvisioningComplete;

    case rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_UNKNOWN:
    default:
      NOTREACHED();
      return MojomProvisioningStep::kProvisioningUnknown;
  }
  NOTREACHED();
  return MojomProvisioningStep::kProvisioningUnknown;
}

// static
bool EnumTraits<MojomProvisioningStep, ProtoProvisioningStep>::FromMojom(
    MojomProvisioningStep step,
    ProtoProvisioningStep* out) {
  switch (step) {
    case MojomProvisioningStep::kInProgress:
      *out = rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_IN_PROGRESS;
      return true;
    case MojomProvisioningStep::kProvisioningComplete:
      *out = rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_COMPLETE;
      return true;
    case MojomProvisioningStep::kProvisioningUnknown:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

bool StructTraits<ash::shimless_rma::mojom::ComponentDataView,
                  rmad::ComponentsRepairState_ComponentRepairStatus>::
    Read(ash::shimless_rma::mojom::ComponentDataView data,
         rmad::ComponentsRepairState_ComponentRepairStatus* out) {
  rmad::RmadComponent component;
  rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus repair_status;
  if (data.ReadComponent(&component) && data.ReadState(&repair_status)) {
    out->set_component(component);
    out->set_repair_status(repair_status);
    return true;
  }
  return false;
}

}  // namespace mojo
