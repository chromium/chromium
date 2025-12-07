// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROBE_MESSAGE_H_
#define CHROME_BROWSER_NET_PROBE_MESSAGE_H_

#include <stdint.h>

#include <array>
#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/net/probe_message.pb.h"

namespace chrome_browser_net {

// Packet format between client and server is defined in probe_message.proto.
class ProbeMessage {
 public:
  ProbeMessage();

  ProbeMessage(const ProbeMessage&) = delete;
  ProbeMessage& operator=(const ProbeMessage&) = delete;

  // Generate a ProbeRequest packet.
  void GenerateProbeRequest(const ProbePacket_Token& received_token,
                            uint32_t group_id,
                            uint32_t probe_size,
                            uint32_t pacing_interval_micros,
                            uint32_t number_probe_packets,
                            ProbePacket* output);
  // Make an encoded packet (string) from a ProbePacket object.
  std::string MakeEncodedPacket(const ProbePacket& packet) const;
  // Fill some common fields in the packet header.
  void SetPacketHeader(ProbePacket_Type packet_type,
                       ProbePacket* probe_packet) const;
  // Parse the input string (which is an encoded packet) and save the result to
  // packet. Return true if there is no error and false otherwise.
  bool ParseInput(const std::string& input, ProbePacket* packet) const;

  static constexpr uint32_t kMaxProbePacketBytes = 1500;

 private:
  // For unittest.
  friend class ProbeMessageTest;
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestChecksum);
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestEncode);
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestGenerateProbeRequest);
  FRIEND_TEST_ALL_PREFIXES(ProbeMessageTest, TestSetPacketHeader);

  // Compute the checksum of the padding string.
  uint32_t Checksum(const std::string& str) const;

  // Encode the packet with kEncodingString. This is also used for decoding.
  std::string Encode(const std::string& input) const;

  static constexpr uint32_t kVersion = 2;
  static constexpr uint32_t kMaxNumberProbePackets = 21;
  // Maximum pacing interval is 300 seconds (for testing NAT binding).
  static constexpr uint32_t kMaxPacingIntervalMicros = 300000000;
  static constexpr auto kEncodingString = std::to_array<const uint8_t>({
      'T',  0xd3, '?',  0xa5, 'h',  '2',  0x9c, 0x8e, 'n',  0xf1, 'Q',  '6',
      0xbc, '{',  0xc6, '-',  '4',  0xfa, '$',  'f',  0xb9, '[',  0xa6, 0xcd,
      '@',  '6',  ',',  0xdf, 0xb3, 'i',  '-',  0xe6, 'v',  0x9e, 'V',  0x8d,
      'X',  'd',  0xd9, 'k',  'E',  0xf6, '=',  0xbe, 'O',
  });
};
}       // namespace chrome_browser_net
#endif  // CHROME_BROWSER_NET_PROBE_MESSAGE_H_
