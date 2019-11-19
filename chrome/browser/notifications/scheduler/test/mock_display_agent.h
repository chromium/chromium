// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_DISPLAY_AGENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_DISPLAY_AGENT_H_

#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockDisplayAgent : public DisplayAgent {
 public:
  MockDisplayAgent();
  ~MockDisplayAgent() override;
  MOCK_METHOD2(ShowNotification,
               void(std::unique_ptr<NotificationData>,
                    std::unique_ptr<SystemData>));
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_DISPLAY_AGENT_H_
