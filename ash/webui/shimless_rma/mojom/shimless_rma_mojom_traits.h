// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_
#define ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_

#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::shimless_rma::mojom::State, rmad::RmadState::StateCase> {
  static ash::shimless_rma::mojom::State ToMojom(
      rmad::RmadState::StateCase key_status);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::RmadErrorCode,
                  rmad::RmadErrorCode> {
  static ash::shimless_rma::mojom::RmadErrorCode ToMojom(
      rmad::RmadErrorCode key_status);

  static rmad::RmadErrorCode FromMojom(
      ash::shimless_rma::mojom::RmadErrorCode input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ComponentType,
                  rmad::RmadComponent> {
  static ash::shimless_rma::mojom::ComponentType ToMojom(
      rmad::RmadComponent key_status);

  static rmad::RmadComponent FromMojom(
      ash::shimless_rma::mojom::ComponentType input);
};

template <>
struct EnumTraits<
    ash::shimless_rma::mojom::ComponentRepairStatus,
    rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus> {
  static ash::shimless_rma::mojom::ComponentRepairStatus ToMojom(
      rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus
          key_status);

  static rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus
  FromMojom(ash::shimless_rma::mojom::ComponentRepairStatus input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::OsUpdateOperation,
                  update_engine::Operation> {
  static ash::shimless_rma::mojom::OsUpdateOperation ToMojom(
      update_engine::Operation operation);

  static update_engine::Operation FromMojom(
      ash::shimless_rma::mojom::OsUpdateOperation input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::UpdateErrorCode,
                  update_engine::ErrorCode> {
  static ash::shimless_rma::mojom::UpdateErrorCode ToMojom(
      update_engine::ErrorCode error_code);

  static update_engine::ErrorCode FromMojom(
      ash::shimless_rma::mojom::UpdateErrorCode input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::WriteProtectDisableCompleteAction,
                  rmad::WriteProtectDisableCompleteState::Action> {
  static ash::shimless_rma::mojom::WriteProtectDisableCompleteAction ToMojom(
      rmad::WriteProtectDisableCompleteState::Action action);

  static rmad::WriteProtectDisableCompleteState::Action FromMojom(
      ash::shimless_rma::mojom::WriteProtectDisableCompleteAction input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ProvisioningStatus,
                  rmad::ProvisionStatus::Status> {
  static ash::shimless_rma::mojom::ProvisioningStatus ToMojom(
      rmad::ProvisionStatus::Status key_status);

  static rmad::ProvisionStatus::Status FromMojom(
      ash::shimless_rma::mojom::ProvisioningStatus input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ProvisioningError,
                  rmad::ProvisionStatus::Error> {
  static ash::shimless_rma::mojom::ProvisioningError ToMojom(
      rmad::ProvisionStatus::Error error);

  static rmad::ProvisionStatus::Error FromMojom(
      ash::shimless_rma::mojom::ProvisioningError input);
};

template <>
class StructTraits<ash::shimless_rma::mojom::ComponentDataView,
                   rmad::ComponentsRepairState_ComponentRepairStatus> {
 public:
  static rmad::RmadComponent component(
      const rmad::ComponentsRepairState_ComponentRepairStatus& component) {
    return component.component();
  }

  static rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus state(
      const rmad::ComponentsRepairState_ComponentRepairStatus& component) {
    return component.repair_status();
  }

  static const std::string& identifier(
      const rmad::ComponentsRepairState_ComponentRepairStatus& component);

  static bool Read(ash::shimless_rma::mojom::ComponentDataView data,
                   rmad::ComponentsRepairState_ComponentRepairStatus* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::CalibrationSetupInstruction,
                  rmad::CalibrationSetupInstruction> {
  static ash::shimless_rma::mojom::CalibrationSetupInstruction ToMojom(
      rmad::CalibrationSetupInstruction key_status);

  static rmad::CalibrationSetupInstruction FromMojom(
      ash::shimless_rma::mojom::CalibrationSetupInstruction input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::CalibrationOverallStatus,
                  rmad::CalibrationOverallStatus> {
  static ash::shimless_rma::mojom::CalibrationOverallStatus ToMojom(
      rmad::CalibrationOverallStatus key_status);

  static rmad::CalibrationOverallStatus FromMojom(
      ash::shimless_rma::mojom::CalibrationOverallStatus input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::CalibrationStatus,
                  rmad::CalibrationComponentStatus_CalibrationStatus> {
  static ash::shimless_rma::mojom::CalibrationStatus ToMojom(
      rmad::CalibrationComponentStatus_CalibrationStatus key_status);

  static rmad::CalibrationComponentStatus_CalibrationStatus FromMojom(
      ash::shimless_rma::mojom::CalibrationStatus input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::FinalizationStatus,
                  rmad::FinalizeStatus_Status> {
  static ash::shimless_rma::mojom::FinalizationStatus ToMojom(
      rmad::FinalizeStatus_Status key_status);

  static rmad::FinalizeStatus_Status FromMojom(
      ash::shimless_rma::mojom::FinalizationStatus input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::FinalizationError,
                  rmad::FinalizeStatus::Error> {
  static ash::shimless_rma::mojom::FinalizationError ToMojom(
      rmad::FinalizeStatus::Error error);

  static rmad::FinalizeStatus::Error FromMojom(
      ash::shimless_rma::mojom::FinalizationError input);
};

template <>
class StructTraits<ash::shimless_rma::mojom::CalibrationComponentStatusDataView,
                   rmad::CalibrationComponentStatus> {
 public:
  static rmad::RmadComponent component(
      const rmad::CalibrationComponentStatus& component) {
    return component.component();
  }

  static rmad::CalibrationComponentStatus_CalibrationStatus status(
      const rmad::CalibrationComponentStatus& component) {
    return component.status();
  }

  static double progress(const rmad::CalibrationComponentStatus& component) {
    return component.progress();
  }

  static bool Read(
      ash::shimless_rma::mojom::CalibrationComponentStatusDataView data,
      rmad::CalibrationComponentStatus* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::UpdateRoFirmwareStatus,
                  rmad::UpdateRoFirmwareStatus> {
  static ash::shimless_rma::mojom::UpdateRoFirmwareStatus ToMojom(
      rmad::UpdateRoFirmwareStatus status);

  static rmad::UpdateRoFirmwareStatus FromMojom(
      ash::shimless_rma::mojom::UpdateRoFirmwareStatus input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ShutdownMethod,
                  rmad::RepairCompleteState::ShutdownMethod> {
  static ash::shimless_rma::mojom::ShutdownMethod ToMojom(
      rmad::RepairCompleteState::ShutdownMethod shutdown_method);

  static rmad::RepairCompleteState::ShutdownMethod FromMojom(
      ash::shimless_rma::mojom::ShutdownMethod input);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::FeatureLevel,
                  rmad::UpdateDeviceInfoState::FeatureLevel> {
  static ash::shimless_rma::mojom::FeatureLevel ToMojom(
      rmad::UpdateDeviceInfoState::FeatureLevel feature_level);

  static rmad::UpdateDeviceInfoState::FeatureLevel FromMojom(
      ash::shimless_rma::mojom::FeatureLevel input);
};

}  // namespace mojo

#endif  // ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_
