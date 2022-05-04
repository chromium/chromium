// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/probe_service.h"

#include <utility>

#include "ash/webui/telemetry_extension_ui/services/probe_service_converters.h"
#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
namespace cros_healthd = ::ash::cros_healthd;
constexpr char kOemDataLogName[] = "oemdata";
}  // namespace

ProbeService::ProbeService(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver)
    : receiver_(this, std::move(receiver)) {}

ProbeService::~ProbeService() = default;

void ProbeService::ProbeTelemetryInfo(
    const std::vector<health::mojom::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  GetService()->ProbeTelemetryInfo(
      converters::ConvertCategoryVector(categories),
      base::BindOnce(
          [](health::mojom::ProbeService::ProbeTelemetryInfoCallback callback,
             cros_healthd::mojom::TelemetryInfoPtr ptr) {
            std::move(callback).Run(
                converters::ConvertProbePtr(std::move(ptr)));
          },
          std::move(callback)));
}

void ProbeService::GetOemData(GetOemDataCallback callback) {
  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  debugd_client->GetLog(
      kOemDataLogName,
      base::BindOnce(
          [](GetOemDataCallback callback,
             absl::optional<std::string> oem_data) {
            std::move(callback).Run(
                health::mojom::OemData::New(std::move(oem_data)));
          },
          std::move(callback)));
}

cros_healthd::mojom::CrosHealthdProbeService* ProbeService::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(
        base::BindOnce(&ProbeService::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void ProbeService::OnDisconnect() {
  service_.reset();
}

}  // namespace ash
