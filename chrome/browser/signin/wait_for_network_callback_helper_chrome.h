// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_WAIT_FOR_NETWORK_CALLBACK_HELPER_CHROME_H_
#define CHROME_BROWSER_SIGNIN_WAIT_FOR_NETWORK_CALLBACK_HELPER_CHROME_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/base/wait_for_network_callback_helper.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"

class WaitForNetworkCallbackHelperChrome
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public WaitForNetworkCallbackHelper {
 public:
  WaitForNetworkCallbackHelperChrome();

  WaitForNetworkCallbackHelperChrome(
      const WaitForNetworkCallbackHelperChrome&) = delete;
  WaitForNetworkCallbackHelperChrome& operator=(
      const WaitForNetworkCallbackHelperChrome&) = delete;

  ~WaitForNetworkCallbackHelperChrome() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // WaitForNetworkCallbackHelper:
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;

 private:
  std::vector<base::OnceClosure> delayed_callbacks_;
  base::ScopedObservation<
      network::NetworkConnectionTracker,
      network::NetworkConnectionTracker::NetworkConnectionObserver>
      network_connection_observer_{this};
  base::WeakPtrFactory<WaitForNetworkCallbackHelperChrome> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_WAIT_FOR_NETWORK_CALLBACK_HELPER_CHROME_H_
