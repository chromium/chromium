// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/fake_audio_stream_factory.h"

#include <utility>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "media/audio/audio_io.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace readaloud {

FakeAudioStreamFactory::FakeAudioStreamFactory() = default;
FakeAudioStreamFactory::~FakeAudioStreamFactory() = default;

void FakeAudioStreamFactory::Bind(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void FakeAudioStreamFactory::Disconnect() {
  receiver_.reset();
}

void FakeAudioStreamFactory::CreateOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  last_device_id_ = device_id;
  last_params_ = params;
  last_group_id_ = group_id;
  last_stream_receiver_ = std::move(stream);
  last_callback_ = std::move(callback);
  create_output_stream_called_count_++;

  if (create_output_stream_callback_) {
    std::move(create_output_stream_callback_).Run();
  }

  if (auto_respond_) {
    RespondWithLastCallback(should_succeed_);
  }
}

void FakeAudioStreamFactory::RespondWithLastCallback(bool succeed) {
  if (last_callback_.is_null()) {
    return;
  }
  if (!succeed) {
    std::move(last_callback_).Run(nullptr);
    return;
  }
  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      CreateValidDataPipe(last_params_);
  std::move(last_callback_).Run(std::move(data_pipe));
}

media::mojom::ReadWriteAudioDataPipePtr
FakeAudioStreamFactory::CreateValidDataPipe(
    const media::AudioParameters& params) {
  uint32_t buffer_size = media::ComputeAudioOutputBufferSize(params);
  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!shared_memory_region.IsValid()) {
    return nullptr;
  }

  base::CancelableSyncSocket local_socket;
  base::CancelableSyncSocket foreign_socket;
  if (!base::CancelableSyncSocket::CreatePair(&local_socket, &foreign_socket)) {
    return nullptr;
  }

  return media::mojom::ReadWriteAudioDataPipe::New(
      std::move(shared_memory_region),
      mojo::PlatformHandle(foreign_socket.Take()));
}

}  // namespace readaloud
