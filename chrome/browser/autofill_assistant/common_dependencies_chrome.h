// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_COMMON_DEPENDENCIES_CHROME_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_COMMON_DEPENDENCIES_CHROME_H_

#include "components/autofill_assistant/browser/common_dependencies.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

class PrefService;
class Profile;

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace autofill_assistant {

// Chrome implementation of the CommonDependencies interface.
class CommonDependenciesChrome : public CommonDependencies {
 public:
  explicit CommonDependenciesChrome(content::BrowserContext* browser_context);

  // CommonDependencies:
  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const override;
  std::string GetLocale() const override;
  std::string GetLatestCountryCode() const override;
  std::string GetStoredPermanentCountryCode() const override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const override;
  PrefService* GetPrefs() const override;
  std::string GetSignedInEmail() const override;
  bool IsSupervisedUser() const override;
  bool IsAllowedForMachineLearning() const override;
  // The AnnotateDomModelService is a KeyedService. There is only one per
  // BrowserContext.
  AnnotateDomModelService* GetOrCreateAnnotateDomModelService() const override;
  bool IsWebLayer() const override;
  signin::IdentityManager* GetIdentityManager() const override;
  consent_auditor::ConsentAuditor* GetConsentAuditor() const override;
  version_info::Channel GetChannel() const override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;
  bool GetMetricsReportingEnabled() const override;

 private:
  // Helper method to return the profile associated with this object's
  // `BrowserContext`.
  Profile* GetProfile() const;

  // The `BrowserContext` of these dependencies. Since dependencies are
  // injected either into class that extend either `WebContentsUserData<>` or
  // `KeyedService` (or objects with the same life time), it is safe to
  // assume that the `BrowserContext` remains alive and constant.
  const raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_COMMON_DEPENDENCIES_CHROME_H_
