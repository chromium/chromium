// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_GUEST_OS_VM_SK_FORWARDING_SERVICE_H_
#define CHROME_BROWSER_LACROS_GUEST_OS_VM_SK_FORWARDING_SERVICE_H_

#include "chromeos/crosapi/mojom/guest_os_sk_forwarder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace guest_os {
class VmSkForwardingService : public crosapi::mojom::GuestOsSkForwarder {
 public:
  VmSkForwardingService();
  VmSkForwardingService(const VmSkForwardingService&) = delete;
  VmSkForwardingService& operator=(const VmSkForwardingService&) = delete;
  ~VmSkForwardingService() override;

  // crosapi::mojom::GuestOsSkForwarder
  void ForwardRequest(const std::string& message,
                      ForwardRequestCallback callback) override;

 private:
  mojo::Remote<crosapi::mojom::GuestOsSkForwarderFactory> remote_;
  mojo::Receiver<crosapi::mojom::GuestOsSkForwarder> receiver_;
};
}  // namespace guest_os
#endif  // CHROME_BROWSER_LACROS_GUEST_OS_VM_SK_FORWARDING_SERVICE_H_
