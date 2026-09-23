// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/search_engine_service_android.h"

#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/search_engines/template_url_service.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/SearchEngineService_jni.h"
#include "components/search_engines/android/jni_headers/AiModeButtonUiConfig_jni.h"

using base::android::ScopedJavaLocalRef;

SearchEngineServiceAndroid::SearchEngineServiceAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    Profile* profile)
    : java_ref_(obj), profile_(profile) {
  if (auto* ai_mode_button_service =
          AiModeButtonServiceFactory::GetForProfile(profile_)) {
    // RegisterOnConfigChanged() synchronously invokes the callback with the
    // current config upon registration, bootstrapping the initial state.
    ai_mode_config_subscription_ =
        ai_mode_button_service->RegisterOnConfigChanged(base::BindRepeating(
            &SearchEngineServiceAndroid::OnAiModeButtonConfigChanged,
            weak_factory_.GetWeakPtr()));
  }
  if (auto* aim_eligibility_service =
          AimEligibilityServiceFactory::GetForProfile(profile_)) {
    aim_eligibility_subscription_ =
        aim_eligibility_service->RegisterEligibilityChangedCallback(
            base::BindRepeating(
                &SearchEngineServiceAndroid::OnEligibilityChanged,
                weak_factory_.GetWeakPtr()));
  }
}

SearchEngineServiceAndroid::~SearchEngineServiceAndroid() = default;

void SearchEngineServiceAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void SearchEngineServiceAndroid::OnEligibilityChanged() {
  auto* ai_mode_button_service =
      AiModeButtonServiceFactory::GetForProfile(profile_);
  OnAiModeButtonConfigChanged(ai_mode_button_service
                                  ? ai_mode_button_service->GetCurrentConfig()
                                  : nullptr);
}

void SearchEngineServiceAndroid::OnAiModeButtonConfigChanged(
    const AiModeButtonUiConfig* config) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!config) {
    Java_SearchEngineService_onAiModeButtonUiConfigChanged(env, java_ref_,
                                                           nullptr);
    return;
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);
  auto* ai_mode_button_service =
      AiModeButtonServiceFactory::GetForProfile(profile_);

  if (!OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(aim_eligibility_service,
                                                        ai_mode_button_service,
                                                        template_url_service)) {
    Java_SearchEngineService_onAiModeButtonUiConfigChanged(env, java_ref_,
                                                           nullptr);
    return;
  }

  ScopedJavaLocalRef<jobject> java_config =
      Java_AiModeButtonUiConfig_Constructor(
          env, config->text, config->tooltip, config->a11y_label,
          config->context_menu_label, config->placeholder_text,
          GURL(config->favicon_url), std::string(config->navigation_url),
          GURL(config->navigation_url_empty));

  Java_SearchEngineService_onAiModeButtonUiConfigChanged(env, java_ref_,
                                                         java_config);
}

static int64_t JNI_SearchEngineService_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& caller,
    Profile* profile) {
  return reinterpret_cast<intptr_t>(
      new SearchEngineServiceAndroid(env, caller, profile));
}

DEFINE_JNI(SearchEngineService)
