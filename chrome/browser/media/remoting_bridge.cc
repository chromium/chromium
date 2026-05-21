// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/remoting_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

RemotingBridge::RemotingBridge(
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    Client* client)
    : source_(std::move(source)), client_(client) {
  DCHECK(client_);
  source_.set_disconnect_handler(
      base::BindOnce(&RemotingBridge::Stop, base::Unretained(this),
                     RemotingStopReason::SOURCE_GONE));
  client_->RegisterBridge(this);
}

RemotingBridge::~RemotingBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->DeregisterBridge(this, RemotingStopReason::SOURCE_GONE);
  }
}

// static
void RemotingBridge::CreateMediaRemoter(
    Client* client,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  if (!client) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemotingBridge>(std::move(source), client),
      std::move(receiver));
}

void RemotingBridge::OnSinkAvailable(const RemotingSinkMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnSinkAvailable(metadata.Clone());
}

void RemotingBridge::OnSinkGone() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnSinkGone();
}

void RemotingBridge::OnStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnStarted();
}

void RemotingBridge::OnStartFailed(RemotingStartFailReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnStartFailed(reason);
}

void RemotingBridge::OnMessageFromSink(const std::vector<uint8_t>& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnMessageFromSink(message);
}

void RemotingBridge::OnStopped(RemotingStopReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnStopped(reason);
}

void RemotingBridge::OnClientDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_ = nullptr;
}

void RemotingBridge::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->StartRemoting(this);
  }
}

void RemotingBridge::StartWithPermissionAlreadyGranted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->StartWithPermissionAlreadyGranted(this);
  }
}

void RemotingBridge::StartDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->StartRemotingDataStreams(
        this, std::move(audio_pipe), std::move(video_pipe),
        std::move(audio_sender), std::move(video_sender));
  }
}

void RemotingBridge::Stop(RemotingStopReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->StopRemoting(this, reason, true);
  }
}

void RemotingBridge::SendMessageToSink(const std::vector<uint8_t>& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->SendMessageToSink(this, message);
  }
}

void RemotingBridge::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client_) {
    client_->EstimateTransmissionCapacity(std::move(callback));
  } else {
    std::move(callback).Run(0);
  }
}
