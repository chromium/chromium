// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/diagnostics/diagnostics_service.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cfm {

namespace {
static DiagnosticsService* g_info_service = nullptr;
}  // namespace

// static
void DiagnosticsService::Initialize() {
  CHECK(!g_info_service);
  g_info_service = new DiagnosticsService();
}

// static
void DiagnosticsService::Shutdown() {
  CHECK(g_info_service);
  delete g_info_service;
  g_info_service = nullptr;
}

// static
DiagnosticsService* DiagnosticsService::Get() {
  CHECK(g_info_service)
      << "DeviceInfoService::Get() called before Initialize()";
  return g_info_service;
}

// static
bool DiagnosticsService::IsInitialized() {
  return g_info_service;
}

void DiagnosticsService::GetCrosHealthdTelemetry(
    GetCrosHealthdTelemetryCallback callback) {
  cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      {cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
       cros_healthd::mojom::ProbeCategoryEnum::kCpu,
       cros_healthd::mojom::ProbeCategoryEnum::kTimezone,
       cros_healthd::mojom::ProbeCategoryEnum::kMemory,
       cros_healthd::mojom::ProbeCategoryEnum::kFan,
       cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition,
       cros_healthd::mojom::ProbeCategoryEnum::kSystem,
       cros_healthd::mojom::ProbeCategoryEnum::kNetwork},
      std::move(callback));
}

void DiagnosticsService::GetCrosHealthdProcessInfo(
    uint32_t pid,
    GetCrosHealthdProcessInfoCallback callback) {
  cros_healthd::ServiceConnection::GetInstance()->ProbeProcessInfo(
      static_cast<pid_t>(pid), std::move(callback));
}

bool DiagnosticsService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::MeetDevicesDiagnostics::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void DiagnosticsService::OnAdaptorDisconnect() {
  LOG(ERROR) << "mojom::Diagnostics Service Adaptor has been disconnected";
  // CleanUp to follow the lifecycle of the primary CfmServiceContext
  receivers_.Clear();
}

void DiagnosticsService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::MeetDevicesDiagnostics>(
                           std::move(receiver_pipe)));
}

// Private methods

DiagnosticsService::DiagnosticsService()
    : service_adaptor_(mojom::MeetDevicesDiagnostics::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);
}

DiagnosticsService::~DiagnosticsService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace cfm
}  // namespace chromeos
