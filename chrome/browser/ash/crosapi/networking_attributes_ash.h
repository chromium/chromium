// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORKING_ATTRIBUTES_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORKING_ATTRIBUTES_ASH_H_

#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the NetworkingAttributes crosapi interface.
class NetworkingAttributesAsh : public mojom::NetworkingAttributes {
 public:
  NetworkingAttributesAsh();
  NetworkingAttributesAsh(const NetworkingAttributesAsh&) = delete;
  NetworkingAttributesAsh& operator=(const NetworkingAttributesAsh&) = delete;
  ~NetworkingAttributesAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::NetworkingAttributes> receiver);

  // crosapi::mojom::NetworkingAttributes:
  void GetNetworkDetails(GetNetworkDetailsCallback callback) override;

 private:
  using Result = mojom::GetNetworkDetailsResult;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::NetworkingAttributes> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORKING_ATTRIBUTES_ASH_H_
