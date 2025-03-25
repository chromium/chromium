// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_QUEUE_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_QUEUE_MANAGER_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_queue_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace privacy_sandbox {

class MockPrivacySandboxQueueManager
    : public privacy_sandbox::PrivacySandboxQueueManager {
 public:
  MockPrivacySandboxQueueManager();  // Default constructor
  ~MockPrivacySandboxQueueManager() override;

  // Only mocking queue and unqueue methods. Can add more.
  // If you would like to use true queueing functionality, create a queue
  // manager and create an ON_CALL in your test.
  MOCK_METHOD(void, MaybeUnqueueNotice, (), (override));
  MOCK_METHOD(void, MaybeQueueNotice, (), (override));
};
}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_MOCK_QUEUE_MANAGER_H_
