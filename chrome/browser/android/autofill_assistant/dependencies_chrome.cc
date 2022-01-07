// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/android/autofill_assistant/dependencies_chrome.h"

#include "base/android/scoped_java_ref.h"
#include "base/strings/string_piece.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantStaticDependenciesChrome_jni.h"
#include "chrome/browser/android/autofill_assistant/assistant_field_trial_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

using ::base::StringPiece;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;
using ::content::WebContents;
using ::variations::VariationsService;

namespace autofill_assistant {

static jlong JNI_AssistantStaticDependenciesChrome_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object) {
  return reinterpret_cast<intptr_t>(new DependenciesChrome(env, java_object));
}

DependenciesChrome::DependenciesChrome(JNIEnv* env,
                                       const JavaParamRef<jobject>& java_object)
    : Dependencies(env, java_object) {}

class AssistantFieldTrialUtilChrome : public AssistantFieldTrialUtil {
  bool RegisterSyntheticFieldTrial(
      base::StringPiece trial_name,
      base::StringPiece group_name) const override {
    return ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        trial_name, group_name);
  }
};

std::unique_ptr<AssistantFieldTrialUtil>
DependenciesChrome::CreateFieldTrialUtil() const {
  return std::make_unique<AssistantFieldTrialUtilChrome>();
}

variations::VariationsService* DependenciesChrome::GetVariationsService()
    const {
  return g_browser_process->variations_service();
}

std::string DependenciesChrome::GetChromeSignedInEmailAddress(
    WebContents* web_contents) const {
  CoreAccountInfo account_info =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  return account_info.email;
}

}  // namespace autofill_assistant
