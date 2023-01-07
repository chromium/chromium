// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_SEND_MESSAGE_EXPRESS_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_SEND_MESSAGE_EXPRESS_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"

namespace chrome_browser_nearby_sharing_instantmessaging {
class SendMessageExpressRequest;
}  // namespace chrome_browser_nearby_sharing_instantmessaging

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

// Sends messages using the Instant Messaging API over HTTP. This object is
// intended to service exactly one request to SendMessage per object. The
// WebRtcSignalingMessenger that creates this object may choose to clean it up
// after the SuccessCallback is invoked so no interaction with the |this|
// pointer should happen after that point.
class SendMessageExpress {
 public:
  using SuccessCallback = base::OnceCallback<void(bool success)>;

  SendMessageExpress(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~SendMessageExpress();

  void SendMessage(const chrome_browser_nearby_sharing_instantmessaging::
                       SendMessageExpressRequest& request,
                   SuccessCallback callback);

 private:
  void DoSendMessage(const chrome_browser_nearby_sharing_instantmessaging::
                         SendMessageExpressRequest& request,
                     SuccessCallback callback,
                     const std::string& oauth_token);
  void OnSendMessageResponse(
      const std::string& message_id,
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      SuccessCallback callback,
      std::unique_ptr<std::string> response_body);

  TokenFetcher token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<SendMessageExpress> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_SEND_MESSAGE_EXPRESS_H_
