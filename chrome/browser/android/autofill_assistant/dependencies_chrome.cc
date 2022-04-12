// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/android/autofill_assistant/dependencies_chrome.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/android/features/autofill_assistant/jni_headers_public/AssistantStaticDependenciesChrome_jni.h"
#include "chrome/browser/android/autofill_assistant/annotate_dom_model_service_factory.h"
#include "chrome/browser/android/autofill_assistant/assistant_field_trial_util_chrome.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

using ::autofill::PersonalDataManager;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;
using ::content::WebContents;
using ::password_manager::PasswordManagerClient;
using ::variations::VariationsService;

namespace autofill_assistant {

static jlong JNI_AssistantStaticDependenciesChrome_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies) {
  // The dynamic_cast is necessary here to safely cast the resulting intptr back
  // to Dependencies using reinterpret_cast.
  return reinterpret_cast<intptr_t>(dynamic_cast<Dependencies*>(
      new DependenciesChrome(env, jstatic_dependencies)));
}

DependenciesChrome::DependenciesChrome(
    JNIEnv* env,
    const JavaParamRef<jobject>& jstatic_dependencies)
    : Dependencies(env, jstatic_dependencies) {}

std::unique_ptr<AssistantFieldTrialUtil>
DependenciesChrome::CreateFieldTrialUtil() const {
  return std::make_unique<AssistantFieldTrialUtilChrome>();
}

VariationsService* DependenciesChrome::GetVariationsService() const {
  return g_browser_process->variations_service();
}

PersonalDataManager* DependenciesChrome::GetPersonalDataManager() const {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
}

PasswordManagerClient* DependenciesChrome::GetPasswordManagerClient(
    WebContents* web_contents) const {
  return ChromePasswordManagerClient::FromWebContents(web_contents);
}

std::string DependenciesChrome::GetChromeSignedInEmailAddress(
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

AnnotateDomModelService* DependenciesChrome::GetOrCreateAnnotateDomModelService(
    content::BrowserContext* browser_context) const {
  return AnnotateDomModelServiceFactory::GetForBrowserContext(browser_context);
}

bool DependenciesChrome::IsCustomTab(const WebContents& web_contents) const {
  auto* tab_android = TabAndroid::FromWebContents(&web_contents);
  if (!tab_android) {
    return false;
  }

  return tab_android->IsCustomTab();
}

bool DependenciesChrome::IsWebLayer() const {
  return false;
}

}  // namespace autofill_assistant
