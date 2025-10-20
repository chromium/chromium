// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_RECEIVER_APP_UI_AUDIO_PACKET_CONVERTER_H_
#define ASH_WEBUI_BOCA_RECEIVER_APP_UI_AUDIO_PACKET_CONVERTER_H_

#include <memory>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"

namespace remoting {
class AudioPacket;
}  // namespace remoting

namespace ash::boca_receiver {

// Converts a remoting::AudioPacket proto to a mojom::DecodedAudioPacket.
// Returns a nullptr if the conversion fails due to unexpected packet
// properties.
boca_receiver::mojom::DecodedAudioPacketPtr ConvertAudioPacketToMojom(
    std::unique_ptr<remoting::AudioPacket> proto_packet);

}  // namespace ash::boca_receiver

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_AUDIO_PACKET_CONVERTER_H_
