// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"

#include "base/android/jni_string.h"
#include "base/check.h"
#include "build/build_config.h"
#include "chrome/android/chrome_jni_headers/SmsFetcherMessageHandler_jni.h"
#include "chrome/browser/sharing/proto/sms_fetch_message_test_proto3_optional.pb.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/gurl.h"
#include "url/origin.h"

SmsFetchRequestHandler::SmsFetchRequestHandler(content::SmsFetcher* fetcher)
    : fetcher_(fetcher) {}

SmsFetchRequestHandler::~SmsFetchRequestHandler() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SmsFetcherMessageHandler_reset(env);
}

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

void SmsFetchRequestHandler::AskUserPermission(
    const content::OriginList& origin_list,
    const std::string& one_time_code) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
  const std::u16string origin = url_formatter::FormatOriginForSecurityDisplay(
      origin_list[0], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  // If there is a notification from a previous request on screen this will
  // overwrite that one with the new origin. In most cases where there's only
  // one pending origin, the request will be removed when |SmsRetrieverClient|
  // times out which would triggers |Request::OnFailure|.
  // TODO(crbug.com/1138454): We should improve the infrastructure to be able to
  // handle failures when there are multiple pending origins simultaneously.
  Java_SmsFetcherMessageHandler_showNotification(
      env, base::android::ConvertUTF8ToJavaString(env, one_time_code),
      base::android::ConvertUTF16ToJavaString(env, origin),
      reinterpret_cast<intptr_t>(this));
}

void SmsFetchRequestHandler::OnConfirm(JNIEnv* env, jstring j_origin) {
  // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
  std::u16string origin =
      base::android::ConvertJavaStringToUTF16(env, j_origin);
  auto* request = GetRequest(origin);
  DCHECK(request);
  request->SendSuccessMessage();
}

void SmsFetchRequestHandler::OnDismiss(JNIEnv* env, jstring j_origin) {
  // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
  std::u16string origin =
      base::android::ConvertJavaStringToUTF16(env, j_origin);
  auto* request = GetRequest(origin);
  DCHECK(request);
  // TODO(crbug.com/1015645): We should have a separate catergory for this type
  // of failure.
  request->SendFailureMessage(FailureType::kPromptCancelled);
}

SmsFetchRequestHandler::Request* SmsFetchRequestHandler::GetRequest(
    const std::u16string& origin) {
  for (auto& request : requests_) {
    const auto& origin_list = request->origin_list();
    // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
    if (origin == url_formatter::FormatOriginForSecurityDisplay(
                      origin_list[0],
                      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)) {
      return request.get();
    }
  }
  return nullptr;
}

SmsFetchRequestHandler::Request::Request(
    SmsFetchRequestHandler* handler,
    content::SmsFetcher* fetcher,
    const url::Origin& origin,
    SharingMessageHandler::DoneCallback respond_callback)
    : handler_(handler),
      fetcher_(fetcher),
      origin_list_(content::OriginList{origin}),
      respond_callback_(std::move(respond_callback)) {
  // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
  fetcher_->Subscribe(origin_list_, this);
}

SmsFetchRequestHandler::Request::~Request() {
  // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
  fetcher_->Unsubscribe(origin_list_, this);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SmsFetcherMessageHandler_dismissNotification(env);
}

void SmsFetchRequestHandler::Request::OnReceive(
    const content::OriginList& origin_list,
    const std::string& one_time_code,
    content::SmsFetcher::UserConsent consent_requirement) {
  // TODO(crbug.com/1015645): Support iframe in cross-device WebOTP.
  DCHECK_EQ(origin_list[0], origin_list_[0]);
  one_time_code_ = one_time_code;
  handler_->AskUserPermission(origin_list, one_time_code);
}

void SmsFetchRequestHandler::Request::SendSuccessMessage() {
  auto response = std::make_unique<chrome_browser_sharing::ResponseMessage>();
  for (const auto& origin : origin_list_)
    response->mutable_sms_fetch_response()->add_origin(origin.Serialize());
  response->mutable_sms_fetch_response()->set_one_time_code(one_time_code_);

  std::move(respond_callback_).Run(std::move(response));
  handler_->RemoveRequest(this);
}

void SmsFetchRequestHandler::Request::SendFailureMessage(
    FailureType failure_type) {
  auto response = std::make_unique<chrome_browser_sharing::ResponseMessage>();
  response->mutable_sms_fetch_response()->set_failure_type(
      static_cast<chrome_browser_sharing::SmsFetchResponse::FailureType>(
          failure_type));

  std::move(respond_callback_).Run(std::move(response));
  handler_->RemoveRequest(this);
}

void SmsFetchRequestHandler::Request::OnFailure(FailureType failure_type) {
  SendFailureMessage(failure_type);
}
