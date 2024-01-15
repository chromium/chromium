// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/lacros/cros_apps/api/diagnostics/cros_diagnostics_impl.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/network_service.mojom.h"

CrosDiagnosticsImpl::~CrosDiagnosticsImpl() = default;

// static
void CrosDiagnosticsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver) {
  CHECK(!CrosDiagnosticsImpl::GetForCurrentDocument(render_frame_host));
  CrosDiagnosticsImpl::CreateForCurrentDocument(render_frame_host,
                                                std::move(receiver));
}

CrosDiagnosticsImpl::CrosDiagnosticsImpl(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CrosDiagnostics> receiver)
    : content::DocumentUserData<CrosDiagnosticsImpl>(render_frame_host),
      cros_diagnostics_receiver_(this, std::move(receiver)) {}

void CrosDiagnosticsImpl::GetCpuInfo(GetCpuInfoCallback callback) {
  auto cpu_info_mojom = blink::mojom::CrosCpuInfo::New();

  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::TelemetryProbeService>()) {
    auto error =
        blink::mojom::GetCpuInfoError::kTelemetryProbeServiceUnavailable;
    std::move(callback).Run(
        blink::mojom::GetCpuInfoResult::NewError(std::move(error)));
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::TelemetryProbeService>()
      ->ProbeTelemetryInfo(
          {crosapi::mojom::ProbeCategoryEnum::kCpu},
          base::BindOnce(
              &CrosDiagnosticsImpl::GetCpuInfoProbeTelemetryInfoCallback,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback),
              std::move(cpu_info_mojom)));
}

void CrosDiagnosticsImpl::GetCpuInfoProbeTelemetryInfoCallback(
    GetCpuInfoCallback callback,
    blink::mojom::CrosCpuInfoPtr cpu_info_mojom,
    crosapi::mojom::ProbeTelemetryInfoPtr telemetry_info) {
  CHECK(telemetry_info->cpu_result);
  // TODO(b/298621530): Plumb the error from cpu_result through
  // to the `chromeos.diagnostics.getCpuInfo()` API.
  if (telemetry_info->cpu_result->is_error()) {
    auto error = blink::mojom::GetCpuInfoError::kCpuTelemetryInfoUnavailable;
    std::move(callback).Run(
        blink::mojom::GetCpuInfoResult::NewError(std::move(error)));
    return;
  }

  switch (telemetry_info->cpu_result->get_cpu_info()->architecture) {
    case crosapi::mojom::ProbeCpuArchitectureEnum::kUnknown:
      cpu_info_mojom->architecture =
          blink::mojom::CrosCpuArchitecture::kUnknown;
      break;
    case crosapi::mojom::ProbeCpuArchitectureEnum::kX86_64:
      cpu_info_mojom->architecture = blink::mojom::CrosCpuArchitecture::kX86_64;
      break;
    case crosapi::mojom::ProbeCpuArchitectureEnum::kArmv7l:
      cpu_info_mojom->architecture = blink::mojom::CrosCpuArchitecture::kArm;
      break;
    case crosapi::mojom::ProbeCpuArchitectureEnum::kAArch64:
      cpu_info_mojom->architecture = blink::mojom::CrosCpuArchitecture::kArm64;
      break;
  }

  std::vector<blink::mojom::CrosLogicalCpuInfoPtr> logical_cpu_infos_mojom;

  // Concatenate logical processor infos from each physical CPU.
  for (const auto& physical_cpu :
       telemetry_info->cpu_result->get_cpu_info()->physical_cpus) {
    if (physical_cpu->model_name) {
      // Assume that either only one `physical_cpu` exists, or each different
      // `physical_cpu` shares the same `model_name`.
      cpu_info_mojom->model_name = physical_cpu->model_name.value();
    }
    for (const auto& logical_cpu : physical_cpu->logical_cpus) {
      auto logical_cpu_info_mojom = blink::mojom::CrosLogicalCpuInfo::New();

      if (logical_cpu->core_id) {
        logical_cpu_info_mojom->core_id = logical_cpu->core_id->value;
      }
      if (logical_cpu->idle_time_ms) {
        logical_cpu_info_mojom->idle_time_ms = logical_cpu->idle_time_ms->value;
      }
      if (logical_cpu->max_clock_speed_khz) {
        logical_cpu_info_mojom->max_clock_speed_khz =
            logical_cpu->max_clock_speed_khz->value;
      }
      if (logical_cpu->scaling_current_frequency_khz) {
        logical_cpu_info_mojom->scaling_current_frequency_khz =
            logical_cpu->scaling_current_frequency_khz->value;
      }
      if (logical_cpu->scaling_max_frequency_khz) {
        logical_cpu_info_mojom->scaling_max_frequency_khz =
            logical_cpu->scaling_max_frequency_khz->value;
      }

      logical_cpu_infos_mojom.push_back(std::move(logical_cpu_info_mojom));
    }
  }

  cpu_info_mojom->logical_cpus = std::move(logical_cpu_infos_mojom);
  std::move(callback).Run(
      blink::mojom::GetCpuInfoResult::NewCpuInfo(std::move(cpu_info_mojom)));
}

void CrosDiagnosticsImpl::GetNetworkInterfaces(
    GetNetworkInterfacesCallback callback) {
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(
          &CrosDiagnosticsImpl::GetNetworkInterfacesGetNetworkListCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrosDiagnosticsImpl::GetNetworkInterfacesGetNetworkListCallback(
    GetNetworkInterfacesCallback callback,
    const std::optional<net::NetworkInterfaceList>& interface_list) {
  if (!interface_list.has_value()) {
    auto error =
        blink::mojom::GetNetworkInterfacesError::kNetworkInterfaceLookupFailed;
    std::move(callback).Run(
        blink::mojom::GetNetworkInterfacesResult::NewError(std::move(error)));
    return;
  }

  std::vector<blink::mojom::CrosNetworkInterfacePtr> network_interfaces_mojom;
  for (const auto& interface : interface_list.value()) {
    auto network_interface_mojom = blink::mojom::CrosNetworkInterface::New();

    network_interface_mojom->address = interface.address.ToString();
    network_interface_mojom->name = interface.name;
    network_interface_mojom->prefix_length = interface.prefix_length;

    network_interfaces_mojom.push_back(std::move(network_interface_mojom));
  }

  std::move(callback).Run(
      blink::mojom::GetNetworkInterfacesResult::NewNetworkInterfaces(
          std::move(network_interfaces_mojom)));
}

DOCUMENT_USER_DATA_KEY_IMPL(CrosDiagnosticsImpl);
