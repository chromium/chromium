// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEBUG_INTERFACE_REGISTERER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEBUG_INTERFACE_REGISTERER_ASH_H_

#include "ash/public/cpp/debug_delegate.h"
#include "chromeos/crosapi/mojom/debug_interface.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

class DebugInterfaceRegistererAsh : public mojom::DebugInterfaceRegisterer,
                                    public ash::DebugDelegate {
 public:
  DebugInterfaceRegistererAsh();
  DebugInterfaceRegistererAsh(const DebugInterfaceRegistererAsh&) = delete;
  DebugInterfaceRegistererAsh operator=(const DebugInterfaceRegistererAsh&) =
      delete;
  ~DebugInterfaceRegistererAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::DebugInterfaceRegisterer> pending_receiver);

  // crosapi::mojom::DebugInterfaceRegisterer:
  void RegisterDebugInterface(
      mojo::PendingRemote<mojom::DebugInterface> interface) override;

  // ash::DebugDelegate:
  void PrintLayerHierarchy() override;
  void PrintWindowHierarchy() override;
  void PrintViewHierarchy() override;

 private:
  mojo::ReceiverSet<mojom::DebugInterfaceRegisterer> receivers_;
  mojo::RemoteSet<mojom::DebugInterface> interfaces_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEBUG_INTERFACE_REGISTERER_ASH_H_
