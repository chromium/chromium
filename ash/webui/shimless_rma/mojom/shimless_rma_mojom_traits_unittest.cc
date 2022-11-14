// Copyright 2021 The Chromium Authors
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
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
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
        (mojo::EnumTraits<MojoEnum, ProtoEnum>::ToMojom(enum_pair.second)),
        enum_pair.first)
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
  constexpr auto enums = base::MakeFixedFlatMap<mojom::State,
                                                rmad::RmadState::StateCase>(
      {{mojom::State::kWelcomeScreen, rmad::RmadState::kWelcome},
       {mojom::State::kSelectComponents, rmad::RmadState::kComponentsRepair},
       {mojom::State::kChooseDestination, rmad::RmadState::kDeviceDestination},
       {mojom::State::kChooseWipeDevice, rmad::RmadState::kWipeSelection},
       {mojom::State::kChooseWriteProtectDisableMethod,
        rmad::RmadState::kWpDisableMethod},
       {mojom::State::kEnterRSUWPDisableCode, rmad::RmadState::kWpDisableRsu},
       {mojom::State::kWaitForManualWPDisable,
        rmad::RmadState::kWpDisablePhysical},
       {mojom::State::kWPDisableComplete, rmad::RmadState::kWpDisableComplete},
       {mojom::State::kUpdateRoFirmware, rmad::RmadState::kUpdateRoFirmware},
       {mojom::State::kRestock, rmad::RmadState::kRestock},
       {mojom::State::kUpdateDeviceInformation,
        rmad::RmadState::kUpdateDeviceInfo},
       {mojom::State::kCheckCalibration, rmad::RmadState::kCheckCalibration},
       {mojom::State::kSetupCalibration, rmad::RmadState::kSetupCalibration},
       {mojom::State::kRunCalibration, rmad::RmadState::kRunCalibration},
       {mojom::State::kProvisionDevice, rmad::RmadState::kProvisionDevice},
       {mojom::State::kWaitForManualWPEnable,
        rmad::RmadState::kWpEnablePhysical},
       {mojom::State::kFinalize, rmad::RmadState::kFinalize},
       {mojom::State::kRepairComplete, rmad::RmadState::kRepairComplete}});

  // rmad::RmadState::STATE_NOT_SET is used when RMA is not active so the
  // toMojo conversion is reachable, unlike most other enums.
  EXPECT_EQ(static_cast<int32_t>(mojom::State::kUnknown), 0);
  EXPECT_EQ(static_cast<int32_t>(rmad::RmadState::STATE_NOT_SET), 0);
  // This test hits a NOTREACHED so it is a release mode only test.
  EXPECT_EQ(
      (mojo::EnumTraits<mojom::State, rmad::RmadState::StateCase>::ToMojom(
          rmad::RmadState::STATE_NOT_SET)),
      mojom::State::kUnknown);
  for (auto enum_pair : enums) {
    EXPECT_EQ(
        (mojo::EnumTraits<mojom::State, rmad::RmadState::StateCase>::ToMojom(
            enum_pair.second)),
        enum_pair.first)
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

TEST_F(ShimlessRmaMojoToProtoTest, ErrorsMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::RmadErrorCode,
                                                rmad::RmadErrorCode>(
      {{mojom::RmadErrorCode::kOk, rmad::RmadErrorCode::RMAD_ERROR_OK},
       {mojom::RmadErrorCode::kWait, rmad::RmadErrorCode::RMAD_ERROR_WAIT},
       {mojom::RmadErrorCode::kExpectReboot,
        rmad::RmadErrorCode::RMAD_ERROR_EXPECT_REBOOT},
       {mojom::RmadErrorCode::kExpectShutdown,
        rmad::RmadErrorCode::RMAD_ERROR_EXPECT_SHUTDOWN},
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
       {mojom::RmadErrorCode::kCalibrationComponentMissing,
        rmad::RmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_MISSING},
       {mojom::RmadErrorCode::kCalibrationStatusMissing,
        rmad::RmadErrorCode::RMAD_ERROR_CALIBRATION_STATUS_MISSING},
       {mojom::RmadErrorCode::kCalibrationComponentInvalid,
        rmad::RmadErrorCode::RMAD_ERROR_CALIBRATION_COMPONENT_INVALID},
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
        rmad::RmadErrorCode::RMAD_ERROR_CANNOT_CANCEL_RMA},
       {mojom::RmadErrorCode::kCannotGetLog,
        rmad::RmadErrorCode::RMAD_ERROR_CANNOT_GET_LOG},
       {mojom::RmadErrorCode::kDaemonInitializationFailed,
        rmad::RmadErrorCode::RMAD_ERROR_DAEMON_INITIALIZATION_FAILED},
       {mojom::RmadErrorCode::kUpdateRoFirmwareFailed,
        rmad::RmadErrorCode::RMAD_ERROR_UPDATE_RO_FIRMWARE_FAILED},
       {mojom::RmadErrorCode::kWpEnabled,
        rmad::RmadErrorCode::RMAD_ERROR_WP_ENABLED},
       {mojom::RmadErrorCode::kCannotWrite,
        rmad::RmadErrorCode::RMAD_ERROR_CANNOT_WRITE},
       {mojom::RmadErrorCode::kCannotSaveLog,
        rmad::RmadErrorCode::RMAD_ERROR_CANNOT_SAVE_LOG},
       {mojom::RmadErrorCode::kCannotRecordBrowserAction,
        rmad::RmadErrorCode::RMAD_ERROR_CANNOT_RECORD_BROWSER_ACTION},
       {mojom::RmadErrorCode::kUsbNotFound,
        rmad::RmadErrorCode::RMAD_ERROR_USB_NOT_FOUND}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, RepairComponentsMatch) {
  constexpr auto enums =
      base::MakeFixedFlatMap<mojom::ComponentType, rmad::RmadComponent>(
          {{mojom::ComponentType::kAudioCodec,
            rmad::RmadComponent::RMAD_COMPONENT_AUDIO_CODEC},
           {mojom::ComponentType::kBattery,
            rmad::RmadComponent::RMAD_COMPONENT_BATTERY},
           {mojom::ComponentType::kStorage,
            rmad::RmadComponent::RMAD_COMPONENT_STORAGE},
           {mojom::ComponentType::kVpdCached,
            rmad::RmadComponent::RMAD_COMPONENT_VPD_CACHED},
           {mojom::ComponentType::kNetwork,
            rmad::RmadComponent::RMAD_COMPONENT_NETWORK},  // Obsolete in M91.
           {mojom::ComponentType::kCamera,
            rmad::RmadComponent::RMAD_COMPONENT_CAMERA},
           {mojom::ComponentType::kStylus,
            rmad::RmadComponent::RMAD_COMPONENT_STYLUS},
           {mojom::ComponentType::kTouchpad,
            rmad::RmadComponent::RMAD_COMPONENT_TOUCHPAD},
           {mojom::ComponentType::kTouchsreen,
            rmad::RmadComponent::RMAD_COMPONENT_TOUCHSCREEN},
           {mojom::ComponentType::kDram,
            rmad::RmadComponent::RMAD_COMPONENT_DRAM},
           {mojom::ComponentType::kDisplayPanel,
            rmad::RmadComponent::RMAD_COMPONENT_DISPLAY_PANEL},
           {mojom::ComponentType::kCellular,
            rmad::RmadComponent::RMAD_COMPONENT_CELLULAR},
           {mojom::ComponentType::kEthernet,
            rmad::RmadComponent::RMAD_COMPONENT_ETHERNET},
           {mojom::ComponentType::kWireless,
            rmad::RmadComponent::RMAD_COMPONENT_WIRELESS},
           // Additional rmad components.
           {mojom::ComponentType::kBaseAccelerometer,
            rmad::RmadComponent::RMAD_COMPONENT_BASE_ACCELEROMETER},
           {mojom::ComponentType::kLidAccelerometer,
            rmad::RmadComponent::RMAD_COMPONENT_LID_ACCELEROMETER},
           {mojom::ComponentType::kBaseGyroscope,
            rmad::RmadComponent::RMAD_COMPONENT_BASE_GYROSCOPE},
           {mojom::ComponentType::kLidGyroscope,
            rmad::RmadComponent::RMAD_COMPONENT_LID_GYROSCOPE},
           {mojom::ComponentType::kScreen,
            rmad::RmadComponent::RMAD_COMPONENT_SCREEN},

           // Irrelevant components.
           // TODO(chenghan): Do we really need these?
           {mojom::ComponentType::kKeyboard,
            rmad::RmadComponent::RMAD_COMPONENT_KEYBOARD},
           {mojom::ComponentType::kPowerButton,
            rmad::RmadComponent::RMAD_COMPONENT_POWER_BUTTON}});
  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, RepairStatesMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<
      mojom::ComponentRepairStatus,
      rmad::ComponentsRepairState::ComponentRepairStatus::RepairStatus>(
      {{mojom::ComponentRepairStatus::kOriginal,
        rmad::ComponentsRepairState::ComponentRepairStatus::
            RMAD_REPAIR_STATUS_ORIGINAL},
       {mojom::ComponentRepairStatus::kReplaced,
        rmad::ComponentsRepairState::ComponentRepairStatus::
            RMAD_REPAIR_STATUS_REPLACED},
       {mojom::ComponentRepairStatus::kMissing,
        rmad::ComponentsRepairState::ComponentRepairStatus::
            RMAD_REPAIR_STATUS_MISSING}});
  // RMAD_REPAIR_STATUS_UNKNOWN is used when components are first received from
  // rmad to indicate that repair state has not been set.
  EXPECT_EQ(static_cast<int32_t>(mojom::ComponentRepairStatus::kRepairUnknown),
            0);
  EXPECT_EQ(
      static_cast<int32_t>(rmad::ComponentsRepairState::ComponentRepairStatus::
                               RMAD_REPAIR_STATUS_UNKNOWN),
      0);

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, WriteProtectDisableCompleteActionMatch) {
  constexpr auto enums =
      base::MakeFixedFlatMap<mojom::WriteProtectDisableCompleteAction,
                             rmad::WriteProtectDisableCompleteState::Action>(
          {{mojom::WriteProtectDisableCompleteAction::kSkippedAssembleDevice,
            rmad::WriteProtectDisableCompleteState::
                RMAD_WP_DISABLE_SKIPPED_ASSEMBLE_DEVICE},
           {mojom::WriteProtectDisableCompleteAction::kCompleteAssembleDevice,
            rmad::WriteProtectDisableCompleteState::
                RMAD_WP_DISABLE_COMPLETE_ASSEMBLE_DEVICE},
           {mojom::WriteProtectDisableCompleteAction::kCompleteKeepDeviceOpen,
            rmad::WriteProtectDisableCompleteState::
                RMAD_WP_DISABLE_COMPLETE_KEEP_DEVICE_OPEN},
           {mojom::WriteProtectDisableCompleteAction::kCompleteNoOp,
            rmad::WriteProtectDisableCompleteState::
                RMAD_WP_DISABLE_COMPLETE_NO_OP}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, UpdateRoFirmwareStatusMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::UpdateRoFirmwareStatus,
                                                rmad::UpdateRoFirmwareStatus>(
      {{mojom::UpdateRoFirmwareStatus::kWaitUsb,
        rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_WAIT_USB},
       {mojom::UpdateRoFirmwareStatus::kFileNotFound,
        rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_FILE_NOT_FOUND},
       {mojom::UpdateRoFirmwareStatus::kDownloading,
        rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_DOWNLOADING},
       {mojom::UpdateRoFirmwareStatus::kUpdating,
        rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_UPDATING},
       {mojom::UpdateRoFirmwareStatus::kRebooting,
        rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_REBOOTING},
       {mojom::UpdateRoFirmwareStatus::kComplete,
        rmad::UpdateRoFirmwareStatus::RMAD_UPDATE_RO_FIRMWARE_COMPLETE}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, ProvisioningStatusMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::ProvisioningStatus,
                                                rmad::ProvisionStatus::Status>(
      {{mojom::ProvisioningStatus::kInProgress,
        rmad::ProvisionStatus::RMAD_PROVISION_STATUS_IN_PROGRESS},
       {mojom::ProvisioningStatus::kComplete,
        rmad::ProvisionStatus::RMAD_PROVISION_STATUS_COMPLETE},
       {mojom::ProvisioningStatus::kFailedBlocking,
        rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING},
       {mojom::ProvisioningStatus::kFailedNonBlocking,
        rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_NON_BLOCKING}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

TEST_F(ShimlessRmaMojoToProtoTest, FinalizationStatusMatch) {
  constexpr auto enums = base::MakeFixedFlatMap<mojom::FinalizationStatus,
                                                rmad::FinalizeStatus::Status>(
      {{mojom::FinalizationStatus::kInProgress,
        rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_IN_PROGRESS},
       {mojom::FinalizationStatus::kComplete,
        rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_COMPLETE},
       {mojom::FinalizationStatus::kFailedBlocking,
        rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING},
       {mojom::FinalizationStatus::kFailedNonBlocking,
        rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_NON_BLOCKING}});

  TestProtoToMojo(enums);
  TestMojoToProto(enums);
}

}  // namespace shimless_rma
}  // namespace ash
