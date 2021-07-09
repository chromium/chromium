// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for any definitions that have to be kept in sync between rmad.proto and
// shimless_rma.mojom.
// Currently enum tests cannot catch new states being added to the end of
// the list in rmad.proto because protos do not generate a 'max value' enum.

#include <map>

#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"
#include "base/containers/fixed_flat_map.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace shimless_rma {
namespace {

template <typename MojoEnum, typename ProtoEnum, size_t N>
void TestProtoToMojo(
    const base::fixed_flat_map<MojoEnum, ProtoEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size(), static_cast<size_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    EXPECT_EQ(
        enum_pair.first,
        (mojo::EnumTraits<MojoEnum, ProtoEnum>::ToMojom(enum_pair.second)))
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

template <typename MojoEnum, typename ProtoEnum, size_t N>
void TestMojoToProto(
    const base::fixed_flat_map<MojoEnum, ProtoEnum, N>& enums) {
  // The mojo enum is not sparse.
  EXPECT_EQ(enums.size(), static_cast<uint32_t>(MojoEnum::kMaxValue));

  for (auto enum_pair : enums) {
    ProtoEnum mojo_to_proto;
    EXPECT_TRUE((mojo::EnumTraits<MojoEnum, ProtoEnum>::FromMojom(
        enum_pair.first, &mojo_to_proto)));
    EXPECT_EQ(mojo_to_proto, enum_pair.second)
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

}  // namespace

class ShimlessRmaMojoToProtoTest : public testing::Test {
 public:
  ShimlessRmaMojoToProtoTest() = default;

  ~ShimlessRmaMojoToProtoTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ShimlessRmaMojoToProtoTest, StatesMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::RmaState,
                                                rmad::RmadState::StateCase>(
      {{mojom::RmaState::kWelcomeScreen, rmad::RmadState::kWelcome},
       {mojom::RmaState::kConfigureNetwork, rmad::RmadState::kSelectNetwork},
       {mojom::RmaState::kUpdateChrome, rmad::RmadState::kUpdateChrome},
       {mojom::RmaState::kSelectComponents, rmad::RmadState::kComponentsRepair},
       {mojom::RmaState::kChooseDestination,
        rmad::RmadState::kDeviceDestination},
       {mojom::RmaState::kChooseWriteProtectDisableMethod,
        rmad::RmadState::kWpDisableMethod},
       {mojom::RmaState::kEnterRSUWPDisableCode,
        rmad::RmadState::kWpDisableRsu},
       {mojom::RmaState::kWaitForManualWPDisable,
        rmad::RmadState::kWpDisablePhysical},
       {mojom::RmaState::kWPDisableComplete,
        rmad::RmadState::kWpDisableComplete},
       {mojom::RmaState::kChooseFirmwareReimageMethod,
        rmad::RmadState::kUpdateRoFirmware},
       {mojom::RmaState::kRestock, rmad::RmadState::kRestock},
       {mojom::RmaState::kUpdateDeviceInformation,
        rmad::RmadState::kUpdateDeviceInfo},
       {mojom::RmaState::kCalibrateComponents,
        rmad::RmadState::kCalibrateComponents},
       {mojom::RmaState::kProvisionDevice, rmad::RmadState::kProvisionDevice},
       {mojom::RmaState::kWaitForManualWPEnable,
        rmad::RmadState::kWpEnablePhysical},
       {mojom::RmaState::kRepairComplete, rmad::RmadState::kFinalize}});

  // rmad::RmadState::STATE_NOT_SET is used when RMA is not active so the
  // toMojo conversion is reachable, unlike other enums.
  EXPECT_EQ(static_cast<int32_t>(mojom::RmaState::kUnknown), 0);
  EXPECT_EQ(static_cast<int32_t>(rmad::RmadState::STATE_NOT_SET), 0);
  // This test hits a NOTREACHED so it is a release mode only test.
  EXPECT_EQ(
      mojom::RmaState::kUnknown,
      (mojo::EnumTraits<mojom::RmaState, rmad::RmadState::StateCase>::ToMojom(
          rmad::RmadState::STATE_NOT_SET)));
  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, ErrorsMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::RmadErrorCode,
                                                rmad::RmadErrorCode>(
      {{mojom::RmadErrorCode::kOk, rmad::RmadErrorCode::RMAD_ERROR_OK},
       {mojom::RmadErrorCode::KWait, rmad::RmadErrorCode::RMAD_ERROR_WAIT},
       {mojom::RmadErrorCode::KNeedReboot,
        rmad::RmadErrorCode::RMAD_ERROR_NEED_REBOOT},
       {mojom::RmadErrorCode::kRmaNotRequired,
        rmad::RmadErrorCode::RMAD_ERROR_RMA_NOT_REQUIRED},
       {mojom::RmadErrorCode::kStateHandlerMissing,
        rmad::RmadErrorCode::RMAD_ERROR_STATE_HANDLER_MISSING},
       {mojom::RmadErrorCode::kStateHandlerInitializationFailed,
        rmad::RmadErrorCode::RMAD_ERROR_STATE_HANDLER_INITIALIZATION_FAILED},
       {mojom::RmadErrorCode::kRequestInvalid,
        rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID},
       {mojom::RmadErrorCode::kRequestArgsMissing,
        rmad::RmadErrorCode::RMAD_ERROR_REQUEST_ARGS_MISSING},
       {mojom::RmadErrorCode::kRequestArgsViolation,
        rmad::RmadErrorCode::RMAD_ERROR_REQUEST_ARGS_VIOLATION},
       {mojom::RmadErrorCode::kTransitionFailed,
        rmad::RmadErrorCode::RMAD_ERROR_TRANSITION_FAILED},
       {mojom::RmadErrorCode::kAbortFailed,
        rmad::RmadErrorCode::RMAD_ERROR_ABORT_FAILED},
       {mojom::RmadErrorCode::kMissingComponent,
        rmad::RmadErrorCode::RMAD_ERROR_MISSING_COMPONENT},
       {mojom::RmadErrorCode::kWriteProtectDisableRsuNoChallenge,
        rmad::RmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_NO_CHALLENGE},
       {mojom::RmadErrorCode::kWriteProtectDisableRsuCodeInvalid,
        rmad::RmadErrorCode::RMAD_ERROR_WRITE_PROTECT_DISABLE_RSU_CODE_INVALID},
       {mojom::RmadErrorCode::kWriteProtectDisableBatteryNotDisconnected,
        rmad::RmadErrorCode::
            RMAD_ERROR_WRITE_PROTECT_DISABLE_BATTERY_NOT_DISCONNECTED},
       {mojom::RmadErrorCode::kWriteProtectSignalNotDetected,
        rmad::RmadErrorCode::
            RMAD_ERROR_WRITE_PROTECT_DISABLE_SIGNAL_NOT_DETECTED},
       {mojom::RmadErrorCode::kReimagingDownloadNoNetwork,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NO_NETWORK},
       {mojom::RmadErrorCode::kReimagingDownloadNetworkError,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_NETWORK_ERROR},
       {mojom::RmadErrorCode::kReimagingDownloadCancelled,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_DOWNLOAD_CANCELLED},
       {mojom::RmadErrorCode::kReimagingUsbNotFound,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_USB_NOT_FOUND},
       {mojom::RmadErrorCode::kReimagingUsbTooManyFound,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_USB_TOO_MANY_FOUND},
       {mojom::RmadErrorCode::kReimagingUsbInvalidImage,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_USB_INVALID_IMAGE},
       {mojom::RmadErrorCode::kReimagingImagingFailed,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_IMAGING_FAILED},
       {mojom::RmadErrorCode::kReimagingUnknownFailure,
        rmad::RmadErrorCode::RMAD_ERROR_REIMAGING_UNKNOWN_FAILURE},
       {mojom::RmadErrorCode::kDeviceInfoInvalid,
        rmad::RmadErrorCode::RMAD_ERROR_DEVICE_INFO_INVALID},
       {mojom::RmadErrorCode::kCalibrationFailed,
        rmad::RmadErrorCode::RMAD_ERROR_CALIBRATION_FAILED},
       {mojom::RmadErrorCode::kProvisioningFailed,
        rmad::RmadErrorCode::RMAD_ERROR_PROVISIONING_FAILED},
       {mojom::RmadErrorCode::kPowerwashFailed,
        rmad::RmadErrorCode::RMAD_ERROR_POWERWASH_FAILED},
       {mojom::RmadErrorCode::kFinalizationFailed,
        rmad::RmadErrorCode::RMAD_ERROR_FINALIZATION_FAILED},
       {mojom::RmadErrorCode::kLogUploadFtpServerCannotConnect,
        rmad::RmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CANNOT_CONNECT},
       {mojom::RmadErrorCode::kLogUploadFtpServerConnectionRejected,
        rmad::RmadErrorCode::
            RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_CONNECTION_REJECTED},
       {mojom::RmadErrorCode::kLogUploadFtpServerTransferFailed,
        rmad::RmadErrorCode::RMAD_ERROR_LOG_UPLOAD_FTP_SERVER_TRANSFER_FAILED},
       {mojom::RmadErrorCode::kCannotCancelRma,
        rmad::RmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, RepairComponentsMatch) {
  constexpr auto enums =
      base::MakeFixedFlatMap<mojom::ComponentType,
                             rmad::ComponentRepairState::Component>(
          {{mojom::ComponentType::kMainboardRework,
            rmad::ComponentRepairState::RMAD_COMPONENT_MAINBOARD_REWORK},
           {mojom::ComponentType::kKeyboard,
            rmad::ComponentRepairState::RMAD_COMPONENT_KEYBOARD},
           {mojom::ComponentType::kScreen,
            rmad::ComponentRepairState::RMAD_COMPONENT_SCREEN},
           {mojom::ComponentType::kTrackpad,
            rmad::ComponentRepairState::RMAD_COMPONENT_TRACKPAD},
           {mojom::ComponentType::kPowerButton,
            rmad::ComponentRepairState::RMAD_COMPONENT_POWER_BUTTON},
           {mojom::ComponentType::kThumbReader,
            rmad::ComponentRepairState::RMAD_COMPONENT_THUMB_READER}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, RepairStatesMatch) {
  constexpr auto enums =
      base::MakeFixedFlatMap<mojom::ComponentRepairState,
                             rmad::ComponentRepairState::RepairState>(
          {{mojom::ComponentRepairState::kOriginal,
            rmad::ComponentRepairState::RMAD_REPAIR_ORIGINAL},
           {mojom::ComponentRepairState::kReplaced,
            rmad::ComponentRepairState::RMAD_REPAIR_REPLACED},
           {mojom::ComponentRepairState::kMissing,
            rmad::ComponentRepairState::RMAD_REPAIR_MISSING}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, CalibrationComponentsMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<
      mojom::CalibrationComponent,
      rmad::CalibrateComponentsState::CalibrationComponent>(
      {{mojom::CalibrationComponent::kAccelerometer,
        rmad::CalibrateComponentsState::
            RMAD_CALIBRATION_COMPONENT_ACCELEROMETER}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, ProvisioningStepsMatch) {
  constexpr auto enums =
      base::MakeFixedFlatMap<mojom::ProvisioningStep,
                             rmad::ProvisionDeviceState::ProvisioningStep>(
          {{mojom::ProvisioningStep::kInProgress,
            rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_IN_PROGRESS},
           {mojom::ProvisioningStep::kProvisioningComplete,
            rmad::ProvisionDeviceState::RMAD_PROVISIONING_STEP_COMPLETE}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

}  // namespace shimless_rma
}  // namespace ash
