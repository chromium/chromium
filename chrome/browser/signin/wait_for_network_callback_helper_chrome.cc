// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"

#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"

WaitForNetworkCallbackHelperChrome::WaitForNetworkCallbackHelperChrome() {
  network_connection_observer_.Observe(content::GetNetworkConnectionTracker());
}

WaitForNetworkCallbackHelperChrome::~WaitForNetworkCallbackHelperChrome() =
    default;

void WaitForNetworkCallbackHelperChrome::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
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
  network::mojom::ConnectionType type;
  bool sync = content::GetNetworkConnectionTracker()->GetConnectionType(
      &type,
      base::BindOnce(&WaitForNetworkCallbackHelperChrome::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!sync || type == network::mojom::ConnectionType::CONNECTION_NONE) {
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
