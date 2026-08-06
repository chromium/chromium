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
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

class Profile;

#if BUILDFLAG(IS_ANDROID)
class WebappRegistry;
#endif

class PrivacySandboxSettingsDelegate
    : public privacy_sandbox::PrivacySandboxSettings::Delegate {
 public:
  explicit PrivacySandboxSettingsDelegate(Profile* profile);
  ~PrivacySandboxSettingsDelegate() override;

  // PrivacySandboxSettings::Delegate:
  bool IsPrivacySandboxRestricted() const override;
  bool IsPrivacySandboxCurrentlyUnrestricted() const override;
  bool IsIncognitoProfile() const override;

#if BUILDFLAG(IS_ANDROID)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

 private:
  bool IsSubjectToEnterpriseFeatures() const;
  raw_ptr<Profile> profile_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_
