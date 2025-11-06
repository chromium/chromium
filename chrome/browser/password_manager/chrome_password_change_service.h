// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_change_service_interface.h"
#include "components/password_manager/core/browser/password_form.h"

class GURL;

namespace autofill {
class LogRouter;
}  // namespace autofill

namespace affiliations {
class AffiliationService;
}

class OptimizationGuideKeyedService;

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFeatureManager;
class PasswordManagerSettingsService;
}

class PrefService;

// Password change availability state used for UMA. Corresponds to
// `PasswordChangeAvailability` in enums.xml.
//
// These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(PasswordChangeAvailability)
enum class PasswordChangeAvailability {
  kAvailable = 0,
  kPasswordGenerationDisabled = 1,
  kModelExecutionNotAllowed = 2,
  kPasswordSavingDisabled = 3,
  kDisabledByPolicy = 4,
  kFeatureDisabled = 5,
  kUnsupportedLanguage = 6,
  kUnsupportedCountryCode = 7,
  kNotSupportedSite = 8,
  kNoSavedPasswords = 9,
  kThrottled = 10,
  kMaxValue = kThrottled,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordChangeAvailability)

class ChromePasswordChangeService
    : public KeyedService,
      public password_manager::PasswordChangeServiceInterface,
      public PasswordChangeDelegate::Observer {
 public:
  // Callback which allows to open a new tab for password change. Useful for
  // testing UI interactions in browser tests.
  using OpenNewTabCallback =
      base::RepeatingCallback<content::WebContents*(const GURL&,
                                                    content::WebContents*)>;
  static constexpr char kHasPasswordChangeUrlHistogram[] =
      "PasswordManager.HasPasswordChangeUrl";

  ChromePasswordChangeService(
      PrefService* pref_service,
      affiliations::AffiliationService* affiliation_service,
      OptimizationGuideKeyedService* optimization_keyed_service,
      password_manager::PasswordManagerSettingsService* settings_service,
      std::unique_ptr<password_manager::PasswordFeatureManager> feature_manager,
      autofill::LogRouter* log_router);
  ~ChromePasswordChangeService() override;

  // Indicates that password change will be proposed to the user for a given
  // `credentials`. `originator` belongs to a tab which initiated the process.
  virtual void OfferPasswordChangeUi(password_manager::PasswordForm credentials,
                                     content::WebContents* originator);

  // Responds with PasswordChangeDelegate for a given `web_contents`.
  // The same object is returned for a tab which initiated password change and a
  // tab where password change is performed. Returns nullptr if `web_contents`
  // isn't associated with any delegate.
  virtual PasswordChangeDelegate* GetPasswordChangeDelegate(
      content::WebContents* web_contents);

  // PasswordChangeServiceInterface implementation.
  bool IsPasswordChangeAvailable() const override;
  bool IsPasswordChangeSupported(
      const GURL& url,
      const autofill::LanguageCode& page_language) const override;
  void RecordLoginAttemptQuality(
      password_manager::LogInWithChangedPasswordOutcome login_outcome,
      const GURL& page_url) const override;

  // Checks if user has interacted with the feature and only then general
  // availability.
  bool UserIsActivePasswordChangeUser() const;

 private:
  // PasswordChangeDelegate::Observer impl.
  void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) override;

  // KeyedService impl.
  void Shutdown() override;

  PasswordChangeAvailability GetGeneralAvailability() const;
  PasswordChangeAvailability GetPerSiteAvailability(
      const GURL& url,
      const autofill::LanguageCode& page_language) const;

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<affiliations::AffiliationService> affiliation_service_;
  const raw_ptr<OptimizationGuideKeyedService> optimization_keyed_service_;
  const raw_ptr<password_manager::PasswordManagerSettingsService>
      settings_service_;
  std::unique_ptr<password_manager::PasswordFeatureManager> feature_manager_;

  std::vector<std::unique_ptr<PasswordChangeDelegate>>
      password_change_delegates_;

  // The router for logs. Maybe be null in tests.
  const raw_ptr<autofill::LogRouter> log_router_;

  base::WeakPtrFactory<ChromePasswordChangeService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
