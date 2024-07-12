// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_delegate.h"

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

namespace privacy_sandbox {
TrackingProtectionOnboardingDelegate::TrackingProtectionOnboardingDelegate(
    Profile* profile)
    : profile_(profile) {}

TrackingProtectionOnboardingDelegate::~TrackingProtectionOnboardingDelegate() =
    default;

bool TrackingProtectionOnboardingDelegate::IsEnterpriseManaged() const {
  return chrome::enterprise_util::IsBrowserManaged(profile_);
}
}  // namespace privacy_sandbox
