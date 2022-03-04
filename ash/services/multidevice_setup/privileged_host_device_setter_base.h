// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_BASE_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_BASE_H_

#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace multidevice_setup {

// PrivilegedHostDeviceSetter implementation which accepts receivers to bind to
// it.
class PrivilegedHostDeviceSetterBase
    : public ash::multidevice_setup::mojom::PrivilegedHostDeviceSetter {
 public:
  PrivilegedHostDeviceSetterBase(const PrivilegedHostDeviceSetterBase&) =
      delete;
  PrivilegedHostDeviceSetterBase& operator=(
      const PrivilegedHostDeviceSetterBase&) = delete;

  ~PrivilegedHostDeviceSetterBase() override;

  void BindReceiver(
      mojo::PendingReceiver<
          ash::multidevice_setup::mojom::PrivilegedHostDeviceSetter> receiver);

 protected:
  PrivilegedHostDeviceSetterBase();

 private:
  mojo::ReceiverSet<ash::multidevice_setup::mojom::PrivilegedHostDeviceSetter>
      receivers_;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_PRIVILEGED_HOST_DEVICE_SETTER_BASE_H_
