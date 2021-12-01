// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

TelemetryApiFunctionBase::TelemetryApiFunctionBase()
    : probe_service_(remote_probe_service_.BindNewPipeAndPassReceiver()) {}
TelemetryApiFunctionBase::~TelemetryApiFunctionBase() = default;

// OsTelemetryGetVpdInfoFunction -----------------------------------------------

OsTelemetryGetVpdInfoFunction::OsTelemetryGetVpdInfoFunction() = default;
OsTelemetryGetVpdInfoFunction::~OsTelemetryGetVpdInfoFunction() = default;

void OsTelemetryGetVpdInfoFunction::RunIfAllowed() {
  auto cb = base::BindOnce(&OsTelemetryGetVpdInfoFunction::OnResult, this);

  remote_probe_service_->ProbeTelemetryInfo(
      {ash::health::mojom::ProbeCategoryEnum::kCachedVpdData}, std::move(cb));
}

void OsTelemetryGetVpdInfoFunction::OnResult(
    ash::health::mojom::TelemetryInfoPtr ptr) {
  if (!ptr || !ptr->vpd_result || !ptr->vpd_result->is_vpd_info()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::VpdInfo result;

  const auto& vpd_info = ptr->vpd_result->get_vpd_info();
  if (vpd_info->first_power_date.has_value()) {
    result.activate_date =
        std::make_unique<std::string>(vpd_info->first_power_date.value());
  }
  if (vpd_info->model_name.has_value()) {
    result.model_name =
        std::make_unique<std::string>(vpd_info->model_name.value());
  }
  if (vpd_info->sku_number.has_value()) {
    result.sku_number =
        std::make_unique<std::string>(vpd_info->sku_number.value());
  }

  // Protect accessing the serial number by a permission.
  if (extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber) &&
      vpd_info->serial_number.has_value()) {
    result.serial_number =
        std::make_unique<std::string>(vpd_info->serial_number.value());
  }

  Respond(ArgumentList(api::os_telemetry::GetVpdInfo::Results::Create(result)));
}

// OsTelemetryGetOemDataFunction -----------------------------------------------

OsTelemetryGetOemDataFunction::OsTelemetryGetOemDataFunction() = default;
OsTelemetryGetOemDataFunction::~OsTelemetryGetOemDataFunction() = default;

void OsTelemetryGetOemDataFunction::RunIfAllowed() {
  // Protect accessing the serial number by a permission.
  if (!extension()->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kChromeOSTelemetrySerialNumber)) {
    Respond(
        Error("Unauthorized access to chrome.os.telemetry.getOemData. Extension"
              " doesn't have the permission."));
    return;
  }

  auto cb = base::BindOnce(&OsTelemetryGetOemDataFunction::OnResult, this);

  remote_probe_service_->GetOemData(std::move(cb));
}

void OsTelemetryGetOemDataFunction::OnResult(
    ash::health::mojom::OemDataPtr ptr) {
  if (!ptr || !ptr->oem_data.has_value()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::OemData result;
  result.oem_data =
      std::make_unique<std::string>(std::move(ptr->oem_data.value()));

  Respond(ArgumentList(api::os_telemetry::GetOemData::Results::Create(result)));
}

}  // namespace chromeos
