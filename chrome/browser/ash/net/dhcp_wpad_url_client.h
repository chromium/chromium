// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_DHCP_WPAD_URL_CLIENT_H_
#define CHROME_BROWSER_ASH_NET_DHCP_WPAD_URL_CLIENT_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/dhcp_wpad_url_client.mojom.h"
#include "url/gurl.h"

namespace ash {

// A mojom::DhcpWpadUrlClient implementation that gets the PAC
// script URL from the DefaultNetwork, which mirrors the state from Shill
// Manager.DefaultNetwork.DhcpWpadUrlClient.
class DhcpWpadUrlClient : public network::mojom::DhcpWpadUrlClient {
 public:
  DhcpWpadUrlClient() = default;

  DhcpWpadUrlClient(const DhcpWpadUrlClient&) = delete;
  DhcpWpadUrlClient& operator=(const DhcpWpadUrlClient&) = delete;

  ~DhcpWpadUrlClient() override {}

  // Gets the PAC script URL from the DefaultNetwork and calls |callback| with
  // the result. If an error occurs or no PAC URL is provided, |callback| is
  // called with an empty string.
  void GetPacUrl(GetPacUrlCallback callback) override;

  // Convenience method that creates a self-owned
  // DhcpWpadUrlClient and returns a remote endpoint to
  // control it.
  static mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
  CreateWithSelfOwnedReceiver();

  // Overrides the URL returned by DhcpWpadUrlClient for testing. Instead of
  // querying the default network's DHCP, queries will return the value pointed
  // by |url|. Should be matched with a call to |ClearPacUrlForTesting| at the
  // end of the test.
  static void SetPacUrlForTesting(const GURL& url);
  static void ClearPacUrlForTesting();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_DHCP_WPAD_URL_CLIENT_H_
