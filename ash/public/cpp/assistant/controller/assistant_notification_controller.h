// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_NOTIFICATION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"

namespace ash {

// Interface to the AssistantNotificationController which is owned by the
// AssistantController. Currently used by the Assistant service to modify
// Assistant notification state in Ash in response to LibAssistant events.
class ASH_PUBLIC_EXPORT AssistantNotificationController {
 public:
  static AssistantNotificationController* Get();

  // Requests that the notification uniquely identified by |id| be removed. If
  // |from_server| is true the request to remove was initiated by the server.
  virtual void RemoveNotificationById(const std::string& id,
                                      bool from_server) = 0;

  // Changes the quiet mode state in the message center.
  virtual void SetQuietMode(bool enabled) = 0;

 protected:
  AssistantNotificationController();
  virtual ~AssistantNotificationController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_NOTIFICATION_CONTROLLER_H_
