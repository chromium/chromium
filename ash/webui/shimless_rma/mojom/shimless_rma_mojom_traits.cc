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
using ProtoComponentType = rmad::ComponentRepairState::Component;

using MojomComponentRepairState =
    ash::shimless_rma::mojom::ComponentRepairState;
using ProtoComponentRepairState = rmad::ComponentRepairState::RepairState;

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
      return MojomRmaState::kUnknown;
  }
  NOTREACHED();
  return MojomRmaState::kUnknown;
}

// static
bool EnumTraits<MojomRmaState, ProtoRmadState>::FromMojom(MojomRmaState state,
                                                          ProtoRmadState* out) {
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
  NOTREACHED();
  return false;
}

}  // namespace mojo
