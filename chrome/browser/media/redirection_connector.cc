// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/redirection_connector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/media/redirection_remoting_source_bridge.h"
#include "content/public/browser/browser_thread.h"

using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

// static
RedirectionConnector* RedirectionConnector::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<RedirectionConnector> instance;
  return instance.get();
}

RedirectionConnector::RedirectionConnector() = default;

RedirectionConnector::~RedirectionConnector() = default;

void RedirectionConnector::StartingRedirection(
    CreateRedirectionSessionCallback create_session_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  create_session_callback_ = std::move(create_session_callback);

  for (const auto& [bridge, source_bridge] : bridges_) {
    bridge->SetClientAvailable(this, true);
  }
}

void RedirectionConnector::StoppingRedirection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!create_session_callback_) {
    return;
  }
  create_session_callback_.Reset();

  for (const auto& [bridge, source_bridge] : bridges_) {
    bridge->SetClientAvailable(this, false);
  }
}

void RedirectionConnector::RegisterBridge(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!bridges_.contains(bridge));

  bridges_[bridge] = nullptr;
  bridge->SetClientAvailable(this, !create_session_callback_.is_null());
}

void RedirectionConnector::DeregisterBridge(RemotingBridge* bridge,
                                            RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());

  if (it->second) {
    it->second->StopRemoting(reason);
  }
  bridges_.erase(it);
}

void RedirectionConnector::StartRemoting(RemotingBridge* bridge) {
  StartWithPermissionAlreadyGranted(bridge);
}

void RedirectionConnector::StartWithPermissionAlreadyGranted(
    RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());

  if (!it->second) {
    DVLOG(2) << "Remoting start failed: no redirection session.";
    bridge->OnStartFailed(this,
                          RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
    return;
  }

  it->second->Start();
}

void RedirectionConnector::StartRemotingDataStreams(
    RemotingBridge* bridge,
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());

  if (!it->second) {
    return;
  }

  // Error out early if neither pipe was provided, or if a sender receiver is
  // missing for a pipe that was.
  if ((!audio_pipe.is_valid() && !video_pipe.is_valid()) ||
      (audio_pipe.is_valid() && !audio_sender.is_valid()) ||
      (video_pipe.is_valid() && !video_sender.is_valid())) {
    it->second->StopRemoting(RemotingStopReason::DATA_SEND_FAILED);
    return;
  }

  it->second->StartRemotingDataStreams(
      std::move(audio_pipe), std::move(video_pipe), std::move(audio_sender),
      std::move(video_sender));
}

void RedirectionConnector::StopRemoting(RemotingBridge* bridge,
                                        RemotingStopReason reason,
                                        bool is_initiated_by_source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());

  if (it->second) {
    it->second->StopRemoting(reason);
  }
}

void RedirectionConnector::SendMessageToSink(
    RemotingBridge* bridge,
    const std::vector<uint8_t>& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());

  if (it->second) {
    it->second->SendMessageToSink(message);
  }
}

void RedirectionConnector::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  std::move(callback).Run(0);
}

void RedirectionConnector::OnClientActivated(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(create_session_callback_);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());
  CHECK(!it->second);

  mojo::PendingRemote<redirection::mojom::RedirectionSessionHost> session_host;
  mojo::PendingRemote<media::mojom::Remoter> remoter;
  mojo::PendingRemote<media::mojom::RemotingSource> source;
  auto session_host_receiver = session_host.InitWithNewPipeAndPassReceiver();
  auto remoter_receiver = remoter.InitWithNewPipeAndPassReceiver();
  auto source_receiver = source.InitWithNewPipeAndPassReceiver();

  create_session_callback_.Run(std::move(session_host_receiver),
                               std::move(remoter_receiver), std::move(source));

  it->second = std::make_unique<RedirectionRemotingSourceBridge>(
      this, bridge, std::move(session_host), std::move(remoter),
      std::move(source_receiver),
      // A dropped session hands the source back to any other client.
      base::BindOnce(&RemotingBridge::SetClientAvailable,
                     base::Unretained(bridge), this, false));
}

void RedirectionConnector::OnClientDeactivated(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = bridges_.find(bridge);
  CHECK(it != bridges_.end());
  CHECK(it->second);

  it->second->StopRemoting(RemotingStopReason::ROUTE_TERMINATED);
  it->second.reset();
}
