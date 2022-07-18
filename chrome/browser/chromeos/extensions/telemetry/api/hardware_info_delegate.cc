// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/remote_probe_service_strategy.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace chromeos {

namespace {

// Returns manufacturer read from sys_vendor file. Runs in the separate thread
// pool which supports blocking sys calls.
// Returns an empty string on error.
//
// We use this function instead of base::SysInfo::GetHardwareInfo() since the
// latter always returns "Google" as a manufacturer on ChromeOS.
std::string GetManufacturerFromSysfsSync() {
  static const size_t kMaxStringSize = 100u;
  std::string manufacturer;
  if (base::ReadFileToStringWithMaxSize(
          base::FilePath("/sys/devices/virtual/dmi/id/sys_vendor"),
          &manufacturer, kMaxStringSize)) {
    DCHECK(base::IsStringUTF8(manufacturer));
    base::TrimWhitespaceASCII(manufacturer, base::TrimPositions::TRIM_ALL,
                              &manufacturer);
  }
  return manufacturer;
}

void GetManufacturerFromSysfs(base::OnceCallback<void(std::string)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetManufacturerFromSysfsSync), std::move(callback));
}

// Callback from ProbeTelemetryService::ProbeTelemetryInfo().
std::string OnGetSystemInfo(ash::health::mojom::TelemetryInfoPtr ptr) {
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

mojo::Remote<ash::health::mojom::ProbeService>&
HardwareInfoDelegate::GetRemoteService() {
  return remote_probe_service_strategy_->GetRemoteService();
}

// GetManufacturer tries to get the manufacturer (or OEM name) from
// ProbeTelemetryService[1] first. If no (or empty) information is returned,
// GetManufacturer falls back to SysInfo[2].
// [1] ProbeTelemetryService fetches the OEM name from cros_config.
// [2] SysInfo fetches the manufacturer information from the
// "/sys/devices/virtual/dmi/id/sys_vendor" system file.
void HardwareInfoDelegate::GetManufacturer(ManufacturerCallback done_cb) {
  auto fallback = base::BindOnce(&HardwareInfoDelegate::FallbackHandler,
                                 base::Unretained(this), std::move(done_cb));
  auto cb = base::BindOnce(&OnGetSystemInfo).Then(std::move(fallback));
  GetRemoteService()->ProbeTelemetryInfo(
      {ash::health::mojom::ProbeCategoryEnum::kSystem}, std::move(cb));
}

void HardwareInfoDelegate::FallbackHandler(ManufacturerCallback done_cb,
                                           std::string probe_service_result) {
  if (!probe_service_result.empty()) {
    std::move(done_cb).Run(probe_service_result);
    return;
  }

  GetManufacturerFromSysfs(std::move(done_cb));
}

}  // namespace chromeos
