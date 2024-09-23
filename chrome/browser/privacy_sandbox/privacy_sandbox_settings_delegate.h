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
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"

class Profile;

#if BUILDFLAG(IS_ANDROID)
class WebappRegistry;
#endif

namespace tpcd::experiment {
class ExperimentManager;
}  // namespace tpcd::experiment

class PrivacySandboxSettingsDelegate
    : public privacy_sandbox::PrivacySandboxSettings::Delegate {
 public:
  PrivacySandboxSettingsDelegate(
      Profile* profile,
      tpcd::experiment::ExperimentManager* experiment_manager);
  ~PrivacySandboxSettingsDelegate() override;

  // PrivacySandboxSettings::Delegate:
  bool IsRestrictedNoticeEnabled() const override;
  bool IsPrivacySandboxRestricted() const override;
  bool IsPrivacySandboxCurrentlyUnrestricted() const override;
  bool IsIncognitoProfile() const override;
  bool HasAppropriateTopicsConsent() const override;
  bool IsSubjectToM1NoticeRestricted() const override;
  bool IsCookieDeprecationExperimentEligible() const override;
  privacy_sandbox::TpcdExperimentEligibility
  GetCookieDeprecationExperimentCurrentEligibility() const override;
  bool IsCookieDeprecationLabelAllowed() const override;
  bool AreThirdPartyCookiesBlockedByCookieDeprecationExperiment()
      const override;

#if BUILDFLAG(IS_ANDROID)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

 private:
  bool PrivacySandboxRestrictedNoticeRequired() const;
  bool IsSubjectToEnterprisePolicies() const;
  raw_ptr<Profile> profile_;
  // TODO(linnan): Remove this field when
  // `IsCookieDeprecationExperimentEligible()` consults `ExperimentManager`.
  mutable std::optional<bool> is_cookie_deprecation_experiment_eligible_;

  // The experiment manager is a singleton and lives forever.
  raw_ptr<tpcd::experiment::ExperimentManager> experiment_manager_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_DELEGATE_H_
