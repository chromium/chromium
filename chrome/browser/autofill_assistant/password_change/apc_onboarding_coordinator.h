// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_H_

#include "base/callback.h"

// Abstract interface for an onboarding coordinator.
class ApcOnboardingCoordinator {
 public:
  // A callback with a success parameter indicating whether consent has been
  // given.
  using Callback = base::OnceCallback<void(bool)>;

  ApcOnboardingCoordinator() = default;
  virtual ~ApcOnboardingCoordinator() = default;

  // Starts the onboarding process. This may include several steps, such as
  // checking preferences whether consent has been given previously, prompting
  // the user to give consent now, etc.
  virtual void PerformOnboarding(Callback callback) = 0;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_H_
