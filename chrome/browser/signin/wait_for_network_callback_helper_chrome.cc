// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/network_quality_tracker.h"

WaitForNetworkCallbackHelperChrome::WaitForNetworkCallbackHelperChrome(
    bool should_disable_metrics_for_testing) {
  network_connection_observer_.Observe(content::GetNetworkConnectionTracker());
  // Metrics timer causes `TaskEnvironment::FastForwardUntilNoTasksRemain()` to
  // spin forever in testing environments.
  if (!should_disable_metrics_for_testing) {
    CHECK(g_browser_process);
    metrics_timer_.Start(FROM_HERE, base::Minutes(30), this,
                         &WaitForNetworkCallbackHelperChrome::LogMetrics);
  }
}

WaitForNetworkCallbackHelperChrome::~WaitForNetworkCallbackHelperChrome() =
    default;

void WaitForNetworkCallbackHelperChrome::OnConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    return;
  }

  std::vector<base::OnceClosure> callbacks;
  delayed_callbacks_.swap(callbacks);
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

bool WaitForNetworkCallbackHelperChrome::AreNetworkCallsDelayed() {
  // Don't bother if we don't have any kind of network connection.
  net::NetworkChangeNotifier::ConnectionType type;
  bool sync = content::GetNetworkConnectionTracker()->GetConnectionType(
      &type,
      base::BindOnce(&WaitForNetworkCallbackHelperChrome::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!sync ||
      type == net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    return true;
  }

  return false;
}

void WaitForNetworkCallbackHelperChrome::DelayNetworkCall(
    base::OnceClosure callback) {
  if (!AreNetworkCallsDelayed()) {
    std::move(callback).Run();
    return;
  }

  // This queue will be processed in `OnConnectionChanged()`.
  delayed_callbacks_.push_back(std::move(callback));
}

void WaitForNetworkCallbackHelperChrome::LogMetrics() {
  net::EffectiveConnectionType ect =
      g_browser_process->network_quality_tracker()
          ->GetEffectiveConnectionType();

  base::UmaHistogramCounts100(
      base::StrCat({"Signin.DelayedNetworkCallQueueSize.",
                    net::GetNameForEffectiveConnectionType(ect)}),
      delayed_callbacks_.size());
}
