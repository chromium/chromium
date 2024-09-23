// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_RECEIVE_MESSAGES_EXPRESS_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_RECEIVE_MESSAGES_EXPRESS_H_

#include <cstdint>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace chrome_browser_nearby_sharing_instantmessaging {
class ReceiveMessagesExpressRequest;
}  // namespace chrome_browser_nearby_sharing_instantmessaging

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

// Receives streaming messages from Instant Messaging API over HTTP. Responsible
// for parsing incoming bytes into valid ReceivesMessagesExpressResponse
// messages.
class ReceiveMessagesExpress : public sharing::mojom::ReceiveMessagesSession,
                               public network::SimpleURLLoaderStreamConsumer {
 public:
  using SuccessCallback = base::OnceCallback<void(bool success)>;
  using StartReceivingMessagesCallback =
      sharing::mojom::WebRtcSignalingMessenger::StartReceivingMessagesCallback;

  static void StartReceiveSession(
      const std::string& self_id,
      sharing::mojom::LocationHintPtr location_hint,
      mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
          incoming_messages_listener,
      StartReceivingMessagesCallback callback,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~ReceiveMessagesExpress() override;

  // sharing::mojom::ReceiveMessagesSession:
  void StopReceivingMessages() override;

 private:
  ReceiveMessagesExpress(
      mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
          incoming_messages_listener,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  void StartReceivingMessages(
      const chrome_browser_nearby_sharing_instantmessaging::
          ReceiveMessagesExpressRequest& request,
      StartReceivingMessagesCallback callback,
      mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession>
          pending_remote_for_result);

  void DoStartReceivingMessages(
      const chrome_browser_nearby_sharing_instantmessaging::
          ReceiveMessagesExpressRequest& request,
      const std::string& oauth_token);

  void OnFastPathReadyTimeout();

  // network::SimpleURLLoaderStreamConsumer:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  void DelegateMessage(const chrome_browser_nearby_sharing_instantmessaging::
                           ReceiveMessagesResponse& response);

  // StreamParser callbacks:
  void OnFastPathReady();
  void OnMessageReceived(const std::string& message);

  // This method will cause the object to shut down its mojo pipe
  // and self destruct. After calling, this object may no longer be valid and
  // no further interactions should be done.
  void FailSessionAndDestruct(const std::string reason);

  StartReceivingMessagesCallback start_receiving_messages_callback_;
  mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession>
      self_pending_remote_;
  mojo::Remote<sharing::mojom::IncomingMessagesListener>
      incoming_messages_listener_;
  TokenFetcher token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  StreamParser stream_parser_;
  base::OneShotTimer fast_path_ready_timeout_timer_;
  std::string request_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReceiveMessagesExpress> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_RECEIVE_MESSAGES_EXPRESS_H_
