// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_WIRE_FORMAT_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_WIRE_FORMAT_H_

#include <mach/message.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"

namespace browser_actuator {

// Layout of a local-actuation Mach message. Each message carries its data
// inline, with no port or memory descriptors:
//
//   mach_msg_header_t          24 bytes
//   LocalActuationWireHeader   12 bytes
//   info_plist                 `info_plist_len` bytes: the sender's
//                              `Info.plist`
//   payload                    `payload_len` bytes: a serialized
//                              `LocalActuationRequest` or
//                              `LocalActuationResponse`
//   padding                    0 to 3 bytes, so the size is a multiple of 4

// `msgh_id` of a request (caller -> Chrome). The integer value of the
// four-character code 'lact'.
inline constexpr mach_msg_id_t kLocalActuationRequestMsgId = 0x6C616374;
// `msgh_id` of a reply (Chrome -> caller). The integer value of 'LACT'.
inline constexpr mach_msg_id_t kLocalActuationReplyMsgId = 0x4C414354;

// Value of `LocalActuationWireHeader::protocol_version`. Changes only when the
// layout or payload encoding changes, not when the protos gain fields or
// methods.
inline constexpr uint32_t kLocalActuationProtocolVersion = 1;

// Follows `mach_msg_header_t`. Fields are in native byte order.
struct LocalActuationWireHeader {
  uint32_t protocol_version;
  uint32_t info_plist_len;
  uint32_t payload_len;
};
static_assert(sizeof(LocalActuationWireHeader) == 12);

// Combined size of `mach_msg_header_t` and `LocalActuationWireHeader`. A valid
// message is larger: its `info_plist` is never empty.
inline constexpr size_t kLocalActuationMinMessageSize =
    sizeof(mach_msg_header_t) + sizeof(LocalActuationWireHeader);

// Largest message, including `mach_msg_header_t`, that either side accepts.
inline constexpr size_t kLocalActuationMaxMessageSize = 64 * 1024;

// Largest `Info.plist` either side accepts in a message.
inline constexpr size_t kLocalActuationMaxInfoPlistSize = 32 * 1024;

// Views into the `body` passed to `ParseLocalActuationWireBody()`.
struct ParsedLocalActuationMessage {
  STACK_ALLOCATED();

 public:
  base::span<const uint8_t> info_plist;
  base::span<const uint8_t> payload;
};

// Splits `body`, the bytes of a received message after `mach_msg_header_t`,
// into its `Info.plist` and payload. `body` must end where the message does
// (at `msgh_size`) and exclude the kernel's trailer. Returns `std::nullopt` if
// `body` is not a valid version-1 message.
std::optional<ParsedLocalActuationMessage> ParseLocalActuationWireBody(
    base::span<const uint8_t> body);

// Returns a message with `msgh_size`, `msgh_id`, and everything after the Mach
// header filled in. The caller sets `msgh_bits` and the ports before sending.
// Returns an empty array if `info_plist` is empty or larger than
// `kLocalActuationMaxInfoPlistSize`, or if the message would exceed
// `kLocalActuationMaxMessageSize`.
base::HeapArray<uint8_t> BuildLocalActuationWireMessage(
    mach_msg_id_t msgh_id,
    base::span<const uint8_t> info_plist,
    base::span<const uint8_t> payload);

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_WIRE_FORMAT_H_
