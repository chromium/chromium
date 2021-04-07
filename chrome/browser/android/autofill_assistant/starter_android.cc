// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/starter_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/features/autofill_assistant/jni_headers_public/Starter_jni.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill_assistant {

static jlong JNI_Starter_FromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);
  StarterAndroid::CreateForWebContents(web_contents);
  auto* tab_helper_android = StarterAndroid::FromWebContents(web_contents);
  return reinterpret_cast<intptr_t>(tab_helper_android);
}

StarterAndroid::StarterAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents),
      website_login_manager_(std::make_unique<WebsiteLoginManagerImpl>(
          ChromePasswordManagerClient::FromWebContents(web_contents),
          web_contents)) {}

StarterAndroid::~StarterAndroid() = default;

void StarterAndroid::Attach(JNIEnv* env, const JavaParamRef<jobject>& jcaller) {
  Detach(env, jcaller);
  java_object_ = base::android::ScopedJavaGlobalRef<jobject>(jcaller);

  starter_ =
      std::make_unique<Starter>(web_contents_, this, ukm::UkmRecorder::Get());
}

void StarterAndroid::Detach(JNIEnv* env, const JavaParamRef<jobject>& jcaller) {
  java_object_ = nullptr;
  starter_.reset();
}

WebsiteLoginManager* StarterAndroid::GetWebsiteLoginManager() const {
  return website_login_manager_.get();
}

version_info::Channel StarterAndroid::GetChannel() const {
  return chrome::GetChannel();
}

bool StarterAndroid::GetFeatureModuleInstalled() const {
  return Java_Starter_getFeatureModuleInstalled(
      base::android::AttachCurrentThread());
}

void StarterAndroid::InstallFeatureModule(
    bool show_ui,
    base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
        callback) {
  DCHECK(java_object_);
  feature_module_installation_finished_callback_ = std::move(callback);
  Java_Starter_installFeatureModule(base::android::AttachCurrentThread(),
                                    java_object_, show_ui);
}

void StarterAndroid::OnFeatureModuleInstalled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint result) {
  DCHECK(feature_module_installation_finished_callback_);
  std::move(feature_module_installation_finished_callback_)
      .Run(static_cast<Metrics::FeatureModuleInstallation>(result));
}

void StarterAndroid::OnInteractabilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean is_interactable) {
  if (!is_interactable || !starter_) {
    return;
  }

  // The tab has become interactable again. Users may have adjusted their
  // settings, so we need to check them again.
  starter_->CheckSettings();
}

bool StarterAndroid::GetIsFirstTimeUser() const {
  return Java_Starter_getIsFirstTimeUser(base::android::AttachCurrentThread());
}

void StarterAndroid::SetIsFirstTimeUser(bool first_time_user) {
  Java_Starter_setIsFirstTimeUser(base::android::AttachCurrentThread(),
                                  first_time_user);
}

bool StarterAndroid::GetOnboardingAccepted() const {
  return Java_Starter_getOnboardingAccepted(
      base::android::AttachCurrentThread());
}

void StarterAndroid::SetOnboardingAccepted(bool accepted) {
  Java_Starter_setOnboardingAccepted(base::android::AttachCurrentThread(),
                                     accepted);
}

void StarterAndroid::ShowOnboarding(
    bool use_dialog_onboarding,
    const TriggerContext& trigger_context,
    base::OnceCallback<void(bool shown, OnboardingResult result)> callback) {
  DCHECK(java_object_);
  if (onboarding_finished_callback_) {
    DCHECK(false) << "onboarding requested while already being shown";
    std::move(callback).Run(false, OnboardingResult::DISMISSED);
    return;
  }
  onboarding_finished_callback_ = std::move(callback);

  std::vector<std::string> keys;
  std::vector<std::string> values;
  for (const auto& param : trigger_context.GetScriptParameters().ToProto()) {
    keys.emplace_back(param.name());
    values.emplace_back(param.value());
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Starter_showOnboarding(env, java_object_, use_dialog_onboarding,
                              base::android::ConvertUTF8ToJavaString(
                                  env, trigger_context.GetInitialUrl()),
                              base::android::ConvertUTF8ToJavaString(
                                  env, trigger_context.GetExperimentIds()),
                              base::android::ToJavaArrayOfStrings(env, keys),
                              base::android::ToJavaArrayOfStrings(env, values));
}

void StarterAndroid::HideOnboarding() {
  // TODO(arbesser): implement this.
}

void StarterAndroid::OnOnboardingFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean shown,
    jint result) {
  DCHECK(onboarding_finished_callback_);
  std::move(onboarding_finished_callback_)
      .Run(shown, static_cast<OnboardingResult>(result));
}

bool StarterAndroid::GetProactiveHelpSettingEnabled() const {
  return Java_Starter_getProactiveHelpSettingEnabled(
      base::android::AttachCurrentThread());
}

void StarterAndroid::SetProactiveHelpSettingEnabled(bool enabled) {
  Java_Starter_setProactiveHelpSettingEnabled(
      base::android::AttachCurrentThread(), enabled);
}

bool StarterAndroid::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return Java_Starter_getMakeSearchesAndBrowsingBetterSettingEnabled(
      base::android::AttachCurrentThread());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(StarterAndroid)

}  // namespace autofill_assistant
