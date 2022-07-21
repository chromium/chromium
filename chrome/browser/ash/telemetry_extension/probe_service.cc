// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/probe_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/telemetry_extension/probe_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
namespace cros_healthd = ::ash::cros_healthd;
constexpr char kOemDataLogName[] = "oemdata";
}  // namespace

// static
ProbeService::Factory* ProbeService::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<health::mojom::ProbeService> ProbeService::Factory::Create(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  return base::WrapUnique<ProbeService>(new ProbeService(std::move(receiver)));
}

// static
void ProbeService::Factory::SetForTesting(ProbeService::Factory* test_factory) {
  test_factory_ = test_factory;
}

ProbeService::Factory::~Factory() = default;

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
  chromeos::DebugDaemonClient::Get()->GetLog(
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
