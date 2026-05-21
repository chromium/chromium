// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_REMOTING_BRIDGE_H_
#define CHROME_BROWSER_MEDIA_REMOTING_BRIDGE_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// Implementation of the media::mojom::Remoter service for a single media source
// in a render frame. This is a "lightweight bridge" that delegates calls
// back-and-forth between a Client and a media::mojom::RemotingSource. An
// instance of this class is owned by its mojo message pipe.
class RemotingBridge final : public media::mojom::Remoter {
 public:
  // Interface for objects that manage remoting sessions.
  class Client {
   public:
    virtual ~Client() = default;

    virtual void RegisterBridge(RemotingBridge* bridge) = 0;
    virtual void DeregisterBridge(RemotingBridge* bridge,
                                  media::mojom::RemotingStopReason reason) = 0;
    virtual void StartRemoting(RemotingBridge* bridge) = 0;
    virtual void StartWithPermissionAlreadyGranted(RemotingBridge* bridge) = 0;
    virtual void StartRemotingDataStreams(
        RemotingBridge* bridge,
        mojo::ScopedDataPipeConsumerHandle audio_pipe,
        mojo::ScopedDataPipeConsumerHandle video_pipe,
        mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
            audio_sender,
        mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
            video_sender) = 0;
    virtual void StopRemoting(RemotingBridge* bridge,
                              media::mojom::RemotingStopReason reason,
                              bool is_initiated_by_source) = 0;
    virtual void SendMessageToSink(RemotingBridge* bridge,
                                   const std::vector<uint8_t>& message) = 0;
    virtual void EstimateTransmissionCapacity(
        media::mojom::Remoter::EstimateTransmissionCapacityCallback
            callback) = 0;
  };

  // Constructs a "bridge" to delegate calls between the given |source| and
  // |client|. |client| must be valid at the time of construction, but is
  // otherwise a weak pointer that can become invalid during the lifetime of a
  // RemotingBridge.
  RemotingBridge(mojo::PendingRemote<media::mojom::RemotingSource> source,
                 Client* client);

  RemotingBridge(const RemotingBridge&) = delete;
  RemotingBridge& operator=(const RemotingBridge&) = delete;

  ~RemotingBridge() final;

  // Used by ChromeContentBrowserClient to create a Remoter for each new
  // source in a render frame. The caller is responsible for providing the
  // appropriate Client for the source.
  static void CreateMediaRemoter(
      Client* client,
      mojo::PendingRemote<media::mojom::RemotingSource> source,
      mojo::PendingReceiver<media::mojom::Remoter> receiver);

  // The Client calls these to forward notifications to the RemotingSource.
  void OnSinkAvailable(const media::mojom::RemotingSinkMetadata& metadata);
  void OnSinkGone();
  void OnStarted();
  void OnStartFailed(media::mojom::RemotingStartFailReason reason);
  void OnMessageFromSink(const std::vector<uint8_t>& message);
  void OnStopped(media::mojom::RemotingStopReason reason);

  // Called by the Client when it is being destroyed.
  void OnClientDestroyed();

 private:
  // media::mojom::Remoter implementation. The source calls these to start/stop
  // media remoting and send messages to the sink. These simply delegate to the
  // Client, which is responsible for establishing and managing remoting
  // connections. The client will respond to this request by calling one of:
  // OnStarted() or OnStartFailed().
  void Start() final;
  void StartWithPermissionAlreadyGranted() final;
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender) final;
  void Stop(media::mojom::RemotingStopReason reason) final;
  void SendMessageToSink(const std::vector<uint8_t>& message) final;
  void EstimateTransmissionCapacity(
      media::mojom::Remoter::EstimateTransmissionCapacityCallback callback)
      final;

  mojo::Remote<media::mojom::RemotingSource> source_;

  // Weak pointer. Will be set to nullptr if the Client is destroyed before
  // this RemotingBridge.
  raw_ptr<Client> client_;

  // Ensure RemotingBridge is used on a single thread.
  THREAD_CHECKER(thread_checker_);
};

#endif  // CHROME_BROWSER_MEDIA_REMOTING_BRIDGE_H_
