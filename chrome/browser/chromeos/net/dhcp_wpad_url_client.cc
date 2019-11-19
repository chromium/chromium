// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/dhcp_wpad_url_client.h"

#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {
GURL* g_pac_url_for_testing = nullptr;
}

namespace chromeos {

void DhcpWpadUrlClient::GetPacUrl(GetPacUrlCallback callback) {
  if (g_pac_url_for_testing) {
    std::move(callback).Run(g_pac_url_for_testing->spec());
    return;
  }
  // NetworkHandler may be uninitialized during shutdown and unit tests.
  const NetworkState* default_network =
      NetworkHandler::IsInitialized()
          ? NetworkHandler::Get()->network_state_handler()->DefaultNetwork()
          : nullptr;
  if (default_network) {
    std::move(callback).Run(
        default_network->GetWebProxyAutoDiscoveryUrl().spec());
    return;
  }
  std::move(callback).Run(std::string());
}

// static
mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
DhcpWpadUrlClient::CreateWithSelfOwnedReceiver() {
  mojo::PendingRemote<network::mojom::DhcpWpadUrlClient> remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<DhcpWpadUrlClient>(),
                              remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

// static
void DhcpWpadUrlClient::ClearPacUrlForTesting() {
  if (g_pac_url_for_testing) {
    delete g_pac_url_for_testing;
    g_pac_url_for_testing = nullptr;
  }
}

// static
void DhcpWpadUrlClient::SetPacUrlForTesting(const GURL& url) {
  ClearPacUrlForTesting();
  g_pac_url_for_testing = new GURL(url);
}

}  // namespace chromeos
