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

  static bool FromMojom(ash::shimless_rma::mojom::RmaState input,
                        rmad::RmadState::StateCase* out);
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
                  rmad::ComponentRepairState::Component> {
  static ash::shimless_rma::mojom::ComponentType ToMojom(
      rmad::ComponentRepairState::Component key_status);

  static bool FromMojom(ash::shimless_rma::mojom::ComponentType input,
                        rmad::ComponentRepairState::Component* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ComponentRepairState,
                  rmad::ComponentRepairState::RepairState> {
  static ash::shimless_rma::mojom::ComponentRepairState ToMojom(
      rmad::ComponentRepairState::RepairState key_status);

  static bool FromMojom(ash::shimless_rma::mojom::ComponentRepairState input,
                        rmad::ComponentRepairState::RepairState* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::CalibrationComponent,
                  rmad::CalibrateComponentsState::CalibrationComponent> {
  static ash::shimless_rma::mojom::CalibrationComponent ToMojom(
      rmad::CalibrateComponentsState::CalibrationComponent key_status);

  static bool FromMojom(
      ash::shimless_rma::mojom::CalibrationComponent input,
      rmad::CalibrateComponentsState::CalibrationComponent* out);
};

template <>
struct EnumTraits<ash::shimless_rma::mojom::ProvisioningStep,
                  rmad::ProvisionDeviceState::ProvisioningStep> {
  static ash::shimless_rma::mojom::ProvisioningStep ToMojom(
      rmad::ProvisionDeviceState::ProvisioningStep key_status);

  static bool FromMojom(ash::shimless_rma::mojom::ProvisioningStep input,
                        rmad::ProvisionDeviceState::ProvisioningStep* out);
};
}  // namespace mojo

#endif  // ASH_WEBUI_SHIMLESS_RMA_MOJOM_SHIMLESS_RMA_MOJOM_TRAITS_H_
