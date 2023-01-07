// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_APC_ONBOARDING_COORDINATOR_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_APC_ONBOARDING_COORDINATOR_H_

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"

// Mocked ApcOnboardingController used in unit tests.
class MockApcOnboardingCoordinator : public ApcOnboardingCoordinator {
 public:
  MockApcOnboardingCoordinator();
  ~MockApcOnboardingCoordinator() override;

  MOCK_METHOD(void, PerformOnboarding, (Callback callback), (override));
  MOCK_METHOD(void,
              RevokeConsent,
              (const std::vector<int>& description_ids),
              (override));
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_APC_ONBOARDING_COORDINATOR_H_
