// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/hardware_info_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace chromeos {

namespace {

HardwareInfoDelegate* g_instance = nullptr;

// Callback from ProbeServiceAsh::ProbeTelemetryInfo().
std::string OnGetSystemInfo(crosapi::mojom::ProbeTelemetryInfoPtr ptr) {
  if (!ptr || !ptr->system_result || !ptr->system_result->is_system_info()) {
    return "";
  }

  const auto& system_info = ptr->system_result->get_system_info();
  if (!system_info->os_info->oem_name.has_value()) {
    return "";
  }

  return system_info->os_info->oem_name.value();
}

}  // namespace

// static
HardwareInfoDelegate& HardwareInfoDelegate::Get() {
  if (!g_instance) {
    g_instance = new HardwareInfoDelegate();
  }
  return *g_instance;
}

HardwareInfoDelegate::HardwareInfoDelegate() = default;
HardwareInfoDelegate::~HardwareInfoDelegate() = default;

// GetManufacturer tries to get the manufacturer (or OEM name) from
// ProbeServiceAsh, which fetches the OEM name from cros_config.
void HardwareInfoDelegate::GetManufacturer(ManufacturerCallback done_cb) {
  if (manufacturer_cache_.has_value()) {
    std::move(done_cb).Run(manufacturer_cache_.value());
    return;
  }

  auto set_cache_cb =
      base::BindOnce(&HardwareInfoDelegate::SetCacheAndReturnResult,
                     base::Unretained(this), std::move(done_cb));
  auto* probe_service =
      RemoteProbeServiceStrategy::Get()->GetRemoteProbeService();
  if (probe_service) {
    auto cb = base::BindOnce(&OnGetSystemInfo).Then(std::move(set_cache_cb));
    probe_service->ProbeTelemetryInfo(
        {crosapi::mojom::ProbeCategoryEnum::kSystem}, std::move(cb));
  } else {
    std::move(done_cb).Run("");
  }
}

void HardwareInfoDelegate::ClearCacheForTesting() {
  manufacturer_cache_ = std::nullopt;
}

void HardwareInfoDelegate::SetCacheAndReturnResult(
    ManufacturerCallback done_cb,
    const std::string& manufacturer) {
  manufacturer_cache_ = manufacturer;
  std::move(done_cb).Run(manufacturer);
}

}  // namespace chromeos
