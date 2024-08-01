// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>
#include <string.h>

#include "chrome/browser/net/probe_message.h"
#include "chrome/browser/net/probe_message.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

class ProbeMessageTest : public ::testing::Test {
 protected:
  ProbeMessageTest() {}

  ~ProbeMessageTest() override {}
};

TEST_F(ProbeMessageTest, TestGenerateProbeRequest) {
  ProbeMessage pm;
  ProbePacket_Token token;
  token.set_timestamp_micros(1000000U);
  token.mutable_hash()->assign("1x1x");
  uint32_t group_id = 1;
  uint32_t probe_size = 500;
  uint32_t pacing_interval_micros = 1000000;
  uint32_t number_probe_packets = 21;
  ProbePacket probe_packet;
  pm.GenerateProbeRequest(token,
                          group_id,
                          probe_size,
                          pacing_interval_micros,
                          number_probe_packets,
                          &probe_packet);

  EXPECT_EQ(probe_packet.header().type(), ProbePacket_Type_PROBE_REQUEST);
  EXPECT_EQ(probe_packet.header().version(), ProbeMessage::kVersion);
  EXPECT_EQ(probe_packet.group_id(), group_id);
  EXPECT_EQ(probe_packet.probe_size_bytes(), probe_size);
  EXPECT_EQ(probe_packet.pacing_interval_micros(), pacing_interval_micros);
  EXPECT_EQ(probe_packet.number_probe_packets(), number_probe_packets);
  EXPECT_GE(probe_packet.ByteSize(), static_cast<int>(probe_size));
}

TEST_F(ProbeMessageTest, TestSetPacketHeader) {
  ProbeMessage pm;
  ProbePacket probe_packet;
  pm.SetPacketHeader(ProbePacket_Type_HELLO_REQUEST, &probe_packet);
  EXPECT_EQ(probe_packet.header().type(), ProbePacket_Type_HELLO_REQUEST);
  EXPECT_EQ(probe_packet.header().version(), ProbeMessage::kVersion);

  pm.SetPacketHeader(ProbePacket_Type_PROBE_REPLY, &probe_packet);
  EXPECT_EQ(probe_packet.header().type(), ProbePacket_Type_PROBE_REPLY);
}

TEST_F(ProbeMessageTest, TestMakeEncodePacketAndParseInput) {
  ProbeMessage pm;
  ProbePacket in_packet;
  uint32_t version = 2;
  ProbePacket_Type type = ProbePacket_Type_HELLO_REPLY;
  uint32_t number_probe_packets = 2;
  uint32_t group_id = 5;
  in_packet.mutable_header()->set_version(version);
  in_packet.mutable_header()->set_type(type);
  in_packet.set_number_probe_packets(number_probe_packets);
  in_packet.set_group_id(group_id);

  // Encode it to string.
  std::string output = pm.MakeEncodedPacket(in_packet);
  // Parse to ProbePacket.
  ProbePacket out_packet;
  pm.ParseInput(output, &out_packet);

  EXPECT_EQ(out_packet.header().type(), type);
  EXPECT_EQ(out_packet.header().version(), version);
  EXPECT_EQ(out_packet.number_probe_packets(), number_probe_packets);
  EXPECT_EQ(out_packet.group_id(), group_id);
}

TEST_F(ProbeMessageTest, TestChecksum) {
  ProbeMessage pm;
  std::string str("ABC");
  uint32_t computed_checksum = pm.Checksum(str);
  uint32_t expected_sum = 0;
  for (unsigned i = 0; i < str.size(); ++i)
    expected_sum += static_cast<uint8_t>(str[i]);
  EXPECT_EQ(computed_checksum, expected_sum);
}

TEST_F(ProbeMessageTest, TestEncode) {
  ProbeMessage pm;
  std::string original("ABC");
  std::string output = pm.Encode(original);
  std::string expected_str(original.size(), 0);
  for (unsigned i = 0; i < original.size(); ++i) {
    expected_str[i] = original[i] ^ ProbeMessage::kEncodingString[i];
  }
  EXPECT_EQ(output, expected_str);

  // Do it again to decode.
  std::string twice_encoded = pm.Encode(output);
  EXPECT_EQ(twice_encoded, original);
}

}  // namespace chrome_browser_net
