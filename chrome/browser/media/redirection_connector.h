// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_REDIRECTION_CONNECTOR_H_
#define CHROME_BROWSER_MEDIA_REDIRECTION_CONNECTOR_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/media/remoting_bridge.h"
#include "chrome/services/redirection/public/mojom/redirection_service.mojom.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class RedirectionRemotingSourceBridge;

// Connects media sources across the whole browser to the OS video redirection
// (MMR) sink exposed by the redirection utility process. A redirection session
// is browser-level rather than scoped to one tab, applying to every tab at
// once, so this is a process-wide singleton.
//
// Arbitration with the other remoting path lives in RemotingBridge; this class
// only reports whether redirection is available for a source, and creates or
// tears that source's session down as the bridge activates or deactivates it.
class RedirectionConnector final : public RemotingBridge::Client {
 public:
  // Creates one remoting source's session in the utility process, binding it to
  // the given endpoints.
  using CreateRedirectionSessionCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<redirection::mojom::RedirectionSessionHost>
          session_host,
      mojo::PendingReceiver<media::mojom::Remoter> remoter,
      mojo::PendingRemote<media::mojom::RemotingSource> source)>;

  RedirectionConnector(const RedirectionConnector&) = delete;
  RedirectionConnector& operator=(const RedirectionConnector&) = delete;
  ~RedirectionConnector() final;

  static RedirectionConnector* Get();

  // Called when the redirection utility-process session becomes
  // available / goes away. |create_session_callback| is used to create a new
  // session in the redirection utility-process.
  void StartingRedirection(
      CreateRedirectionSessionCallback create_session_callback);
  void StoppingRedirection();

 private:
  friend class base::NoDestructor<RedirectionConnector>;
  friend class RedirectionConnectorTest;

  RedirectionConnector();

  // RemotingBridge::Client implementation.
  void RegisterBridge(RemotingBridge* bridge) final;
  void DeregisterBridge(RemotingBridge* bridge,
                        media::mojom::RemotingStopReason reason) final;
  void StartRemoting(RemotingBridge* bridge) final;
  void StartWithPermissionAlreadyGranted(RemotingBridge* bridge) final;
  void StartRemotingDataStreams(
      RemotingBridge* bridge,
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender) final;
  void StopRemoting(RemotingBridge* bridge,
                    media::mojom::RemotingStopReason reason,
                    bool is_initiated_by_source) final;
  void SendMessageToSink(RemotingBridge* bridge,
                         const std::vector<uint8_t>& message) final;
  void EstimateTransmissionCapacity(
      media::mojom::Remoter::EstimateTransmissionCapacityCallback callback)
      final;
  void OnClientActivated(RemotingBridge* bridge) final;
  void OnClientDeactivated(RemotingBridge* bridge) final;

  // Every registered RemotingBridge, mapped to its source bridge. The value is
  // null while no per-source redirection session exists. The keys are always
  // valid, since RemotingBridge deregisters in its destructor.
  std::map<RemotingBridge*, std::unique_ptr<RedirectionRemotingSourceBridge>>
      bridges_;

  // Callback used to create a new redirection session in the utility process.
  CreateRedirectionSessionCallback create_session_callback_;
};

#endif  // CHROME_BROWSER_MEDIA_REDIRECTION_CONNECTOR_H_
