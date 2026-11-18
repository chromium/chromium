// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/local_actuation/local_actuation_wire_format.h"

#include <mach/message.h>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/containers/buffer_iterator.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"

namespace browser_actuator {

std::optional<ParsedLocalActuationMessage> ParseLocalActuationWireBody(
    base::span<const uint8_t> body) {
  base::BufferIterator<const uint8_t> iterator(body);
  const std::optional<LocalActuationWireHeader> header =
      iterator.CopyObject<LocalActuationWireHeader>();
  if (!header || header->protocol_version != kLocalActuationProtocolVersion) {
    return std::nullopt;
  }
  if (header->info_plist_len == 0 ||
      header->info_plist_len > kLocalActuationMaxInfoPlistSize) {
    return std::nullopt;
  }

  size_t content_size;
  if (!base::CheckAdd(header->info_plist_len, header->payload_len)
           .AssignIfValid(&content_size)) {
    return std::nullopt;
  }
  // The sender rounds `msgh_size` up to a multiple of `sizeof(natural_t)`, so
  // up to 3 pad bytes may follow `payload`.
  const size_t remaining = body.size() - iterator.position();
  if (content_size > remaining ||
      remaining - content_size >= sizeof(natural_t)) {
    return std::nullopt;
  }
  return ParsedLocalActuationMessage{
      .info_plist = iterator.Span<uint8_t>(header->info_plist_len),
      .payload = iterator.Span<uint8_t>(header->payload_len),
  };
}

base::HeapArray<uint8_t> BuildLocalActuationWireMessage(
    mach_msg_id_t msgh_id,
    base::span<const uint8_t> info_plist,
    base::span<const uint8_t> payload) {
  if (info_plist.empty() ||
      info_plist.size() > kLocalActuationMaxInfoPlistSize) {
    return {};
  }
  size_t unpadded_size;
  if (!base::CheckAdd(kLocalActuationMinMessageSize, info_plist.size(),
                      payload.size())
           .AssignIfValid(&unpadded_size) ||
      unpadded_size > kLocalActuationMaxMessageSize) {
    return {};
  }
  // Rounding up cannot pass the limit.
  static_assert(kLocalActuationMaxMessageSize % sizeof(natural_t) == 0);
  const size_t message_size = round_msg(unpadded_size);

  auto buffer = base::HeapArray<uint8_t>::WithSize(message_size);
  base::BufferIterator<uint8_t> iterator(buffer.as_span());
  auto* mach_header = iterator.MutableObject<mach_msg_header_t>();
  mach_header->msgh_size = static_cast<mach_msg_size_t>(message_size);
  mach_header->msgh_id = msgh_id;
  *iterator.MutableObject<LocalActuationWireHeader>() = {
      .protocol_version = kLocalActuationProtocolVersion,
      .info_plist_len = static_cast<uint32_t>(info_plist.size()),
      .payload_len = static_cast<uint32_t>(payload.size()),
  };
  iterator.MutableSpan<uint8_t>(info_plist.size()).copy_from(info_plist);
  iterator.MutableSpan<uint8_t>(payload.size()).copy_from(payload);
  return buffer;
}

}  // namespace browser_actuator
