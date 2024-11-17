// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_FETCH_REQUEST_HANDLER_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_FETCH_REQUEST_HANDLER_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/origin.h"

namespace content {
class SmsFetcher;
}

class SharingDeviceSource;

// Handles incoming messages for the sms fetcher feature.
class SmsFetchRequestHandler : public SharingMessageHandler {
 public:
  using FailureType = content::SmsFetchFailureType;

  SmsFetchRequestHandler(SharingDeviceSource* device_source,
                         content::SmsFetcher* fetcher);

  SmsFetchRequestHandler(const SmsFetchRequestHandler&) = delete;
  SmsFetchRequestHandler& operator=(const SmsFetchRequestHandler&) = delete;

  ~SmsFetchRequestHandler() override;

  // SharingMessageHandler
  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;
  virtual void AskUserPermission(const content::OriginList&,
                                 const std::string& one_time_code,
                                 const std::string& client_name);
  virtual void OnConfirm(JNIEnv*, jstring top_origin, jstring embedded_origin);
  virtual void OnDismiss(JNIEnv*, jstring top_origin, jstring embedded_origin);

 private:
  FRIEND_TEST_ALL_PREFIXES(SmsFetchRequestHandlerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(SmsFetchRequestHandlerTest, OutOfOrder);
  FRIEND_TEST_ALL_PREFIXES(SmsFetchRequestHandlerTest,
                           SendSuccessMessageOnConfirm);
  FRIEND_TEST_ALL_PREFIXES(SmsFetchRequestHandlerTest,
                           SendFailureMessageOnDismiss);
  // Request represents an incoming request from a remote WebOTPService.
  // It manages subscribing and unsubscribing for SMSes in SmsFetcher and
  // responding to the callback.
  // It also lets SmsFetchRequestHandler know when the request is fulfilled
  // to allow its memory to be freed.
  class Request : public content::SmsFetcher::Subscriber {
   public:
    Request(SmsFetchRequestHandler* handler,
            content::SmsFetcher* fetcher,
            const std::vector<url::Origin>& origin_list,
            const std::string& client_name,
            SharingMessageHandler::DoneCallback respond_callback);

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    ~Request() override;

    void OnReceive(const content::OriginList&,
                   const std::string& one_time_code,
                   content::SmsFetcher::UserConsent) override;
    void OnFailure(FailureType failure_type) override;
    const content::OriginList& origin_list() const { return origin_list_; }
    // OnReceive stashes the response and asks users for permission to send it
    // to remote. Based on user's interaction, we send different responses back.
    void SendSuccessMessage();
    void SendFailureMessage(FailureType);

   private:
    raw_ptr<SmsFetchRequestHandler> handler_;
    raw_ptr<content::SmsFetcher> fetcher_;
    const content::OriginList origin_list_;
    std::string one_time_code_;
    std::string client_name_;
    SharingMessageHandler::DoneCallback respond_callback_;
  };

  void RemoveRequest(Request* Request);
  Request* GetRequest(const std::vector<std::u16string>& origins);

  base::WeakPtr<SmsFetchRequestHandler> GetWeakPtr();

  // |device_source_| is owned by |SharingService| which also transitively owns
  // this class via |SharingHandlerRegistry|.
  raw_ptr<SharingDeviceSource> device_source_;
  // |fetcher_| is safe because it is owned by BrowserContext, which also
  // owns (transitively, via SharingService) this class.
  raw_ptr<content::SmsFetcher> fetcher_;
  base::flat_set<std::unique_ptr<Request>, base::UniquePtrComparator> requests_;

  base::WeakPtrFactory<SmsFetchRequestHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_FETCH_REQUEST_HANDLER_H_
