// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sharing_message/proto/sms_fetch_message_test_proto3_optional.pb.h"
#include "components/sharing_message/sharing_device_source.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/sms_fetcher.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SmsFetcherMessageHandler_jni.h"

namespace {
// To mitigate the overlapping of the notification for SMS and the one for
// user permission, we postpone showing the latter to make sure it's always
// visible to users.
static constexpr base::TimeDelta kNotificationDelay = base::Seconds(1);

bool DoesMatchOriginList(const std::vector<std::u16string>& origins,
                         const content::OriginList& origin_list) {
  if (origins.size() != origin_list.size())
    return false;

  for (size_t i = 0; i < origins.size(); ++i) {
    if (origins[i] != url_formatter::FormatOriginForSecurityDisplay(
                          origin_list[i],
                          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS)) {
      return false;
    }
  }
  return true;
}

}  // namespace

SmsFetchRequestHandler::SmsFetchRequestHandler(
    SharingDeviceSource* device_source,
    content::SmsFetcher* fetcher)
    : device_source_(device_source), fetcher_(fetcher) {}

SmsFetchRequestHandler::~SmsFetchRequestHandler() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SmsFetcherMessageHandler_reset(env);
}

void SmsFetchRequestHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_sms_fetch_request());

  std::optional<SharingTargetDeviceInfo> device =
      device_source_->GetDeviceByGuid(message.sender_guid());
  const std::string& client_name =
      device ? device->client_name() : message.sender_device_name();

  // Empty client_name means that the message is from an unsupported version of
  // Chrome. This is rare in practice.
  if (client_name.empty())
    return;

  const google::protobuf::RepeatedPtrField<std::string>& origin_strings =
      message.sms_fetch_request().origins();
  if (origin_strings.empty())
    return;

  std::vector<url::Origin> origin_list;
  for (const std::string& origin_string : origin_strings)
    origin_list.push_back(url::Origin::Create(GURL(origin_string)));

  auto request = std::make_unique<Request>(
      this, fetcher_, origin_list, client_name, std::move(done_callback));
  requests_.insert(std::move(request));
}

void SmsFetchRequestHandler::RemoveRequest(Request* request) {
  requests_.erase(request);
}

void SmsFetchRequestHandler::AskUserPermission(
    const content::OriginList& origin_list,
    const std::string& one_time_code,
    const std::string& client_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(origin_list.size() == 1 || origin_list.size() == 2);

  base::android::ScopedJavaLocalRef<jstring> embedded_origin;
  base::android::ScopedJavaLocalRef<jstring> top_origin;
  if (origin_list.size() == 2) {
    embedded_origin = base::android::ConvertUTF16ToJavaString(
        env,
        url_formatter::FormatOriginForSecurityDisplay(
            origin_list[0], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
    top_origin = base::android::ConvertUTF16ToJavaString(
        env,
        url_formatter::FormatOriginForSecurityDisplay(
            origin_list[1], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  } else {
    top_origin = base::android::ConvertUTF16ToJavaString(
        env,
        url_formatter::FormatOriginForSecurityDisplay(
            origin_list[0], url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  // If there is a notification from a previous request on screen this will
  // overwrite that one with the new origin. In most cases where there's only
  // one pending origin, the request will be removed when |SmsRetrieverClient|
  // times out which would triggers |Request::OnFailure|.
  // TODO(crbug.com/40153007): We should improve the infrastructure to be able
  // to handle failures when there are multiple pending origins simultaneously.
  Java_SmsFetcherMessageHandler_showNotification(
      env, one_time_code, top_origin, embedded_origin, client_name,
      reinterpret_cast<intptr_t>(this));
}

void SmsFetchRequestHandler::OnConfirm(JNIEnv* env,
                                       jstring j_top_origin,
                                       jstring j_embedded_origin) {
  std::vector<std::u16string> origins;
  if (j_embedded_origin) {
    std::u16string embedded_origin =
        base::android::ConvertJavaStringToUTF16(env, j_embedded_origin);
    origins.push_back(embedded_origin);
  }
  std::u16string top_origin =
      base::android::ConvertJavaStringToUTF16(env, j_top_origin);
  origins.push_back(top_origin);
  auto* request = GetRequest(origins);
  DCHECK(request);
  request->SendSuccessMessage();
}

void SmsFetchRequestHandler::OnDismiss(JNIEnv* env,
                                       jstring j_top_origin,
                                       jstring j_embedded_origin) {
  std::vector<std::u16string> origins;
  if (j_embedded_origin) {
    std::u16string embedded_origin =
        base::android::ConvertJavaStringToUTF16(env, j_embedded_origin);
    origins.push_back(embedded_origin);
  }
  std::u16string top_origin =
      base::android::ConvertJavaStringToUTF16(env, j_top_origin);
  origins.push_back(top_origin);
  auto* request = GetRequest(origins);
  DCHECK(request);
  // TODO(crbug.com/40103792): We should have a separate catergory for this type
  // of failure.
  request->SendFailureMessage(FailureType::kPromptCancelled);
}

SmsFetchRequestHandler::Request* SmsFetchRequestHandler::GetRequest(
    const std::vector<std::u16string>& origins) {
  // If the request is made from a cross-origin iframe, the origin_list consists
  // of the embedded frame origin and then the top frame origin.
  for (auto& request : requests_) {
    if (DoesMatchOriginList(origins, request->origin_list()))
      return request.get();
  }
  return nullptr;
}

base::WeakPtr<SmsFetchRequestHandler> SmsFetchRequestHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SmsFetchRequestHandler::Request::Request(
    SmsFetchRequestHandler* handler,
    content::SmsFetcher* fetcher,
    const std::vector<url::Origin>& origin_list,
    const std::string& client_name,
    SharingMessageHandler::DoneCallback respond_callback)
    : handler_(handler),
      fetcher_(fetcher),
      origin_list_(origin_list),
      client_name_(client_name),
      respond_callback_(std::move(respond_callback)) {
  fetcher_->Subscribe(origin_list_, *this);
}

SmsFetchRequestHandler::Request::~Request() {
  fetcher_->Unsubscribe(origin_list_, this);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SmsFetcherMessageHandler_dismissNotification(env);
}

void SmsFetchRequestHandler::Request::OnReceive(
    const content::OriginList& origin_list,
    const std::string& one_time_code,
    content::SmsFetcher::UserConsent consent_requirement) {
  DCHECK(origin_list_ == origin_list);
  one_time_code_ = one_time_code;

  // Postpones asking for user permission to make sure that the notification is
  // not covered by the SMS notification.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SmsFetchRequestHandler::AskUserPermission,
                     handler_->GetWeakPtr(), origin_list, one_time_code,
                     client_name_),
      kNotificationDelay);
}

void SmsFetchRequestHandler::Request::SendSuccessMessage() {
  auto response =
      std::make_unique<components_sharing_message::ResponseMessage>();
  for (const auto& origin : origin_list_)
    response->mutable_sms_fetch_response()->add_origins(origin.Serialize());
  response->mutable_sms_fetch_response()->set_one_time_code(one_time_code_);

  std::move(respond_callback_).Run(std::move(response));
  handler_->RemoveRequest(this);
}

void SmsFetchRequestHandler::Request::SendFailureMessage(
    FailureType failure_type) {
  auto response =
      std::make_unique<components_sharing_message::ResponseMessage>();
  response->mutable_sms_fetch_response()->set_failure_type(
      static_cast<components_sharing_message::SmsFetchResponse::FailureType>(
          failure_type));

  std::move(respond_callback_).Run(std::move(response));
  handler_->RemoveRequest(this);
}

void SmsFetchRequestHandler::Request::OnFailure(FailureType failure_type) {
  SendFailureMessage(failure_type);
}
