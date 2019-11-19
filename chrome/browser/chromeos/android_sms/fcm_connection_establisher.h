// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FCM_CONNECTION_ESTABLISHER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FCM_CONNECTION_ESTABLISHER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/android_sms/connection_establisher.h"

namespace chromeos {

namespace android_sms {

// ConnectionEstablisher implementation that initiates an FCM web push
// suscription frin the Android Messages for Web service worker and the
// Tachyon server by dispatching a known message string to it.
class FcmConnectionEstablisher : public ConnectionEstablisher {
 public:
  explicit FcmConnectionEstablisher(
      std::unique_ptr<base::OneShotTimer> retry_timer);
  ~FcmConnectionEstablisher() override;

  // ConnectionEstablisher:
  void EstablishConnection(
      const GURL& url,
      ConnectionMode connection_mode,
      content::ServiceWorkerContext* service_worker_context) override;

  void TearDownConnection(
      const GURL& url,
      content::ServiceWorkerContext* service_worker_context) override;

 private:
  // This enum is used for logging metrics. It should be kept in sync with
  // AndroidSmsFcmMessageType in enums.xml
  enum class MessageType {
    kStart = 0,
    kResume = 1,
    kStop = 2,
    kMaxValue = kStop,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const MessageType& message_type);

  struct PendingServiceWorkerMessage {
    PendingServiceWorkerMessage(
        GURL service_worker_scope,
        MessageType message_type,
        content::ServiceWorkerContext* service_worker_context);
    GURL service_worker_scope;
    MessageType message_type;
    content::ServiceWorkerContext* service_worker_context;
  };

  struct InFlightMessage {
    explicit InFlightMessage(PendingServiceWorkerMessage message);
    PendingServiceWorkerMessage message;
    size_t retry_count = 0u;
  };

  FRIEND_TEST_ALL_PREFIXES(FcmConnectionEstablisherTest,
                           TestEstablishConnection);
  FRIEND_TEST_ALL_PREFIXES(FcmConnectionEstablisherTest,
                           TestTearDownConnection);

  static MessageType GetMessageTypeForConnectionMode(
      ConnectionMode connection_mode);

  static std::string GetMessageStringForType(MessageType message_type);

  void SendMessageToServiceWorkerWithRetries(
      const GURL& url,
      MessageType message_type,
      content::ServiceWorkerContext* service_worker_context);

  void ProcessMessageQueue();

  void SendInFlightMessage();

  void OnMessageDispatchResult(bool status);

  std::unique_ptr<base::OneShotTimer> retry_timer_;
  base::Optional<InFlightMessage> in_flight_message_;

  // A queue of messages to be dispatched. Messages are dispatched and retried
  // one at a time from this queue.
  base::queue<PendingServiceWorkerMessage> message_queue_;

  static const char kStartFcmMessage[];
  static const char kResumeFcmMessage[];
  static const char kStopFcmMessage[];
  static const int kMaxRetryCount;
  static const base::TimeDelta kRetryDelay;

  base::WeakPtrFactory<FcmConnectionEstablisher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FcmConnectionEstablisher);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FCM_CONNECTION_ESTABLISHER_H_
