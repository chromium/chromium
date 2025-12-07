// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/audio_packet_converter.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "remoting/proto/audio.pb.h"

namespace ash::boca_receiver {

ash::boca_receiver::mojom::DecodedAudioPacketPtr ConvertAudioPacketToMojom(
    std::unique_ptr<remoting::AudioPacket> proto_packet) {
  ash::boca_receiver::mojom::DecodedAudioPacketPtr output_mojo_packet =
      ash::boca_receiver::mojom::DecodedAudioPacket::New();

  if (proto_packet->encoding() != remoting::AudioPacket::ENCODING_RAW ||
      proto_packet->bytes_per_sample() !=
          remoting::AudioPacket::BYTES_PER_SAMPLE_2 ||
      proto_packet->sampling_rate() ==
          remoting::AudioPacket::SAMPLING_RATE_INVALID ||
      proto_packet->channels() == remoting::AudioPacket::CHANNELS_INVALID) {
    LOG(ERROR) << "Unexpected audio packet settings";
    return nullptr;
  }

  if (proto_packet->data().size() != 1) {
    LOG(ERROR) << "Unexpected audio packet size";
    return nullptr;
  }

  output_mojo_packet->sample_rate = proto_packet->sampling_rate();
  output_mojo_packet->channels = proto_packet->channels();

  const std::string& raw_data = proto_packet->data(0);
  const size_t total_size = raw_data.size();
  // The size of an audio sample is 2 bytes. So to find the total number of
  // samples included in this packet divide the total size by int16_t
  // (aka 2 bytes).
  const size_t sample_count = total_size / sizeof(int16_t);

  // Resize the output mojo's data to be an empty of array of `sample_count`
  // elements, each empty slot 2 bytes (size of 1 audio sample) each.
  output_mojo_packet->data.resize(sample_count);

  // The audio data from the input `proto_packet` comes structured as a
  // contiguous sequence of bytes, which means all actions with this data must
  // happen at the 1-byte level.
  // So we need to use `as_writable_bytes()` to view the output mojo's empty
  // array as a contiguous sequence of 1-byte elements in order to perform a
  // safe byte-for-byte copy.
  base::span<uint8_t> output_bytes_span =
      base::as_writable_bytes(base::span(output_mojo_packet->data));

  // Copy the content from the input audio to the output span which is the
  // reference to `output_mojo_packet->data`.
  std::ranges::copy(base::span<const char>(raw_data),
                    output_bytes_span.begin());

  return output_mojo_packet;
}

}  // namespace ash::boca_receiver
