// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SMBFS_IP_ADDRESS_MOJOM_TRAITS_H_
#define ASH_COMPONENTS_SMBFS_IP_ADDRESS_MOJOM_TRAITS_H_

#include "ash/components/smbfs/mojom/ip_address.mojom-shared.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/ip_address.h"

namespace mojo {
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<smbfs::mojom::IPAddressDataView, net::IPAddress> {
  static base::span<const uint8_t> address_bytes(
      const net::IPAddress& ip_address) {
    return ip_address.bytes();
  }

  static bool Read(smbfs::mojom::IPAddressDataView obj, net::IPAddress* out);
};

}  // namespace mojo

#endif  // ASH_COMPONENTS_SMBFS_IP_ADDRESS_MOJOM_TRAITS_H_
