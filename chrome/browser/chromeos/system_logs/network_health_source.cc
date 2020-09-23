// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/network_health_source.h"

#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/net/network_health/network_health_service.h"
#include "chromeos/network/network_event_log.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kNetworkHealthSnapshotEntry[] = "network-health-snapshot";

std::string GetFormattedString(
    const chromeos::network_health::mojom::NetworkHealthStatePtr&
        network_health,
    bool scrub) {
  std::ostringstream output;

  for (const auto& net : network_health->networks) {
    if (scrub) {
      output << "Name: " << chromeos::NetworkGuidId(net->guid.value_or("N/A"))
             << "\n";
    } else {
      output << "Name: " << net->name.value_or("N/A") << "\n";
    }

    output << "Type: " << net->type << "\n";
    output << "State: " << net->state << "\n";
    output << "Signal Strength: "
           << (net->signal_strength
                   ? base::NumberToString(net->signal_strength->value)
                   : "N/A")
           << "\n";
    output << "MAC Address: " << net->mac_address.value_or("N/A") << "\n";
    output << "\n";
  }
  return output.str();
}

}  // namespace

NetworkHealthSource::NetworkHealthSource(bool scrub)
    : SystemLogsSource("NetworkHealth"), scrub_(scrub) {
  chromeos::network_health::NetworkHealthService::GetInstance()
      ->BindHealthReceiver(
          network_health_service_.BindNewPipeAndPassReceiver());
}

NetworkHealthSource::~NetworkHealthSource() {}

void NetworkHealthSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());
  network_health_service_->GetHealthSnapshot(
      base::BindOnce(&NetworkHealthSource::OnNetworkHealthReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NetworkHealthSource::OnNetworkHealthReceived(
    SysLogsSourceCallback callback,
    chromeos::network_health::mojom::NetworkHealthStatePtr network_health) {
  auto response = std::make_unique<SystemLogsResponse>();
  (*response)[kNetworkHealthSnapshotEntry] =
      GetFormattedString(network_health, scrub_);
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
