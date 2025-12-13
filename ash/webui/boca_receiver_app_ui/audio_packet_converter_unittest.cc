// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/audio_packet_converter.h"

#include <array>
#include <cstdint>
#include <memory>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "base/values.h"
#include "remoting/proto/audio.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

std::unique_ptr<remoting::AudioPacket> CreateValidPacket() {
  std::unique_ptr<remoting::AudioPacket> packet =
      std::make_unique<remoting::AudioPacket>();
  packet->set_encoding(remoting::AudioPacket::ENCODING_RAW);
  packet->set_bytes_per_sample(remoting::AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_sampling_rate(remoting::AudioPacket::SAMPLING_RATE_48000);
  packet->set_channels(remoting::AudioPacket::CHANNELS_STEREO);
  const int16_t placeholder_data[] = {1, 2, 3, 4};
  packet->add_data(reinterpret_cast<const char*>(placeholder_data),
                   sizeof(placeholder_data));
  return packet;
}

TEST(AudioPacketConverterTest, ConvertValidPacket) {
  std::unique_ptr<remoting::AudioPacket> proto_packet = CreateValidPacket();
  const std::array<int16_t, 5> test_data = {32767, -32768, 0, 12345, -1};
  proto_packet->set_data(0, reinterpret_cast<const char*>(test_data.data()),
                         sizeof(test_data));

  mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(proto_packet));

  ASSERT_TRUE(mojom_packet);
  EXPECT_EQ(mojom_packet->sample_rate,
            remoting::AudioPacket::SAMPLING_RATE_48000);
  EXPECT_EQ(mojom_packet->channels, remoting::AudioPacket::CHANNELS_STEREO);
  ASSERT_EQ(mojom_packet->data.size(), std::size(test_data));
  for (size_t i = 0; i < test_data.size(); ++i) {
    EXPECT_EQ(mojom_packet->data[i], test_data[i]);
  }
}

TEST(AudioPacketConverterTest, InvalidEncoding) {
  std::unique_ptr<remoting::AudioPacket> proto_packet = CreateValidPacket();
  proto_packet->set_encoding(remoting::AudioPacket::ENCODING_OPUS);
  mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(proto_packet));
  EXPECT_TRUE(mojom_packet.is_null());
}

TEST(AudioPacketConverterTest, InvalidBytesPerSample) {
  std::unique_ptr<remoting::AudioPacket> proto_packet = CreateValidPacket();
  proto_packet->set_bytes_per_sample(
      remoting::AudioPacket::BYTES_PER_SAMPLE_INVALID);
  mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(proto_packet));
  EXPECT_TRUE(mojom_packet.is_null());
}

TEST(AudioPacketConverterTest, InvalidSamplingRate) {
  std::unique_ptr<remoting::AudioPacket> proto_packet = CreateValidPacket();
  proto_packet->set_sampling_rate(remoting::AudioPacket::SAMPLING_RATE_INVALID);
  mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(proto_packet));
  EXPECT_TRUE(mojom_packet.is_null());
}

TEST(AudioPacketConverterTest, InvalidChannels) {
  std::unique_ptr<remoting::AudioPacket> proto_packet = CreateValidPacket();
  proto_packet->set_channels(remoting::AudioPacket::CHANNELS_INVALID);
  mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(proto_packet));
  EXPECT_TRUE(mojom_packet.is_null());
}

TEST(AudioPacketConverterTest, MultipleDataEntries) {
  std::unique_ptr<remoting::AudioPacket> proto_packet = CreateValidPacket();
  // Audio packets are only expected to have one entry in `data`.
  proto_packet->add_data("more data");
  mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(proto_packet));
  EXPECT_TRUE(mojom_packet.is_null());
}

}  // namespace
}  // namespace ash::boca_receiver
