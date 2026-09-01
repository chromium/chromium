// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_REDIRECTION_REMOTING_SOURCE_BRIDGE_H_
#define CHROME_BROWSER_MEDIA_REDIRECTION_REMOTING_SOURCE_BRIDGE_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/remoting_bridge.h"
#include "chrome/services/redirection/public/mojom/redirection_service.mojom.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Connects a single RemotingBridge to the MediaRemoter serving its source in
// the redirection utility process. One of these exists per remoting source for
// the lifetime of that source's redirection session.
//
// Owned by RedirectionConnector, which destroys it when the session ends or the
// bridge deregisters. The service reports that it can no longer serve this one
// source by closing the pipes below, which is distinct from the utility process
// going away -- that closes every session's pipes and is reported separately by
// RedirectionServiceHost.
class RedirectionRemotingSourceBridge final
    : public media::mojom::RemotingSource {
 public:
  // |client| is the RemotingBridge::Client this bridge relays for; it is only
  // used to tag the notifications sent to |bridge|. |disconnected_callback| is
  // run if the utility process drops this source's session; it may destroy
  // this object.
  RedirectionRemotingSourceBridge(
      RemotingBridge::Client* client,
      RemotingBridge* bridge,
      mojo::PendingRemote<redirection::mojom::RedirectionSessionHost>
          session_host,
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> receiver,
      base::OnceClosure disconnected_callback);
  RedirectionRemotingSourceBridge(const RedirectionRemotingSourceBridge&) =
      delete;
  RedirectionRemotingSourceBridge& operator=(
      const RedirectionRemotingSourceBridge&) = delete;
  ~RedirectionRemotingSourceBridge() final;

  // Forwarded from RedirectionConnector on behalf of the RemotingBridge.
  void Start();
  void StartRemotingDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender);
  void SendMessageToSink(const std::vector<uint8_t>& message);
  void StopRemoting(media::mojom::RemotingStopReason reason);

 private:
  // media::mojom::RemotingSource implementation.
  void OnSinkAvailable(media::mojom::RemotingSinkMetadataPtr metadata) override;
  void OnMessageFromSink(const std::vector<uint8_t>& message) override;
  void OnStopped(media::mojom::RemotingStopReason reason) override;
  void OnSinkGone() override;
  void OnStarted() override;
  void OnStartFailed(media::mojom::RemotingStartFailReason reason) override;

  // Runs |disconnected_callback_|, at most once.
  void OnSessionDisconnected();

  const raw_ptr<RemotingBridge::Client> client_;
  const raw_ptr<RemotingBridge> bridge_;

  bool is_started_ = false;

  base::OnceClosure disconnected_callback_;

  mojo::Remote<media::mojom::Remoter> remoter_;
  mojo::Receiver<media::mojom::RemotingSource> receiver_;

  // Keeps the per-source session alive in the utility process.
  mojo::Remote<redirection::mojom::RedirectionSessionHost> session_host_;
};

#endif  // CHROME_BROWSER_MEDIA_REDIRECTION_REMOTING_SOURCE_BRIDGE_H_
