// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill_assistant/annotate_dom_model_service_factory.h"
#include "chrome/browser/autofill_assistant/assistant_field_trial_util_chrome.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/dependencies_util.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::autofill::PersonalDataManager;
using ::content::WebContents;
using ::password_manager::PasswordManagerClient;
using ::variations::VariationsService;

namespace autofill_assistant {

CommonDependenciesChrome::CommonDependenciesChrome(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

std::unique_ptr<AssistantFieldTrialUtil>
CommonDependenciesChrome::CreateFieldTrialUtil() const {
  return std::make_unique<AssistantFieldTrialUtilChrome>();
}

std::string CommonDependenciesChrome::GetLocale() const {
  return g_browser_process->GetApplicationLocale();
}

std::string CommonDependenciesChrome::GetLatestCountryCode() const {
  return dependencies_util::GetLatestCountryCode(
      g_browser_process->variations_service());
}

std::string CommonDependenciesChrome::GetStoredPermanentCountryCode() const {
  return dependencies_util::GetStoredPermanentCountryCode(
      g_browser_process->variations_service());
}

PersonalDataManager* CommonDependenciesChrome::GetPersonalDataManager() const {
  if (!browser_context_)
    return nullptr;

  return autofill::PersonalDataManagerFactory::GetForBrowserContext(
      browser_context_);
}

PasswordManagerClient* CommonDependenciesChrome::GetPasswordManagerClient(
    WebContents* web_contents) const {
  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

PrefService* CommonDependenciesChrome::GetPrefs() const {
  return GetProfile()->GetPrefs();
}

std::string CommonDependenciesChrome::GetSignedInEmail() const {
  signin::IdentityManager* identity_manager = GetIdentityManager();
  if (!identity_manager)
    return std::string();

  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
      .email;
}

security_state::SecurityLevel CommonDependenciesChrome::GetSecurityLevel(
    content::WebContents* web_contents) const {
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  return SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityLevel();
}

bool CommonDependenciesChrome::IsSupervisedUser() const {
  signin::IdentityManager* identity_manager = GetIdentityManager();
  if (!identity_manager)
    return false;

  std::string gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync).gaia;
  return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id)
             .capabilities.is_subject_to_parental_controls() ==
         signin::Tribool::kTrue;
}

bool CommonDependenciesChrome::IsAllowedForMachineLearning() const {
  signin::IdentityManager* identity_manager = GetIdentityManager();
  if (!identity_manager)
    return true;

  std::string gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync).gaia;
  return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id)
             .capabilities.is_allowed_for_machine_learning() !=
         signin::Tribool::kFalse;
}

AnnotateDomModelService*
CommonDependenciesChrome::GetOrCreateAnnotateDomModelService() const {
  return AnnotateDomModelServiceFactory::GetForBrowserContext(browser_context_);
}

bool CommonDependenciesChrome::IsWebLayer() const {
  return false;
}

signin::IdentityManager* CommonDependenciesChrome::GetIdentityManager() const {
  return IdentityManagerFactory::GetForProfile(GetProfile());
}

consent_auditor::ConsentAuditor* CommonDependenciesChrome::GetConsentAuditor()
    const {
  return ConsentAuditorFactory::GetForProfile(GetProfile());
}

version_info::Channel CommonDependenciesChrome::GetChannel() const {
  return chrome::GetChannel();
}

bool CommonDependenciesChrome::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

bool CommonDependenciesChrome::GetMetricsReportingEnabled() const {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

Profile* CommonDependenciesChrome::GetProfile() const {
  return Profile::FromBrowserContext(browser_context_);
}

}  // namespace autofill_assistant
