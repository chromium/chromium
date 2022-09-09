// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_H_

#include <memory>
#include <vector>

#include "base/callback.h"

namespace content {
class WebContents;
}  // namespace content

struct AssistantOnboardingInformation;

// Abstract interface for an onboarding coordinator.
class ApcOnboardingCoordinator {
 public:
  // A callback with a success parameter indicating whether consent has been
  // given.
  using Callback = base::OnceCallback<void(bool)>;

  // Factory function to create an `ApcOnboardingCoordinator` that is defined
  // in `apc_onboarding_controller_impl.cc`.
  static std::unique_ptr<ApcOnboardingCoordinator> Create(
      content::WebContents* web_contents);

  // Returns the information for the consent dialog used in automated
  // password change flows.
  static AssistantOnboardingInformation CreateOnboardingInformation();

  ApcOnboardingCoordinator() = default;
  virtual ~ApcOnboardingCoordinator() = default;

  // Starts the onboarding process. This may include several steps, such as
  // checking preferences whether consent has been given previously, prompting
  // the user to give consent now, etc.
  virtual void PerformOnboarding(Callback callback) = 0;

  // Revokes consent to use Autofill Assistant, where `description_ids` are the
  // resource ids of the text on the description labels.
  virtual void RevokeConsent(const std::vector<int>& description_ids) = 0;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_H_
