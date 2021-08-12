// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/public/mojom/fast_pair_traits.h"

#include <algorithm>
#include <vector>

namespace mojo {

// static
bool StructTraits<DecryptedResponseDataView, DecryptedResponse>::Read(
    DecryptedResponseDataView data,
    DecryptedResponse* out) {
  std::vector<uint8_t> address_bytes;
  if (!data.ReadAddressBytes(&address_bytes) ||
      address_bytes.size() != out->address_bytes.size())
    return false;

  std::vector<uint8_t> salt_bytes;
  if (!data.ReadSalt(&salt_bytes) || salt_bytes.size() != out->salt.size())
    return false;

  if (!EnumTraits<MessageType, FastPairMessageType>::FromMojom(
          data.message_type(), &out->message_type))
    return false;

  std::copy(address_bytes.begin(), address_bytes.end(),
            out->address_bytes.begin());
  std::copy(salt_bytes.begin(), salt_bytes.end(), out->salt.begin());

  return true;
}

// static
bool StructTraits<DecryptedPasskeyDataView, DecryptedPasskey>::Read(
    DecryptedPasskeyDataView data,
    DecryptedPasskey* out) {
  std::vector<uint8_t> salt_bytes;
  if (!data.ReadSalt(&salt_bytes) || salt_bytes.size() != out->salt.size())
    return false;

  if (!EnumTraits<MessageType, FastPairMessageType>::FromMojom(
          data.message_type(), &out->message_type))
    return false;

  out->passkey = data.passkey();
  std::copy(salt_bytes.begin(), salt_bytes.end(), out->salt.begin());

  return true;
}

// static
MessageType EnumTraits<MessageType, FastPairMessageType>::ToMojom(
    FastPairMessageType input) {
  switch (input) {
    case FastPairMessageType::kKeyBasedPairingRequest:
      return MessageType::kKeyBasedPairingRequest;
    case FastPairMessageType::kKeyBasedPairingResponse:
      return MessageType::kKeyBasedPairingResponse;
    case FastPairMessageType::kSeekersPasskey:
      return MessageType::kSeekersPasskey;
    case FastPairMessageType::kProvidersPasskey:
      return MessageType::kProvidersPasskey;
  }
}

// static
bool EnumTraits<MessageType, FastPairMessageType>::FromMojom(
    MessageType input,
    FastPairMessageType* out) {
  switch (input) {
    case MessageType::kKeyBasedPairingRequest:
      *out = FastPairMessageType::kKeyBasedPairingRequest;
      return true;
    case MessageType::kKeyBasedPairingResponse:
      *out = FastPairMessageType::kKeyBasedPairingResponse;
      return true;
    case MessageType::kSeekersPasskey:
      *out = FastPairMessageType::kSeekersPasskey;
      return true;
    case MessageType::kProvidersPasskey:
      *out = FastPairMessageType::kProvidersPasskey;
      return true;
  }

  return false;
}

}  // namespace mojo
