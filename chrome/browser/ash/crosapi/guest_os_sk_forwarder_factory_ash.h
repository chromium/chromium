// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_GUEST_OS_SK_FORWARDER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_GUEST_OS_SK_FORWARDER_FACTORY_ASH_H_

#include "chromeos/crosapi/mojom/guest_os_sk_forwarder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {
class GuestOsSkForwarderFactoryAsh : public mojom::GuestOsSkForwarderFactory {
 public:
  GuestOsSkForwarderFactoryAsh();
  GuestOsSkForwarderFactoryAsh(const GuestOsSkForwarderFactoryAsh&) = delete;
  GuestOsSkForwarderFactoryAsh& operator=(const GuestOsSkForwarderFactoryAsh&) =
      delete;
  ~GuestOsSkForwarderFactoryAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::GuestOsSkForwarderFactory> receiver);

  void BindGuestOsSkForwarder(
      mojo::PendingRemote<mojom::GuestOsSkForwarder> remote) override;

 private:
  mojo::Receiver<mojom::GuestOsSkForwarderFactory> receiver_;
};
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_GUEST_OS_SK_FORWARDER_FACTORY_ASH_H_
