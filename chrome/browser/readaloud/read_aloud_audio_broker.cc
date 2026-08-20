// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_audio_broker.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "content/public/browser/audio_service.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace readaloud {

ReadAloudAudioBroker::ReadAloudAudioBroker(
    AudioStreamFactoryBinder factory_binder)
    : factory_binder_(factory_binder
                          ? std::move(factory_binder)
                          : content::GetAudioServiceStreamFactoryBinder()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ReadAloudAudioBroker::~ReadAloudAudioBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReadAloudAudioBroker::CreateOutputStream(
    const base::UnguessableToken& group_id,
    const media::AudioParameters& params,
    CreateStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(params.IsValid());

  EnsureFactoryConnected();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote;
  auto stream_receiver = stream_remote.InitWithNewPipeAndPassReceiver();

  stream_factory_->CreateOutputStream(
      std::move(stream_receiver),
      /*observer=*/mojo::NullAssociatedRemote(),
      /*log=*/mojo::NullRemote(),
      /*device_id=*/media::AudioDeviceDescription::kDefaultDeviceId, params,
      group_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&ReadAloudAudioBroker::OnStreamCreated,
                         weak_factory_.GetWeakPtr(), std::move(stream_remote),
                         std::move(callback)),
          nullptr));
}

void ReadAloudAudioBroker::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  stream_factory_.reset();
}

void ReadAloudAudioBroker::EnsureFactoryConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_factory_.is_bound()) {
    return;
  }

  factory_binder_.Run(stream_factory_.BindNewPipeAndPassReceiver());

  stream_factory_.set_disconnect_handler(base::BindOnce(
      &ReadAloudAudioBroker::OnFactoryDisconnect, weak_factory_.GetWeakPtr()));
}

void ReadAloudAudioBroker::OnFactoryDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_factory_.reset();
}

void ReadAloudAudioBroker::OnStreamCreated(
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
    CreateStreamCallback callback,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(std::move(stream_remote), std::move(data_pipe));
}

}  // namespace readaloud
