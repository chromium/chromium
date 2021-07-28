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

namespace chromeos {

OsTelemetryGetVpdInfoFunction::OsTelemetryGetVpdInfoFunction()
    : probe_service_(remote_probe_service_.BindNewPipeAndPassReceiver()) {}
OsTelemetryGetVpdInfoFunction::~OsTelemetryGetVpdInfoFunction() = default;

ExtensionFunction::ResponseAction OsTelemetryGetVpdInfoFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(&OsTelemetryGetVpdInfoFunction::OnResult, this);

  remote_probe_service_->ProbeTelemetryInfo(
      {health::mojom::ProbeCategoryEnum::kCachedVpdData}, std::move(cb));

  return RespondLater();
}

void OsTelemetryGetVpdInfoFunction::OnResult(
    health::mojom::TelemetryInfoPtr ptr) {
  if (!ptr || !ptr->vpd_result || !ptr->vpd_result->is_vpd_info()) {
    Respond(Error("API internal error"));
    return;
  }

  api::os_telemetry::VpdInfo result;

  const auto& vpd_info = ptr->vpd_result->get_vpd_info();
  if (vpd_info->model_name.has_value()) {
    result.model_name =
        std::make_unique<std::string>(vpd_info->model_name.value());
  }
  if (vpd_info->serial_number.has_value()) {
    result.serial_number =
        std::make_unique<std::string>(vpd_info->serial_number.value());
  }
  if (vpd_info->sku_number.has_value()) {
    result.sku_number =
        std::make_unique<std::string>(vpd_info->sku_number.value());
  }

  Respond(ArgumentList(api::os_telemetry::GetVpdInfo::Results::Create(result)));
}

}  // namespace chromeos
