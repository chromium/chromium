// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_DECODER_DECODER_ENGINE_H_
#define ASH_SERVICES_IME_DECODER_DECODER_ENGINE_H_

#include "ash/services/ime/ime_decoder.h"
#include "ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "ash/services/ime/public/mojom/input_engine.mojom.h"
#include "ash/services/ime/public/mojom/input_method.mojom.h"
#include "ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "base/scoped_native_library.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace ime {

// A Mojo wrapper around a "decoder" that converts key events and pointer events
// to text. The built-in Chrome OS XKB extension communicates with this to
// implement its IMEs.
class DecoderEngine : public mojom::InputChannel {
 public:
  explicit DecoderEngine(ImeCrosPlatform* platform,
                         absl::optional<ImeDecoder::EntryPoints> entry_points);

  DecoderEngine(const DecoderEngine&) = delete;
  DecoderEngine& operator=(const DecoderEngine&) = delete;

  ~DecoderEngine() override;

  // Binds the mojom::InputChannel interface to this object and returns true if
  // the given ime_spec is supported by the engine.
  bool BindRequest(const std::string& ime_spec,
                   mojo::PendingReceiver<mojom::InputChannel> receiver,
                   mojo::PendingRemote<mojom::InputChannel> remote,
                   const std::vector<uint8_t>& extra);

  // mojom::InputChannel:
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;

 private:
  ImeCrosPlatform* platform_ = nullptr;
  absl::optional<ImeDecoder::EntryPoints> decoder_entry_points_;
  mojo::ReceiverSet<mojom::InputChannel> decoder_channel_receivers_;
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_SERVICES_IME_DECODER_DECODER_ENGINE_H_
