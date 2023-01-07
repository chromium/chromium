// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_WIFI_SIGNAL_STRENGTH_RSSI_FETCHER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_WIFI_SIGNAL_STRENGTH_RSSI_FETCHER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"

namespace reporting {

using WifiSignalStrengthRssiCallback =
    base::OnceCallback<void(base::flat_map<std::string, int>)>;

void FetchWifiSignalStrengthRssi(base::queue<std::string> service_path_queue,
                                 WifiSignalStrengthRssiCallback cb);

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_NETWORK_WIFI_SIGNAL_STRENGTH_RSSI_FETCHER_H_
