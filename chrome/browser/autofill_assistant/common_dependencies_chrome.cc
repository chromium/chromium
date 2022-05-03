// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill_assistant/annotate_dom_model_service_factory.h"
#include "chrome/browser/autofill_assistant/assistant_field_trial_util_chrome.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/dependencies_util.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

using ::autofill::PersonalDataManager;
using ::content::WebContents;
using ::password_manager::PasswordManagerClient;
using ::variations::VariationsService;

namespace autofill_assistant {

CommonDependenciesChrome::CommonDependenciesChrome() = default;

std::unique_ptr<AssistantFieldTrialUtil>
CommonDependenciesChrome::CreateFieldTrialUtil() const {
  return std::make_unique<AssistantFieldTrialUtilChrome>();
}

std::string CommonDependenciesChrome::GetLocale() const {
  return g_browser_process->GetApplicationLocale();
}

std::string CommonDependenciesChrome::GetCountryCode() const {
  return dependencies_util::GetCountryCode(
      g_browser_process->variations_service());
}

PersonalDataManager* CommonDependenciesChrome::GetPersonalDataManager() const {
  // TODO(b/201964911): Using |GetLastUsedProfile| is probably not the best
  // option for desktop. Consider passing a profile on instantiation instead.
  return autofill::PersonalDataManagerFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
}

PasswordManagerClient* CommonDependenciesChrome::GetPasswordManagerClient(
    WebContents* web_contents) const {
  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

std::string CommonDependenciesChrome::GetSignedInEmail(
    WebContents* web_contents) const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!identity_manager) {
    return std::string();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
      .email;
}

AnnotateDomModelService*
CommonDependenciesChrome::GetOrCreateAnnotateDomModelService(
    content::BrowserContext* browser_context) const {
  return AnnotateDomModelServiceFactory::GetForBrowserContext(browser_context);
}

bool CommonDependenciesChrome::IsWebLayer() const {
  return false;
}

signin::IdentityManager* CommonDependenciesChrome::GetIdentityManager(
    content::BrowserContext* browser_context) const {
  return IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

version_info::Channel CommonDependenciesChrome::GetChannel() const {
  return chrome::GetChannel();
}

}  // namespace autofill_assistant
