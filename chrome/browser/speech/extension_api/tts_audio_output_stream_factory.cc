// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_audio_output_stream_factory.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace chromeos::tts {

// static
mojo::PendingRemote<media::mojom::AudioStreamFactory>
TtsAudioOutputStreamFactory::Create(
    mojo::PendingRemote<media::mojom::AudioStreamFactory>
        audio_service_factory) {
  mojo::PendingRemote<media::mojom::AudioStreamFactory> remote;
  auto receiver_ref = mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(
          new TtsAudioOutputStreamFactory(std::move(audio_service_factory))),
      remote.InitWithNewPipeAndPassReceiver());
  if (receiver_ref) {
    static_cast<TtsAudioOutputStreamFactory*>(receiver_ref->impl())
        ->Initialize(receiver_ref);
  }
  return remote;
}

TtsAudioOutputStreamFactory::TtsAudioOutputStreamFactory(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_service_factory)
    : audio_service_factory_(std::move(audio_service_factory)) {}

TtsAudioOutputStreamFactory::~TtsAudioOutputStreamFactory() = default;

void TtsAudioOutputStreamFactory::Initialize(
    mojo::SelfOwnedReceiverRef<media::mojom::AudioStreamFactory> receiver_ref) {
  audio_service_factory_.set_disconnect_handler(base::BindOnce(
      [](mojo::SelfOwnedReceiverRef<media::mojom::AudioStreamFactory> ref) {
        if (ref) {
          ref->Close();
        }
      },
      receiver_ref));
}

void TtsAudioOutputStreamFactory::CreateOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  audio_service_factory_->CreateOutputStream(
      std::move(stream), std::move(observer), std::move(log), device_id, params,
      group_id, std::move(callback));
}

// Note: We intentionally call bare mojo::ReportBadMessage() rather than
// immediately closing the receiver via SelfOwnedReceiverRef::Close().
// Synchronously calling Close() here would destroy `this` while response
// callbacks are in-flight, triggering fatal DCHECK(!connected) crashes in
// ProxyToResponder::~ProxyToResponder() on debug builds. Bare
// mojo::ReportBadMessage() flags the protocol violation to Mojo while
// allowing response callbacks to safely run with default/null values.
void TtsAudioOutputStreamFactory::CreateSwitchableOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
        device_switch_receiver,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateSwitchableOutputStreamCallback callback) {
  mojo::ReportBadMessage(
      "TtsAudioOutputStreamFactory: CreateSwitchableOutputStream is not "
      "supported");
  std::move(callback).Run(nullptr);
}

void TtsAudioOutputStreamFactory::CreateInputStream(
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
    CreateInputStreamCallback callback) {
  mojo::ReportBadMessage(
      "TtsAudioOutputStreamFactory: CreateInputStream is rejected (TTS is "
      "output-only)");
  std::move(callback).Run(nullptr, /*initially_muted=*/false, std::nullopt);
}

void TtsAudioOutputStreamFactory::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  mojo::ReportBadMessage(
      "TtsAudioOutputStreamFactory: AssociateInputAndOutputForAec is "
      "rejected");
}

void TtsAudioOutputStreamFactory::BindMuter(
    mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver,
    const base::UnguessableToken& group_id) {
  mojo::ReportBadMessage("TtsAudioOutputStreamFactory: BindMuter is rejected");
}

void TtsAudioOutputStreamFactory::CreateLoopbackStream(
    mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
    mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
    mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    const base::UnguessableToken& group_id,
    CreateLoopbackStreamCallback callback) {
  mojo::ReportBadMessage(
      "TtsAudioOutputStreamFactory: CreateLoopbackStream is rejected (TTS is "
      "output-only)");
  std::move(callback).Run(nullptr);
}

}  // namespace chromeos::tts
