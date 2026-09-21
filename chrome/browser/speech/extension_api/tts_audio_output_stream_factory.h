// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_AUDIO_OUTPUT_STREAM_FACTORY_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_AUDIO_OUTPUT_STREAM_FACTORY_H_

#include <string>

#include "base/unguessable_token.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos::tts {

// A browser-hosted AudioStreamFactory implementation that restricts calls to
// output-only operations (CreateOutputStream), ensuring the TTS utility
// requests only audio playback streams.
// TODO(crbug.com/554915521): Migrate to an upstream stream-specific factory
// interface (e.g. media::mojom::AudioOutputStreamFactory) or shared helper once
// media::mojom / services/audio provides it.
class TtsAudioOutputStreamFactory : public media::mojom::AudioStreamFactory {
 public:
  // Creates a self-owned receiver for TtsAudioOutputStreamFactory and returns a
  // PendingRemote to be passed to untrusted/sandboxed callers like TtsService.
  static mojo::PendingRemote<media::mojom::AudioStreamFactory> Create(
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          audio_service_factory);

  TtsAudioOutputStreamFactory(const TtsAudioOutputStreamFactory&) = delete;
  TtsAudioOutputStreamFactory& operator=(const TtsAudioOutputStreamFactory&) =
      delete;

  ~TtsAudioOutputStreamFactory() override;

  // media::mojom::AudioStreamFactory:
  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback callback) override;

  void CreateSwitchableOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
          device_switch_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateSwitchableOutputStreamCallback callback) override;

  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      uint32_t shared_memory_count,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback callback) override;

  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override;

  void BindMuter(
      mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver,
      const base::UnguessableToken& group_id) override;

  void CreateLoopbackStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      const base::UnguessableToken& group_id,
      CreateLoopbackStreamCallback callback) override;

 private:
  explicit TtsAudioOutputStreamFactory(
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          audio_service_factory);

  void Initialize(mojo::SelfOwnedReceiverRef<media::mojom::AudioStreamFactory>
                      receiver_ref);

  mojo::Remote<media::mojom::AudioStreamFactory> audio_service_factory_;
};

}  // namespace chromeos::tts

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_AUDIO_OUTPUT_STREAM_FACTORY_H_
