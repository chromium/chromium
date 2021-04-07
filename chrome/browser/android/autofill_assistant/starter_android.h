// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_STARTER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_STARTER_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/starter.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill_assistant {

// C++ helper for the java-side |Starter|. Initializes the native-side |Starter|
// and serves as its Android platform delegate.
//
// This class is intended to be instantiated from Java via |FromWebContents|, at
// an appropriate time when the Java dependencies are ready and |Attach| will be
// called shortly after creation.
class StarterAndroid : public StarterPlatformDelegate,
                       public content::WebContentsUserData<StarterAndroid> {
 public:
  ~StarterAndroid() override;
  StarterAndroid(const StarterAndroid&) = delete;
  StarterAndroid& operator=(const StarterAndroid&) = delete;

  // Attaches this instance to the java-side instance. Only call this with a
  // |jcaller| which is ready to serve requests by native!
  void Attach(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);

  // Detaches this instance from the java-side instance.
  void Detach(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);

  // Implements StarterPlatformDelegate:
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  version_info::Channel GetChannel() const override;
  bool GetFeatureModuleInstalled() const override;
  void InstallFeatureModule(
      bool show_ui,
      base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
          callback) override;
  bool GetIsFirstTimeUser() const override;
  void SetIsFirstTimeUser(bool first_time_user) override;
  bool GetOnboardingAccepted() const override;
  void SetOnboardingAccepted(bool accepted) override;
  void ShowOnboarding(
      bool use_dialog_onboarding,
      const TriggerContext& trigger_context,
      base::OnceCallback<void(bool shown, OnboardingResult result)> callback)
      override;
  void HideOnboarding() override;
  bool GetProactiveHelpSettingEnabled() const override;
  void SetProactiveHelpSettingEnabled(bool enabled) override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;

  // Called by Java when the feature module installation has finished.
  void OnFeatureModuleInstalled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint jresult);

  // Called by Java when the onboarding has finished.
  void OnOnboardingFinished(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jcaller,
                            jboolean shown,
                            jint jresult);

  // Called by Java whenever the interactability of the tab has changed.
  void OnInteractabilityChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean is_interactable);

 private:
  friend class content::WebContentsUserData<StarterAndroid>;
  explicit StarterAndroid(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  content::WebContents* web_contents_;
  std::unique_ptr<Starter> starter_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  std::unique_ptr<WebsiteLoginManager> website_login_manager_;
  base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
      feature_module_installation_finished_callback_;
  base::OnceCallback<void(bool shown, OnboardingResult result)>
      onboarding_finished_callback_;
  base::WeakPtrFactory<StarterAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_STARTER_ANDROID_H_
