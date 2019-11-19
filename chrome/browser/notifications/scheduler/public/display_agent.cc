// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/display_agent.h"

#include "base/logging.h"

namespace notifications {

// Default implementation of DisplayAgent.
class DisplayAgentDefault : public DisplayAgent {
 public:
  DisplayAgentDefault() = default;
  ~DisplayAgentDefault() override = default;

 private:
  void ShowNotification(std::unique_ptr<NotificationData> notification_data,
                        std::unique_ptr<SystemData> system_data) override {
    NOTIMPLEMENTED();
  }

  DISALLOW_COPY_AND_ASSIGN(DisplayAgentDefault);
};

// static
std::unique_ptr<DisplayAgent> DisplayAgent::Create() {
  return std::make_unique<DisplayAgentDefault>();
}

}  // namespace notifications
