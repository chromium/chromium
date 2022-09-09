// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/display_agent.h"

#include "base/notreached.h"

namespace notifications {

// Default implementation of DisplayAgent.
class DisplayAgentDefault : public DisplayAgent {
 public:
  DisplayAgentDefault() = default;
  DisplayAgentDefault(const DisplayAgentDefault&) = delete;
  DisplayAgentDefault& operator=(const DisplayAgentDefault&) = delete;
  ~DisplayAgentDefault() override = default;

 private:
  void ShowNotification(std::unique_ptr<NotificationData> notification_data,
                        std::unique_ptr<SystemData> system_data) override {
    NOTIMPLEMENTED();
  }
};

// static
std::unique_ptr<DisplayAgent> DisplayAgent::Create() {
  return std::make_unique<DisplayAgentDefault>();
}

}  // namespace notifications
