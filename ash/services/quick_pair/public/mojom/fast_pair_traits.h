// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_QUICK_PAIR_PUBLIC_MOJOM_FAST_PAIR_TRAITS_H_
#define ASH_SERVICES_QUICK_PAIR_PUBLIC_MOJOM_FAST_PAIR_TRAITS_H_

#include <algorithm>
#include <cstdint>
#include <vector>

#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace {
using ash::quick_pair::DecryptedPasskey;
using ash::quick_pair::DecryptedResponse;
using ash::quick_pair::FastPairMessageType;
using ash::quick_pair::mojom::DecryptedPasskeyDataView;
using ash::quick_pair::mojom::DecryptedResponseDataView;
using ash::quick_pair::mojom::MessageType;
}  // namespace

template <>
class EnumTraits<MessageType, FastPairMessageType> {
 public:
  static MessageType ToMojom(FastPairMessageType input);
  static bool FromMojom(MessageType input, FastPairMessageType* out);
};

template <>
class StructTraits<DecryptedResponseDataView, DecryptedResponse> {
 public:
  static MessageType message_type(const DecryptedResponse& r) {
    return EnumTraits<MessageType, FastPairMessageType>::ToMojom(
        r.message_type);
  }

  static std::vector<uint8_t> address_bytes(const DecryptedResponse& r) {
    return std::vector<uint8_t>(r.address_bytes.begin(), r.address_bytes.end());
  }

  static std::vector<uint8_t> salt(const DecryptedResponse& r) {
    return std::vector<uint8_t>(r.salt.begin(), r.salt.end());
  }

  static bool Read(DecryptedResponseDataView data, DecryptedResponse* out);
};

template <>
class StructTraits<DecryptedPasskeyDataView, DecryptedPasskey> {
 public:
  static MessageType message_type(const DecryptedPasskey& r) {
    return EnumTraits<MessageType, FastPairMessageType>::ToMojom(
        r.message_type);
  }

  static uint32_t passkey(const DecryptedPasskey& r) { return r.passkey; }

  static std::vector<uint8_t> salt(const DecryptedPasskey& r) {
    return std::vector<uint8_t>(r.salt.begin(), r.salt.end());
  }

  static bool Read(DecryptedPasskeyDataView data, DecryptedPasskey* out);
};

}  // namespace mojo

#endif  // ASH_SERVICES_QUICK_PAIR_PUBLIC_MOJOM_FAST_PAIR_TRAITS_H_
