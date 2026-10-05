// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/redirection_remoting_source_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/media/remoting_bridge.h"
#include "content/public/browser/browser_thread.h"

using media::mojom::RemotingStopReason;

RedirectionRemotingSourceBridge::RedirectionRemotingSourceBridge(
    RemotingBridge::Client* client,
    RemotingBridge* bridge,
    mojo::PendingRemote<redirection::mojom::RedirectionSessionHost>
        session_host,
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver,
    base::OnceClosure disconnected_callback)
    : client_(client),
      bridge_(bridge),
      disconnected_callback_(std::move(disconnected_callback)),
      remoter_(std::move(remoter)),
      receiver_(this, std::move(receiver)),
      session_host_(std::move(session_host)) {
  // Any one of these closing means the utility process can no longer serve
  // this source, whether because the session failed or because the process
  // itself went away.
  auto disconnected = base::BindRepeating(
      &RedirectionRemotingSourceBridge::OnSessionDisconnected,
      base::Unretained(this));
  remoter_.set_disconnect_handler(disconnected);
  receiver_.set_disconnect_handler(disconnected);
  session_host_.set_disconnect_handler(disconnected);
}

RedirectionRemotingSourceBridge::~RedirectionRemotingSourceBridge() = default;

void RedirectionRemotingSourceBridge::Start() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  is_started_ = true;
  remoter_->Start();
}

void RedirectionRemotingSourceBridge::StartRemotingDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  remoter_->StartDataStreams(std::move(audio_pipe), std::move(video_pipe),
                             std::move(audio_sender), std::move(video_sender));
}

void RedirectionRemotingSourceBridge::SendMessageToSink(
    const std::vector<uint8_t>& message) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  remoter_->SendMessageToSink(message);
}

void RedirectionRemotingSourceBridge::StopRemoting(RemotingStopReason reason) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  // Prevent the source from trying to start again before the utility process
  // has torn the session down.
  bridge_->OnSinkGone(client_);

  // A source that never started remoting would read OnStopped() as the end of
  // a session it does not believe it is in.
  if (!is_started_) {
    return;
  }
  is_started_ = false;

  remoter_->Stop(reason);

  bridge_->OnStopped(client_, reason);
}

void RedirectionRemotingSourceBridge::OnSinkAvailable(
    media::mojom::RemotingSinkMetadataPtr metadata) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  // The session describes the codecs it can accept. RENDERING says the source
  // may hand its media pipeline to the remote renderer.
  metadata->features.push_back(media::mojom::RemotingSinkFeature::RENDERING);
  bridge_->OnSinkAvailable(client_, *metadata);
}

void RedirectionRemotingSourceBridge::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  bridge_->OnMessageFromSink(client_, message);
}

void RedirectionRemotingSourceBridge::OnStopped(
    media::mojom::RemotingStopReason reason) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  StopRemoting(reason);
}

void RedirectionRemotingSourceBridge::OnSinkGone() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  StopRemoting(RemotingStopReason::SERVICE_GONE);
}

void RedirectionRemotingSourceBridge::OnStarted() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  bridge_->OnStarted(client_);
}

void RedirectionRemotingSourceBridge::OnStartFailed(
    media::mojom::RemotingStartFailReason reason) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  is_started_ = false;
  bridge_->OnStartFailed(client_, reason);
}

void RedirectionRemotingSourceBridge::OnSessionDisconnected() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (!disconnected_callback_) {
    return;
  }

  // Report why this source stopped, before the teardown below reports a bare
  // route termination.
  StopRemoting(RemotingStopReason::UNEXPECTED_FAILURE);

  // This destroys `this`, so nothing may follow it.
  std::move(disconnected_callback_).Run();
}
