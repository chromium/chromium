// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_delegate.h"

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
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

bool TrackingProtectionOnboardingDelegate::IsNewProfile() const {
  return profile_->IsNewProfile();
}

bool TrackingProtectionOnboardingDelegate::AreThirdPartyCookiesBlocked() const {
  return profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode) ==
         static_cast<int>(
             content_settings::CookieControlsMode::kBlockThirdParty);
}

}  // namespace privacy_sandbox
