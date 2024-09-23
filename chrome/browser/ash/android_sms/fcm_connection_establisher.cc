// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/fcm_connection_establisher.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace ash {
namespace android_sms {

const int FcmConnectionEstablisher::kMaxRetryCount = 7;

constexpr base::TimeDelta FcmConnectionEstablisher::kRetryDelay =
    base::Seconds(5);

// Start message is sent when establishing a connection for the first time on
// log-in. This allows the service worker to freshly subscribe to push
// notifications.
const char FcmConnectionEstablisher::kStartFcmMessage[] =
    "start_fcm_connection";

// Resume message is sent to notify the service worker to resume handling
// background notifications after all "Messages for Web" web pages have
// been closed.
const char FcmConnectionEstablisher::kResumeFcmMessage[] =
    "resume_fcm_connection";

// Stop message is sent to notify the service worker to unsubscribe from
// push messages when the messages feature is disabled.
const char FcmConnectionEstablisher::kStopFcmMessage[] = "stop_fcm_connection";

FcmConnectionEstablisher::PendingServiceWorkerMessage::
    PendingServiceWorkerMessage(
        GURL service_worker_scope,
        MessageType message_type,
        content::ServiceWorkerContext* service_worker_context)
    : service_worker_scope(service_worker_scope),
      message_type(message_type),
      service_worker_context(service_worker_context) {}

FcmConnectionEstablisher::InFlightMessage::InFlightMessage(
    PendingServiceWorkerMessage message)
    : message(message) {}

FcmConnectionEstablisher::FcmConnectionEstablisher(
    std::unique_ptr<base::OneShotTimer> retry_timer)
    : retry_timer_(std::move(retry_timer)) {}
FcmConnectionEstablisher::~FcmConnectionEstablisher() = default;

void FcmConnectionEstablisher::EstablishConnection(
    const GURL& url,
    ConnectionMode connection_mode,
    content::ServiceWorkerContext* service_worker_context) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FcmConnectionEstablisher::SendMessageToServiceWorkerWithRetries,
          weak_ptr_factory_.GetWeakPtr(), url,
          GetMessageTypeForConnectionMode(connection_mode),
          service_worker_context));
}

void FcmConnectionEstablisher::TearDownConnection(
    const GURL& url,
    content::ServiceWorkerContext* service_worker_context) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FcmConnectionEstablisher::SendMessageToServiceWorkerWithRetries,
          weak_ptr_factory_.GetWeakPtr(), url, MessageType::kStop,
          service_worker_context));
}

// static
FcmConnectionEstablisher::MessageType
FcmConnectionEstablisher::GetMessageTypeForConnectionMode(
    ConnectionMode connection_mode) {
  switch (connection_mode) {
    case ConnectionMode::kStartConnection:
      return MessageType::kStart;
    case ConnectionMode::kResumeExistingConnection:
      return MessageType::kResume;
  }
  NOTREACHED_IN_MIGRATION();
}

// static
std::string FcmConnectionEstablisher::GetMessageStringForType(
    MessageType message_type) {
  switch (message_type) {
    case MessageType::kStart:
      return kStartFcmMessage;
    case MessageType::kResume:
      return kResumeFcmMessage;
    case MessageType::kStop:
      return kStopFcmMessage;
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

void FcmConnectionEstablisher::SendMessageToServiceWorkerWithRetries(
    const GURL& url,
    MessageType message_type,
    content::ServiceWorkerContext* service_worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  message_queue_.emplace(url, message_type, service_worker_context);
  ProcessMessageQueue();
}

void FcmConnectionEstablisher::ProcessMessageQueue() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (in_flight_message_)
    return;

  if (message_queue_.empty())
    return;

  in_flight_message_.emplace(message_queue_.front());
  message_queue_.pop();
  SendInFlightMessage();
}

void FcmConnectionEstablisher::SendInFlightMessage() {
  const PendingServiceWorkerMessage& message = in_flight_message_->message;
  blink::TransferableMessage msg = blink::EncodeWebMessagePayload(
      base::UTF8ToUTF16(GetMessageStringForType(message.message_type)));

  PA_LOG(VERBOSE) << "Dispatching message " << message.message_type;
  message.service_worker_context->StartServiceWorkerAndDispatchMessage(
      message.service_worker_scope,
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(message.service_worker_scope)),
      std::move(msg),
      base::BindOnce(&FcmConnectionEstablisher::OnMessageDispatchResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FcmConnectionEstablisher::OnMessageDispatchResult(bool status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(in_flight_message_);
  PA_LOG(VERBOSE) << "Service worker message returned status " << status
                  << " for message "
                  << in_flight_message_->message.message_type;

  if (!status && in_flight_message_->retry_count < kMaxRetryCount) {
    base::TimeDelta retry_delay =
        kRetryDelay * (1 << in_flight_message_->retry_count);
    in_flight_message_->retry_count++;
    UMA_HISTOGRAM_ENUMERATION("AndroidSms.FcmMessageDispatchRetry",
                              in_flight_message_->message.message_type);
    PA_LOG(VERBOSE) << "Scheduling retry with delay " << retry_delay;
    retry_timer_->Start(
        FROM_HERE, retry_delay,
        base::BindOnce(&FcmConnectionEstablisher::SendInFlightMessage,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (status) {
    UMA_HISTOGRAM_ENUMERATION("AndroidSms.FcmMessageDispatchSuccess",
                              in_flight_message_->message.message_type);
  } else {
    UMA_HISTOGRAM_ENUMERATION("AndroidSms.FcmMessageDispatchFailure",
                              in_flight_message_->message.message_type);
    PA_LOG(WARNING) << "Max retries attempted when dispatching message "
                    << in_flight_message_->message.message_type;
  }

  in_flight_message_.reset();
  ProcessMessageQueue();
}

std::ostream& operator<<(
    std::ostream& stream,
    const FcmConnectionEstablisher::MessageType& message_type) {
  switch (message_type) {
    case FcmConnectionEstablisher::MessageType::kStart:
      stream << "MessageType::kStart";
      break;
    case FcmConnectionEstablisher::MessageType::kResume:
      stream << "MessageType::kResume";
      break;
    case FcmConnectionEstablisher::MessageType::kStop:
      stream << "MessageType::kStop";
      break;
  }
  return stream;
}

}  // namespace android_sms
}  // namespace ash
