// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/remoting_bridge.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

RemotingBridge::RemotingBridge(
    base::span<Client* const> clients,
    mojo::PendingRemote<media::mojom::RemotingSource> source)
    : source_(std::move(source)) {
  CHECK(!clients.empty());
  source_.set_disconnect_handler(
      base::BindOnce(&RemotingBridge::Stop, base::Unretained(this),
                     RemotingStopReason::SOURCE_GONE));

  for (Client* client : clients) {
    CHECK(client);
    clients_.push_back({.client = client});
  }

  // Registering is deferred until |clients_| is complete, since a Client may
  // report its availability from within RegisterBridge(), and a lower-priority
  // one must not be activated before its betters are known.
  for (const ClientEntry& entry : clients_) {
    entry.client->RegisterBridge(this);
  }
}

RemotingBridge::~RemotingBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const ClientEntry& entry : clients_) {
    if (entry.client) {
      entry.client->DeregisterBridge(this, RemotingStopReason::SOURCE_GONE);
    }
  }
}

// static
void RemotingBridge::CreateMediaRemoter(
    base::span<Client* const> clients,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemotingBridge>(clients, std::move(source)),
      std::move(receiver));
}

void RemotingBridge::OnSinkAvailable(Client* client,
                                     const RemotingSinkMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client != active_client_) {
    return;
  }
  source_->OnSinkAvailable(metadata.Clone());
}

void RemotingBridge::OnSinkGone(Client* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client != active_client_) {
    return;
  }
  source_->OnSinkGone();
}

void RemotingBridge::OnStarted(Client* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client != active_client_) {
    return;
  }
  source_->OnStarted();
}

void RemotingBridge::OnStartFailed(Client* client,
                                   RemotingStartFailReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client != active_client_) {
    return;
  }
  source_->OnStartFailed(reason);
}

void RemotingBridge::OnMessageFromSink(Client* client,
                                       const std::vector<uint8_t>& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client != active_client_) {
    return;
  }
  source_->OnMessageFromSink(message);
}

void RemotingBridge::OnStopped(Client* client, RemotingStopReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (client != active_client_) {
    return;
  }
  source_->OnStopped(reason);
}

void RemotingBridge::OnClientDestroyed(Client* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetEntry(client).Reset();

  if (client == active_client_) {
    active_client_ = nullptr;
  }

  UpdateActiveClient();
}

void RemotingBridge::SetClientAvailable(Client* client, bool available) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetEntry(client).is_available = available;

  UpdateActiveClient();
}

RemotingBridge::ClientEntry& RemotingBridge::GetEntry(Client* client) {
  CHECK(client);
  auto it = std::ranges::find(clients_, client, &ClientEntry::client);
  CHECK(it != clients_.end());
  return *it;
}

void RemotingBridge::UpdateActiveClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The most-preferred Client that can currently serve the source wins.
  auto it = std::ranges::find_if(
      clients_, [](const ClientEntry& entry) { return entry.is_ready(); });
  Client* preferred_client = it == clients_.end() ? nullptr : it->client.get();

  if (preferred_client == active_client_) {
    return;
  }

  // The outgoing Client is notified while it is still |active_client_|, so
  // that the OnSinkGone() it sends reaches the source.
  if (active_client_) {
    active_client_->OnClientDeactivated(this);
  }
  active_client_ = preferred_client;
  if (active_client_) {
    active_client_->OnClientActivated(this);
  }
}

void RemotingBridge::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!active_client_) {
    // The source asked to start after the sink it was told about went away.
    // It is owed an answer either way, or it waits forever.
    source_->OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
    return;
  }
  active_client_->StartRemoting(this);
}

void RemotingBridge::StartWithPermissionAlreadyGranted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!active_client_) {
    source_->OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
    return;
  }
  active_client_->StartWithPermissionAlreadyGranted(this);
}

void RemotingBridge::StartDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_client_) {
    active_client_->StartRemotingDataStreams(
        this, std::move(audio_pipe), std::move(video_pipe),
        std::move(audio_sender), std::move(video_sender));
  }
}

void RemotingBridge::Stop(RemotingStopReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_client_) {
    active_client_->StopRemoting(this, reason, true);
  }
}

void RemotingBridge::SendMessageToSink(const std::vector<uint8_t>& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_client_) {
    active_client_->SendMessageToSink(this, message);
  }
}

void RemotingBridge::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_client_) {
    active_client_->EstimateTransmissionCapacity(std::move(callback));
  } else {
    std::move(callback).Run(0);
  }
}
