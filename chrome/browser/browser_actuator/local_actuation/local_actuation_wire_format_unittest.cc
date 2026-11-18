// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/local_actuation/local_actuation_wire_format.h"

#include <mach/message.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

constexpr std::string_view kSamplePlist = "<plist>caller</plist>";

TEST(LocalActuationWireFormatTest, RoundTripsMessageWithPayload) {
  const auto plist = base::as_byte_span(kSamplePlist);
  const uint8_t payload_bytes[] = {0x0a, 0x03, 'a', 'b', 'c'};

  base::HeapArray<uint8_t> message = BuildLocalActuationWireMessage(
      kLocalActuationRequestMsgId, plist, payload_bytes);
  ASSERT_FALSE(message.empty());

  const auto* header =
      reinterpret_cast<const mach_msg_header_t*>(message.data());
  EXPECT_EQ(header->msgh_id, kLocalActuationRequestMsgId);
  EXPECT_EQ(header->msgh_size, message.size());
  EXPECT_EQ(header->msgh_bits, 0u);
  EXPECT_EQ(header->msgh_remote_port, static_cast<mach_port_t>(MACH_PORT_NULL));
  EXPECT_EQ(header->msgh_local_port, static_cast<mach_port_t>(MACH_PORT_NULL));

  std::optional<ParsedLocalActuationMessage> parsed =
      ParseLocalActuationWireBody(message.subspan(sizeof(mach_msg_header_t)));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->info_plist, plist);
  EXPECT_EQ(parsed->payload, base::span<const uint8_t>(payload_bytes));
}

TEST(LocalActuationWireFormatTest, RoundTripsEmptyPayload) {
  const auto plist = base::as_byte_span(kSamplePlist);

  base::HeapArray<uint8_t> message =
      BuildLocalActuationWireMessage(kLocalActuationReplyMsgId, plist, {});
  ASSERT_FALSE(message.empty());

  const auto* header =
      reinterpret_cast<const mach_msg_header_t*>(message.data());
  EXPECT_EQ(header->msgh_id, kLocalActuationReplyMsgId);

  std::optional<ParsedLocalActuationMessage> parsed =
      ParseLocalActuationWireBody(message.subspan(sizeof(mach_msg_header_t)));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->info_plist, plist);
  EXPECT_TRUE(parsed->payload.empty());
}

TEST(LocalActuationWireFormatTest, AcceptsZeroToThreeTrailingRoundMsgPadBytes) {
  const auto plist = base::as_byte_span(kSamplePlist);

  for (size_t payload_len = 0; payload_len < sizeof(natural_t); ++payload_len) {
    const std::vector<uint8_t> payload(payload_len, 0x42);
    base::HeapArray<uint8_t> message = BuildLocalActuationWireMessage(
        kLocalActuationRequestMsgId, plist, payload);
    ASSERT_FALSE(message.empty());
    EXPECT_EQ(message.size() % sizeof(natural_t), 0u);

    std::optional<ParsedLocalActuationMessage> parsed =
        ParseLocalActuationWireBody(message.subspan(sizeof(mach_msg_header_t)));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->info_plist, plist);
    EXPECT_EQ(parsed->payload, base::span<const uint8_t>(payload));
  }
}

TEST(LocalActuationWireFormatTest, RejectsTruncatedWireHeader) {
  for (size_t len : {size_t{0}, size_t{4}, size_t{8}, size_t{11}}) {
    const std::vector<uint8_t> short_body(len, 0);
    EXPECT_FALSE(ParseLocalActuationWireBody(short_body).has_value());
  }
}

TEST(LocalActuationWireFormatTest, RejectsUnsupportedProtocolVersion) {
  const auto plist = base::as_byte_span(kSamplePlist);
  for (uint32_t version : {uint32_t{0}, uint32_t{2}}) {
    base::HeapArray<uint8_t> message =
        BuildLocalActuationWireMessage(kLocalActuationRequestMsgId, plist, {});
    ASSERT_FALSE(message.empty());
    base::span<uint8_t> body = message.subspan(sizeof(mach_msg_header_t));
    body.first<4>().copy_from(base::U32ToNativeEndian(version));
    EXPECT_FALSE(ParseLocalActuationWireBody(body).has_value());
  }
}

TEST(LocalActuationWireFormatTest, RejectsEmptyOrOversizedInfoPlistInBody) {
  const auto plist = base::as_byte_span(kSamplePlist);

  // `info_plist_len == 0`.
  {
    base::HeapArray<uint8_t> message =
        BuildLocalActuationWireMessage(kLocalActuationRequestMsgId, plist, {});
    ASSERT_FALSE(message.empty());
    base::span<uint8_t> body = message.subspan(sizeof(mach_msg_header_t));
    body.subspan<4, 4>().copy_from(base::U32ToNativeEndian(0u));
    EXPECT_FALSE(ParseLocalActuationWireBody(body).has_value());
  }

  // `info_plist_len > kLocalActuationMaxInfoPlistSize`.
  {
    std::vector<uint8_t> body(
        sizeof(LocalActuationWireHeader) + kLocalActuationMaxInfoPlistSize + 4,
        'x');
    base::span<uint8_t> body_span(body);
    body_span.first<4>().copy_from(
        base::U32ToNativeEndian(kLocalActuationProtocolVersion));
    body_span.subspan<4, 4>().copy_from(base::U32ToNativeEndian(
        static_cast<uint32_t>(kLocalActuationMaxInfoPlistSize + 1)));
    body_span.subspan<8, 4>().copy_from(base::U32ToNativeEndian(0u));
    EXPECT_FALSE(ParseLocalActuationWireBody(body_span).has_value());
  }
}

TEST(LocalActuationWireFormatTest,
     RejectsOutOfBoundsPayloadOrExcessTrailingBytes) {
  const auto plist = base::as_byte_span(kSamplePlist);
  const uint8_t payload_bytes[] = {1, 2, 3, 4};

  // `payload_len` claims more bytes than the message carries.
  {
    base::HeapArray<uint8_t> message = BuildLocalActuationWireMessage(
        kLocalActuationRequestMsgId, plist, payload_bytes);
    ASSERT_FALSE(message.empty());
    base::span<uint8_t> body = message.subspan(sizeof(mach_msg_header_t));
    body.subspan<8, 4>().copy_from(base::U32ToNativeEndian(32u));
    EXPECT_FALSE(ParseLocalActuationWireBody(body).has_value());
  }

  // `payload_len == UINT32_MAX` does not overflow `size_t` math.
  {
    base::HeapArray<uint8_t> message = BuildLocalActuationWireMessage(
        kLocalActuationRequestMsgId, plist, payload_bytes);
    ASSERT_FALSE(message.empty());
    base::span<uint8_t> body = message.subspan(sizeof(mach_msg_header_t));
    body.subspan<8, 4>().copy_from(
        base::U32ToNativeEndian(std::numeric_limits<uint32_t>::max()));
    EXPECT_FALSE(ParseLocalActuationWireBody(body).has_value());
  }

  // 4 or more trailing bytes beyond `info_plist` + `payload` (more than
  // `round_msg()` padding can add) are rejected.
  {
    base::HeapArray<uint8_t> message = BuildLocalActuationWireMessage(
        kLocalActuationRequestMsgId, plist, payload_bytes);
    ASSERT_FALSE(message.empty());
    base::span<const uint8_t> valid_body =
        message.subspan(sizeof(mach_msg_header_t));
    std::vector<uint8_t> padded(valid_body.begin(), valid_body.end());
    padded.resize(padded.size() + sizeof(natural_t), 0);
    EXPECT_FALSE(ParseLocalActuationWireBody(padded).has_value());
  }
}

TEST(LocalActuationWireFormatTest, BuildRejectsOutOfBoundsInputs) {
  const auto plist = base::as_byte_span(kSamplePlist);

  // Empty `Info.plist`.
  EXPECT_TRUE(
      BuildLocalActuationWireMessage(kLocalActuationRequestMsgId, {}, {})
          .empty());

  // `Info.plist` larger than `kLocalActuationMaxInfoPlistSize`.
  const std::vector<uint8_t> oversized_plist(
      kLocalActuationMaxInfoPlistSize + 1, 'p');
  EXPECT_TRUE(BuildLocalActuationWireMessage(kLocalActuationRequestMsgId,
                                             oversized_plist, {})
                  .empty());

  // Exact `kLocalActuationMaxMessageSize` succeeds; 1 byte over fails.
  const size_t max_payload_len = kLocalActuationMaxMessageSize -
                                 kLocalActuationMinMessageSize - plist.size();
  const std::vector<uint8_t> max_payload(max_payload_len, 'a');
  base::HeapArray<uint8_t> at_limit = BuildLocalActuationWireMessage(
      kLocalActuationRequestMsgId, plist, max_payload);
  EXPECT_EQ(at_limit.size(), kLocalActuationMaxMessageSize);

  const std::vector<uint8_t> over_limit(max_payload_len + 1, 'a');
  EXPECT_TRUE(BuildLocalActuationWireMessage(kLocalActuationRequestMsgId, plist,
                                             over_limit)
                  .empty());
}

}  // namespace
}  // namespace browser_actuator
