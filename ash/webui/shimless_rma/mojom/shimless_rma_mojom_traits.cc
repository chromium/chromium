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
using ProtoComponentType = rmad::ComponentRepairState_Component;

using MojomComponentRepairState =
    ash::shimless_rma::mojom::ComponentRepairState;
using ProtoComponentRepairState = rmad::ComponentRepairState_RepairState;

using MojomCalibrationComponent =
    ash::shimless_rma::mojom::CalibrationComponent;
using ProtoCalibrationComponent =
    rmad::CalibrateComponentsState::CalibrationComponent;

using MojomProvisioningStep = ash::shimless_rma::mojom::ProvisioningStep;
using ProtoProvisioningStep = rmad::ProvisionDeviceState::ProvisioningStep;
}  // namespace

// static
MojomRmaState EnumTraits<MojomRmaState, ProtoRmadState>::ToMojom(
    ProtoRmadState state) {
  switch (state) {
    case ProtoRmadState::kWelcome:
      return MojomRmaState::kWelcomeScreen;
    case ProtoRmadState::kSelectNetwork:
      return MojomRmaState::kConfigureNetwork;
    case ProtoRmadState::kUpdateChrome:
      return MojomRmaState::kUpdateChrome;
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
    case ProtoRmadState::kCalibrateComponents:
      return MojomRmaState::kCalibrateComponents;
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
bool EnumTraits<MojomRmaState, ProtoRmadState>::FromMojom(MojomRmaState state,
                                                          ProtoRmadState* out) {
  switch (state) {
    case MojomRmaState::kWelcomeScreen:
      *out = ProtoRmadState::kWelcome;
      return true;
    case MojomRmaState::kConfigureNetwork:
      *out = ProtoRmadState::kSelectNetwork;
      return true;
    case MojomRmaState::kUpdateChrome:
      *out = ProtoRmadState::kUpdateChrome;
      return true;
    case MojomRmaState::kSelectComponents:
      *out = ProtoRmadState::kComponentsRepair;
      return true;
    case MojomRmaState::kChooseDestination:
      *out = ProtoRmadState::kDeviceDestination;
      return true;
    case MojomRmaState::kChooseWriteProtectDisableMethod:
      *out = ProtoRmadState::kWpDisableMethod;
      return true;
    case MojomRmaState::kEnterRSUWPDisableCode:
      *out = ProtoRmadState::kWpDisableRsu;
      return true;
    case MojomRmaState::kWaitForManualWPDisable:
      *out = ProtoRmadState::kWpDisablePhysical;
      return true;
    case MojomRmaState::kWPDisableComplete:
      *out = ProtoRmadState::kWpDisableComplete;
      return true;
    case MojomRmaState::kChooseFirmwareReimageMethod:
      *out = ProtoRmadState::kUpdateRoFirmware;
      return true;
    case MojomRmaState::kRestock:
      *out = ProtoRmadState::kRestock;
      return true;
    case MojomRmaState::kUpdateDeviceInformation:
      *out = ProtoRmadState::kUpdateDeviceInfo;
      return true;
    case MojomRmaState::kCalibrateComponents:
      *out = ProtoRmadState::kCalibrateComponents;
      return true;
    case MojomRmaState::kProvisionDevice:
      *out = ProtoRmadState::kProvisionDevice;
      return true;
    case MojomRmaState::kWaitForManualWPEnable:
      *out = ProtoRmadState::kWpEnablePhysical;
      return true;
    case MojomRmaState::kRepairComplete:
      *out = ProtoRmadState::kFinalize;
      return true;

    case MojomRmaState::kUnknown:
      *out = ProtoRmadState::STATE_NOT_SET;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
MojomRmadErrorCode EnumTraits<MojomRmadErrorCode, ProtoRmadErrorCode>::ToMojom(
    ProtoRmadErrorCode error) {
  switch (error) {
    case ProtoRmadErrorCode::RMAD_ERROR_OK:
      return MojomRmadErrorCode::kOk;
    case ProtoRmadErrorCode::RMAD_ERROR_WAIT:
      return MojomRmadErrorCode::KWait;
    case ProtoRmadErrorCode::RMAD_ERROR_NEED_REBOOT:
      return MojomRmadErrorCode::KNeedReboot;
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
    case MojomRmadErrorCode::KWait:
      *out = ProtoRmadErrorCode::RMAD_ERROR_WAIT;
      return true;
    case MojomRmadErrorCode::KNeedReboot:
      *out = ProtoRmadErrorCode::RMAD_ERROR_NEED_REBOOT;
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
    case rmad::ComponentRepairState::RMAD_COMPONENT_MAINBOARD_REWORK:
      return MojomComponentType::kMainboardRework;
    case rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD:
      return MojomComponentType::kKeyboard;
    case rmad::ComponentRepairState::RMAD_COMPONENT_SCREEN:
      return MojomComponentType::kScreen;
    case rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD:
      return MojomComponentType::kTrackpad;
    case rmad::ComponentRepairState::RMAD_COMPONENT_POWER_BUTTON:
      return MojomComponentType::kPowerButton;
    case rmad::ComponentRepairState::RMAD_COMPONENT_THUMB_READER:
      return MojomComponentType::kThumbReader;

    case rmad::ComponentRepairState::RMAD_COMPONENT_UNKNOWN:
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
    case MojomComponentType::kMainboardRework:
      *out = rmad::ComponentRepairState::RMAD_COMPONENT_MAINBOARD_REWORK;
      return true;
    case MojomComponentType::kKeyboard:
      *out = rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD;
      return true;
    case MojomComponentType::kScreen:
      *out = rmad::ComponentRepairState::RMAD_COMPONENT_SCREEN;
      return true;
    case MojomComponentType::kTrackpad:
      *out = rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD;
      return true;
    case MojomComponentType::kPowerButton:
      *out = rmad::ComponentRepairState::RMAD_COMPONENT_POWER_BUTTON;
      return true;
    case MojomComponentType::kThumbReader:
      *out = rmad::ComponentRepairState::RMAD_COMPONENT_THUMB_READER;
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
    case rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL:
      return MojomComponentRepairState::kOriginal;
    case rmad::ComponentRepairState::RMAD_REPAIR_REPLACED:
      return MojomComponentRepairState::kReplaced;
    case rmad::ComponentRepairState::RMAD_REPAIR_MISSING:
      return MojomComponentRepairState::kMissing;

    case rmad::ComponentRepairState::RMAD_REPAIR_UNKNOWN:
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
      *out = rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL;
      return true;
    case MojomComponentRepairState::kReplaced:
      *out = rmad::ComponentRepairState::RMAD_REPAIR_REPLACED;
      return true;
    case MojomComponentRepairState::kMissing:
      *out = rmad::ComponentRepairState::RMAD_REPAIR_MISSING;
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
    case rmad::CalibrateComponentsState::
        RMAD_CALIBRATION_COMPONENT_ACCELEROMETER:
      return MojomCalibrationComponent::kAccelerometer;

    case rmad::CalibrateComponentsState::RMAD_CALIBRATION_COMPONENT_UNKNOWN:
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
      *out = rmad::CalibrateComponentsState::
          RMAD_CALIBRATION_COMPONENT_ACCELEROMETER;
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
                  rmad::ComponentRepairState>::
    Read(ash::shimless_rma::mojom::ComponentDataView data,
         rmad::ComponentRepairState* out) {
  rmad::ComponentRepairState_Component name;
  rmad::ComponentRepairState_RepairState repair_state;
  if (data.ReadComponent(&name) && data.ReadState(&repair_state)) {
    out->set_name(name);
    out->set_repair_state(repair_state);
    return true;
  }
  return false;
}

}  // namespace mojo
