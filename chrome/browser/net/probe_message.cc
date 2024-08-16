// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/net/probe_message.h"

#include <stddef.h>

#include <string>

#include "base/check_op.h"
#include "base/logging.h"

namespace chrome_browser_net {

const uint32_t ProbeMessage::kVersion = 2;
const uint32_t ProbeMessage::kMaxNumberProbePackets = 21;
const uint32_t ProbeMessage::kMaxProbePacketBytes = 1500;
// Maximum pacing interval is 300 seconds (for testing NAT binding).
const uint32_t ProbeMessage::kMaxPacingIntervalMicros = 300000000;
const char ProbeMessage::kEncodingString[] =
    "T\xd3?\xa5h2\x9c\x8en\xf1Q6\xbc{\xc6-4\xfa$f\xb9[\xa6\xcd@6,\xdf\xb3i-\xe6"
    "v\x9eV\x8dXd\xd9kE\xf6=\xbeO";

ProbeMessage::ProbeMessage() {}

bool ProbeMessage::ParseInput(const std::string& input,
                              ProbePacket* probe_packet) const {
  // Encode is used for decoding here.
  std::string input_decoded = Encode(input);

  bool parse_result = probe_packet->ParseFromString(input_decoded);
  if (!parse_result) {
    DVLOG(1) << "ProtoBuffer string parsing error. Input size:" << input.size();
    return false;
  }
  const ProbePacket_Header& header = probe_packet->header();
  DVLOG(2) << "version " << header.version() << " checksum "
           << header.checksum() << " type " << header.type();
  if (header.version() != kVersion) {
    DVLOG(1) << "Bad version number: " << header.version()
             << " expected: " << kVersion;
    return false;
  }

  // Checksum is computed on padding only.
  if (probe_packet->has_padding()) {
    DVLOG(3) << "received padding: " << probe_packet->padding();
    uint32_t computed_checksum = Checksum(probe_packet->padding());
    if (computed_checksum != header.checksum()) {
      DVLOG(1) << "Checksum mismatch.  Got: " << header.checksum()
               << " expected: " << computed_checksum;
      return false;
    }
  }

  if (header.type() != ProbePacket_Type_HELLO_REPLY &&
      header.type() != ProbePacket_Type_PROBE_REPLY) {
    DVLOG(1) << "Received unknown packet type:" << header.type();
    return false;
  }
  return true;
}

uint32_t ProbeMessage::Checksum(const std::string& str) const {
  uint32_t ret = 0;
  for (std::string::const_iterator i = str.begin(); i != str.end(); ++i) {
    ret += static_cast<uint8_t>(*i);
  }
  return ret;
}

void ProbeMessage::GenerateProbeRequest(const ProbePacket_Token& token,
                                        uint32_t group_id,
                                        uint32_t probe_size,
                                        uint32_t pacing_interval_micros,
                                        uint32_t number_probe_packets,
                                        ProbePacket* probe_packet) {
  DCHECK_LE(number_probe_packets, kMaxNumberProbePackets);
  DCHECK_LE(probe_size, kMaxProbePacketBytes);
  DCHECK_LE(pacing_interval_micros, kMaxPacingIntervalMicros);

  SetPacketHeader(ProbePacket_Type_PROBE_REQUEST, probe_packet);
  *(probe_packet->mutable_token()) = token;
  probe_packet->set_group_id(group_id);
  probe_packet->set_probe_size_bytes(probe_size);
  probe_packet->set_pacing_interval_micros(pacing_interval_micros);
  probe_packet->set_number_probe_packets(number_probe_packets);

  // Add padding to mitigate amplification attack.
  std::string* padding = probe_packet->mutable_padding();
  int padding_size = probe_size - probe_packet->ByteSize();
  padding->append(std::string(std::max(0, padding_size), 0));
  probe_packet->mutable_header()->set_checksum(Checksum(*padding));
  DVLOG(3) << "Request size " << probe_packet->ByteSize() << " probe size "
           << probe_size;
  DCHECK_LE(probe_size, static_cast<uint32_t>(probe_packet->ByteSize()));
}

void ProbeMessage::SetPacketHeader(ProbePacket_Type packet_type,
                                   ProbePacket* probe_packet) const {
  ProbePacket_Header* header = probe_packet->mutable_header();
  header->set_version(kVersion);
  header->set_type(packet_type);
}

std::string ProbeMessage::Encode(const std::string& input) const {

  std::string output(input.size(), 0);
  int key_pos = 0;
  // kEncodingString contains a ending '\0' character, excluded for encoding.
  int key_size = sizeof(kEncodingString) - 1;
  for (size_t i = 0; i < input.size(); ++i) {
    output[i] = input[i] ^ kEncodingString[key_pos];
    ++key_pos;
    if (key_pos >= key_size)
      key_pos = 0;
  }
  return output;
}

std::string ProbeMessage::MakeEncodedPacket(
    const ProbePacket& probe_packet) const {
  std::string output;
  probe_packet.SerializeToString(&output);
  return Encode(output);
}

}  // namespace chrome_browser_net
