// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORK_CHANGE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORK_CHANGE_ASH_H_

#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Ash-side implementation of NetworkChange interface.
class NetworkChangeAsh : public mojom::NetworkChange {
 public:
  NetworkChangeAsh();
  NetworkChangeAsh(const NetworkChangeAsh&) = delete;
  NetworkChangeAsh& operator=(const NetworkChangeAsh&) = delete;
  ~NetworkChangeAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::NetworkChange> receiver);

  // crosapi::mojom::NetworkChange:
  void AddObserver(mojo::PendingRemote<crosapi::mojom::NetworkChangeObserver>
                       observer) override;

 private:
  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::NetworkChange> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORK_CHANGE_ASH_H_
