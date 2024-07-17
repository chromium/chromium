// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_DELEGATE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

class Profile;

namespace privacy_sandbox {
class TrackingProtectionOnboardingDelegate
    : public TrackingProtectionOnboarding::Delegate {
 public:
  explicit TrackingProtectionOnboardingDelegate(Profile* profile);
  ~TrackingProtectionOnboardingDelegate() override;

  bool IsEnterpriseManaged() const override;
  bool IsNewProfile() const override;
  bool AreThirdPartyCookiesBlocked() const override;

 private:
  raw_ptr<Profile> profile_;
};
}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_DELEGATE_H_
