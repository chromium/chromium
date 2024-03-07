// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_SMS_OBSERVER_H_
#define ASH_SYSTEM_NETWORK_SMS_OBSERVER_H_

#include "ash/ash_export.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "chromeos/ash/components/network/text_message_provider.h"

namespace ash {

// SmsObserver is called when a new sms message is received. Then it shows the
// sms message to the user in the notification center.
class ASH_EXPORT SmsObserver : public TextMessageProvider::Observer {
 public:
  // The prefix of all SMS notifications.
  static const char kNotificationPrefix[];

  SmsObserver();

  SmsObserver(const SmsObserver&) = delete;
  SmsObserver& operator=(const SmsObserver&) = delete;

  ~SmsObserver() override;

  // TextMessageProvider::Observer:
  void MessageReceived(const std::string& guid,
                       const TextMessageData& message) override;

 private:
  // Used to create notification identifier.
  uint32_t message_id_ = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_SMS_OBSERVER_H_
