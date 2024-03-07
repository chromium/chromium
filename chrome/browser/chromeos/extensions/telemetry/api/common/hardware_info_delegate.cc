// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/hardware_info_delegate.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"

namespace chromeos {

namespace {

// It is generally not recommended to bahave differently based on model names.
// However, to keep backward compatibility during the deprecation, use an
// allowlist to restrict the impact to old models. See b/300015436.
constexpr std::string_view kManufacturerFallbackEnabledModelList[] = {
    "drawcia", "drawlat", "drawman",  "drawper", "landia",    "landrid",
    "lantis",  "madoo",   "barla",    "careena", "dragonair", "dratini",
    "jinlon",  "sona",    "syndra",   "dooly",   "noibat",    "snappy",
    "soraka",  "berknip", "dirinboz", "gumboz"};

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

// Callback from SysInfo::GetHardwareInfo().
void OnGetHardwareInfo(base::OnceCallback<void(std::string)> callback,
                       base::SysInfo::HardwareInfo hardware_info) {
  if (!base::Contains(kManufacturerFallbackEnabledModelList,
                      hardware_info.model)) {
    std::move(callback).Run("");
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetManufacturerFromSysfsSync), std::move(callback));
}

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
// ProbeServiceAsh[1] first. If no (or empty) information is returned,
// GetManufacturer falls back to SysInfo[2]. Notice that the fallback [2] is
// going to be deprecated so the fallback is enabled only on certain models
// that have not been migrated to cros_config.
// TODO(b/300015436): Remove [2] after cros_config is backfilled on the listed
// models.
//
// [1] ProbeServiceAsh fetches the OEM name from cros_config.
// [2] SysInfo fetches the manufacturer information from the
// "/sys/devices/virtual/dmi/id/sys_vendor" system file.
void HardwareInfoDelegate::GetManufacturer(ManufacturerCallback done_cb) {
  auto fallback = base::BindOnce(&HardwareInfoDelegate::FallbackHandler,
                                 base::Unretained(this), std::move(done_cb));

  if (remote_probe_service_strategy_) {
    auto cb = base::BindOnce(&OnGetSystemInfo).Then(std::move(fallback));
    remote_probe_service_strategy_->GetRemoteService()->ProbeTelemetryInfo(
        {crosapi::mojom::ProbeCategoryEnum::kSystem}, std::move(cb));
  } else {
    std::move(fallback).Run("");
  }
}

void HardwareInfoDelegate::FallbackHandler(ManufacturerCallback done_cb,
                                           std::string probe_service_result) {
  if (!probe_service_result.empty()) {
    std::move(done_cb).Run(probe_service_result);
    return;
  }

  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&OnGetHardwareInfo, std::move(done_cb)));
}

}  // namespace chromeos
