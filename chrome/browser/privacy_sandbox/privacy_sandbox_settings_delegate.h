// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

class Profile;

#if BUILDFLAG(IS_ANDROID)
class WebappRegistry;
#endif

class PrivacySandboxSettingsDelegate
    : public privacy_sandbox::PrivacySandboxSettings::Delegate {
 public:
  PrivacySandboxSettingsDelegate(
      Profile* profile,
      PrivacySandboxCountries* privacy_sandbox_countries);
  ~PrivacySandboxSettingsDelegate() override;

  // PrivacySandboxSettings::Delegate:
  bool IsRestrictedNoticeEnabled() const override;
  bool IsPrivacySandboxRestricted() const override;
  bool IsPrivacySandboxCurrentlyUnrestricted() const override;
  bool IsIncognitoProfile() const override;
  bool HasAppropriateTopicsConsent() const override;
  bool IsSubjectToM1NoticeRestricted() const override;

#if BUILDFLAG(IS_ANDROID)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

 private:
  bool PrivacySandboxRestrictedNoticeRequired() const;
  bool IsSubjectToEnterpriseFeatures() const;
  raw_ptr<Profile> profile_;

  raw_ptr<PrivacySandboxCountries> privacy_sandbox_countries_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_
