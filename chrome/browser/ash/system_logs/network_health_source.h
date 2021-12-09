// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace system_logs {

// Fetches network health entry.
class NetworkHealthSource : public SystemLogsSource {
 public:
  explicit NetworkHealthSource(bool scrub);
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
  SysLogsSourceCallback callback_;

  absl::optional<std::string> network_health_response_;
  absl::optional<std::string> network_diagnostics_response_;

  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService>
      network_health_service_;
  mojo::Remote<chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      network_diagnostics_service_;

  base::WeakPtrFactory<NetworkHealthSource> weak_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_
