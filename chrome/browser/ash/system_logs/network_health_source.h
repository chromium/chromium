// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/remote.h"

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
      SysLogsSourceCallback callback,
      chromeos::network_health::mojom::NetworkHealthStatePtr network_health);

  bool scrub_;
  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService>
      network_health_service_;

  base::WeakPtrFactory<NetworkHealthSource> weak_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_NETWORK_HEALTH_SOURCE_H_
