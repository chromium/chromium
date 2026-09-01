// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_REMOTING_BRIDGE_H_
#define CHROME_BROWSER_MEDIA_REMOTING_BRIDGE_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
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
//
// A source may be served by any of several Clients, each offering a different
// remoting path. Only one can be active at a time, since each takes over the
// same element's media pipeline and only one can supply the remote renderer.
// Each Client reports whether it is currently able to serve the source, and
// this bridge activates the first available one in the order the Clients were
// given.
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

    // Called when this Client starts or stops serving |bridge|.
    virtual void OnClientActivated(RemotingBridge* bridge) {}
    virtual void OnClientDeactivated(RemotingBridge* bridge) {}
  };

  // Constructs a "bridge" to delegate calls between the given |source| and the
  // Clients. |clients| must be non-empty and free of nulls, and is in order of
  // preference, so that the first available one serves the source. A Client may
  // still be destroyed during the lifetime of a RemotingBridge, which it
  // signals with OnClientDestroyed().
  RemotingBridge(base::span<Client* const> clients,
                 mojo::PendingRemote<media::mojom::RemotingSource> source);

  RemotingBridge(const RemotingBridge&) = delete;
  RemotingBridge& operator=(const RemotingBridge&) = delete;

  ~RemotingBridge() final;

  // Used by ChromeContentBrowserClient to create a Remoter for each new source
  // in a render frame. The caller is responsible for providing the appropriate
  // Clients for the source, in order of preference.
  static void CreateMediaRemoter(
      base::span<Client* const> clients,
      mojo::PendingRemote<media::mojom::RemotingSource> source,
      mojo::PendingReceiver<media::mojom::Remoter> receiver);

  // The Client calls these to forward notifications to the RemotingSource.
  // Notifications from a Client that is not the active one are dropped.
  void OnSinkAvailable(Client* client,
                       const media::mojom::RemotingSinkMetadata& metadata);
  void OnSinkGone(Client* client);
  void OnStarted(Client* client);
  void OnStartFailed(Client* client,
                     media::mojom::RemotingStartFailReason reason);
  void OnMessageFromSink(Client* client, const std::vector<uint8_t>& message);
  void OnStopped(Client* client, media::mojom::RemotingStopReason reason);

  // Called by the Client when it is being destroyed.
  void OnClientDestroyed(Client* client);

  // Called by a Client when its ability to serve this source changes.
  void SetClientAvailable(Client* client, bool available);

 private:
  // A Client and its current ability to serve this source. |client| is a weak
  // pointer, reset to null once the Client reports its own destruction.
  struct ClientEntry {
    raw_ptr<Client> client = nullptr;
    bool is_available = false;

    bool is_ready() const { return client && is_available; }

    void Reset() {
      client = nullptr;
      is_available = false;
    }
  };

  // media::mojom::Remoter implementation. The source calls these to start/stop
  // media remoting and send messages to the sink. These simply delegate to the
  // active Client, which is responsible for establishing and managing remoting
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

  // Returns the entry for |client|, which must be one of |clients_|.
  ClientEntry& GetEntry(Client* client);

  // Picks the active Client from current availability state.
  void UpdateActiveClient();

  mojo::Remote<media::mojom::RemotingSource> source_;

  // The Clients that can serve this source, in order of preference. Populated
  // at construction and never resized, so that a Client destroyed part way
  // through does not disturb the order of the rest.
  std::vector<ClientEntry> clients_;

  // The Client currently serving |source_|, or null if none is ready to.
  raw_ptr<Client> active_client_ = nullptr;

  // Ensure RemotingBridge is used on a single thread.
  THREAD_CHECKER(thread_checker_);
};

#endif  // CHROME_BROWSER_MEDIA_REMOTING_BRIDGE_H_
