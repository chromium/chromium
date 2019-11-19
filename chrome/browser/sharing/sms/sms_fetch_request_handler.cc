// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"

#include "base/logging.h"
#include "components/sync/protocol/sharing_sms_fetch_message.pb.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/gurl.h"
#include "url/origin.h"

SmsFetchRequestHandler::SmsFetchRequestHandler(content::SmsFetcher* fetcher)
    : fetcher_(fetcher) {}

SmsFetchRequestHandler::~SmsFetchRequestHandler() = default;

void SmsFetchRequestHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_sms_fetch_request());

  auto origin = url::Origin::Create(GURL(message.sms_fetch_request().origin()));
  auto request = std::make_unique<Request>(this, fetcher_, origin,
                                           std::move(done_callback));
  requests_.insert(std::move(request));
}

void SmsFetchRequestHandler::RemoveRequest(Request* request) {
  requests_.erase(request);
}

SmsFetchRequestHandler::Request::Request(
    SmsFetchRequestHandler* handler,
    content::SmsFetcher* fetcher,
    const url::Origin& origin,
    SharingMessageHandler::DoneCallback respond_callback)
    : handler_(handler),
      fetcher_(fetcher),
      origin_(origin),
      respond_callback_(std::move(respond_callback)) {
  fetcher_->Subscribe(origin_, this);
}

SmsFetchRequestHandler::Request::~Request() {
  fetcher_->Unsubscribe(origin_, this);
}

void SmsFetchRequestHandler::Request::OnReceive(
    const std::string& one_time_code,
    const std::string& sms) {
  auto response = std::make_unique<chrome_browser_sharing::ResponseMessage>();
  response->mutable_sms_fetch_response()->set_sms(sms);
  response->mutable_sms_fetch_response()->set_one_time_code(one_time_code);

  std::move(respond_callback_).Run(std::move(response));

  handler_->RemoveRequest(this);
}
