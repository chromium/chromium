// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

class Profile;

class PrivacySandboxSettingsDelegate
    : public privacy_sandbox::PrivacySandboxSettings::Delegate {
 public:
  explicit PrivacySandboxSettingsDelegate(Profile* profile);
  ~PrivacySandboxSettingsDelegate() override;

  // PrivacySandboxSettings::Delegate:
  bool IsPrivacySandboxRestricted() const override;
  bool IsIncognitoProfile() const override;
  bool HasAppropriateTopicsConsent() const override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_
