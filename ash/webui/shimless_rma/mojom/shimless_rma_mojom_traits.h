// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_
#define ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_

#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::shimless_rma::mojom::RmaState,
                  rmad::RmadState::StateCase> {
  static ash::shimless_rma::mojom::RmaState ToMojom(
      rmad::RmadState::StateCase key_status);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::RmadErrorCode,
                  rmad::RmadErrorCode> {
  static ash::shimless_rma::mojom::RmadErrorCode ToMojom(
      rmad::RmadErrorCode key_status);

  static bool FromMojom(ash::shimless_rma::mojom::RmadErrorCode input,
                        rmad::RmadErrorCode* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ComponentType,
                  rmad::RmadComponent> {
  static ash::shimless_rma::mojom::ComponentType ToMojom(
      rmad::RmadComponent key_status);

  static bool FromMojom(ash::shimless_rma::mojom::ComponentType input,
                        rmad::RmadComponent* out);
};

template <>
struct EnumTraits<
    ash::shimless_rma::mojom::ComponentRepairStatus,
    rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus> {
  static ash::shimless_rma::mojom::ComponentRepairStatus ToMojom(
      rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus
          key_status);

  static bool FromMojom(
      ash::shimless_rma::mojom::ComponentRepairStatus input,
      rmad::ComponentsRepairState_ComponentRepairStatus_RepairStatus* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::CalibrationComponent,
                  rmad::CheckCalibrationState::CalibrationStatus::Component> {
  static ash::shimless_rma::mojom::CalibrationComponent ToMojom(
      rmad::CheckCalibrationState::CalibrationStatus::Component key_status);

  static bool FromMojom(
      ash::shimless_rma::mojom::CalibrationComponent input,
      rmad::CheckCalibrationState::CalibrationStatus::Component* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ProvisioningStep,
                  rmad::ProvisionDeviceState::ProvisioningStep> {
  static ash::shimless_rma::mojom::ProvisioningStep ToMojom(
      rmad::ProvisionDeviceState::ProvisioningStep key_status);

  static bool FromMojom(ash::shimless_rma::mojom::ProvisioningStep input,
                        rmad::ProvisionDeviceState::ProvisioningStep* out);
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

  static bool Read(ash::shimless_rma::mojom::ComponentDataView data,
                   rmad::ComponentsRepairState_ComponentRepairStatus* out);
};

}  // namespace mojo

#endif  // ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_
