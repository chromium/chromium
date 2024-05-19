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
HardwareInfoDelegate::Factory* HardwareInfoDelegate::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<HardwareInfoDelegate> HardwareInfoDelegate::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }
  return base::WrapUnique<HardwareInfoDelegate>(new HardwareInfoDelegate());
}

// static
void HardwareInfoDelegate::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

HardwareInfoDelegate::Factory::~Factory() = default;

HardwareInfoDelegate::HardwareInfoDelegate()
    : remote_probe_service_strategy_(RemoteProbeServiceStrategy::Create()) {}
HardwareInfoDelegate::~HardwareInfoDelegate() = default;

// GetManufacturer tries to get the manufacturer (or OEM name) from
// ProbeServiceAsh, which fetches the OEM name from cros_config.
void HardwareInfoDelegate::GetManufacturer(ManufacturerCallback done_cb) {
  if (remote_probe_service_strategy_) {
    remote_probe_service_strategy_->GetRemoteService()->ProbeTelemetryInfo(
        {crosapi::mojom::ProbeCategoryEnum::kSystem},
        base::BindOnce(&OnGetSystemInfo).Then(std::move(done_cb)));
  } else {
    std::move(done_cb).Run("");
  }
}

}  // namespace chromeos
