// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace system_logs {

// The entry name for network health snapshot in the SystemLogsResponse returned
// on `Fetch` function.
extern const char kNetworkHealthSnapshotEntry[];

// Fetches network health entry.
class NetworkHealthSource : public SystemLogsSource {
 public:
  // Includes PII (personally identifiable information) if `scrub` is false.
  // Includes network GUIDs if `scrub` is false and
  // `include_guid_when_not_scrub` is true. `include_guid_when_not_scrub` is set
  // to true when the caller wants the network GUIDs to be included in
  // `network_health_snapshot` in the fetched data e.g.
  // NetworkHealthDataCollector.
  explicit NetworkHealthSource(bool scrub, bool include_guid_when_not_scrub);
  ~NetworkHealthSource() override;
  NetworkHealthSource(const NetworkHealthSource&) = delete;
  NetworkHealthSource& operator=(const NetworkHealthSource&) = delete;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;

 private:
  void OnNetworkHealthReceived(
      chromeos::network_health::mojom::NetworkHealthStatePtr network_health);

  void OnNetworkDiagnosticResultsReceived(
      base::flat_map<chromeos::network_diagnostics::mojom::RoutineType,
                     chromeos::network_diagnostics::mojom::RoutineResultPtr>
          results);

  void CheckIfDone();

  bool scrub_;
  bool include_guid_when_not_scrub_;
  SysLogsSourceCallback callback_;

  std::optional<std::string> network_health_response_;
  std::optional<std::string> network_diagnostics_response_;

  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService>
      network_health_service_;
  mojo::Remote<chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      network_diagnostics_service_;

  base::WeakPtrFactory<NetworkHealthSource> weak_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_
