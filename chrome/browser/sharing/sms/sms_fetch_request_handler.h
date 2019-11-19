// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_FETCH_REQUEST_HANDLER_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_FETCH_REQUEST_HANDLER_H_

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/origin.h"

namespace content {
class SmsFetcher;
}

// Handles incoming messages for the sms fetcher feature.
class SmsFetchRequestHandler : public SharingMessageHandler {
 public:
  explicit SmsFetchRequestHandler(content::SmsFetcher* fetcher);
  ~SmsFetchRequestHandler() override;

  // SharingMessageHandler
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  // Request represents an incoming request from a remote SmsService.
  // It manages subscribing and unsubscribing for SMSes in SmsFetcher and
  // responding to the callback.
  // It also lets SmsFetchRequestHandler know when the request is fulfilled
  // to allow its memory to be freed.
  class Request : public content::SmsFetcher::Subscriber {
   public:
    Request(SmsFetchRequestHandler* handler,
            content::SmsFetcher* fetcher,
            const url::Origin& origin,
            SharingMessageHandler::DoneCallback respond_callback);
    ~Request() override;

    void OnReceive(const std::string& one_time_code,
                   const std::string& sms) override;

   private:
    SmsFetchRequestHandler* handler_;
    content::SmsFetcher* fetcher_;
    const url::Origin& origin_;
    SharingMessageHandler::DoneCallback respond_callback_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  void RemoveRequest(Request* Request);

  // |fetcher_| is safe because it is owned by BrowserContext, which also
  // owns (transitively, via SharingService) this class.
  content::SmsFetcher* fetcher_;
  base::flat_set<std::unique_ptr<Request>, base::UniquePtrComparator> requests_;
  base::WeakPtrFactory<SmsFetchRequestHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SmsFetchRequestHandler);
};

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_FETCH_REQUEST_HANDLER_H_
