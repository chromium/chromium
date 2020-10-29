// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_RECEIVE_MESSAGES_EXPRESS_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_RECEIVE_MESSAGES_EXPRESS_H_

#include <cstdint>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace chrome_browser_nearby_sharing_instantmessaging {
class ReceiveMessagesExpressRequest;
}  // namespace chrome_browser_nearby_sharing_instantmessaging

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class TokenFetcher;

// Receives streaming messages from Instant Messaging API over HTTP. Responsible
// for parsing incoming bytes into valid ReceivesMessagesExpressResponse
// messages.
class ReceiveMessagesExpress : public network::SimpleURLLoaderStreamConsumer {
 public:
  using SuccessCallback = base::OnceCallback<void(bool success)>;

  ReceiveMessagesExpress(
      TokenFetcher* token_fetcher,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~ReceiveMessagesExpress() override;

  // Registers with InstantMessaging to start receiving messages. Each
  // individual message is passed to |listener|. Subsequent calls to this method
  // replaces the registered listener.
  void StartReceivingMessages(
      const chrome_browser_nearby_sharing_instantmessaging::
          ReceiveMessagesExpressRequest& request,
      base::RepeatingCallback<void(const std::string& message)> listener,
      SuccessCallback callback);

  // Stops the incoming stream of messages from InstantMessaging and unregisters
  // the listener.
  void StopReceivingMessages();

 private:
  void DoStartReceivingMessages(
      const chrome_browser_nearby_sharing_instantmessaging::
          ReceiveMessagesExpressRequest& request,
      const std::string& oauth_token);

  // network::SimpleURLLoaderStreamConsumer:
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  void OnFastPathReady();

  TokenFetcher* token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::unique_ptr<StreamParser> stream_parser_;
  SuccessCallback success_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReceiveMessagesExpress> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_RECEIVE_MESSAGES_EXPRESS_H_
