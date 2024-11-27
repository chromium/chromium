// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/safety_hub/android/jni_headers/SafetyHubHatsBridge_jni.h"

jboolean JNI_SafetyHubHatsBridge_TriggerHatsSurveyIfEnabled(
    JNIEnv* env,
    Profile* profile,
    const base::android::JavaParamRef<jobject>& jweb_contents_android,
    std::string& module_type,
    jboolean has_tapped_card,
    jboolean has_visited,
    std::string& global_state) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents_android);
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);

  if (hats_service) {
    return hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerSafetyHubAndroid, web_contents,
        /*timeout_ms=*/0,
        /*product_specific_bits_data=*/
        {{"Tapped card", has_tapped_card}, {"Has visited", has_visited}},
        /*product_specific_string_data=*/
        {{"Notification module type", module_type},
         {"Global state", global_state}},
        HatsService::NavigationBehaviour::ALLOW_ANY,
        /*success_callback=*/base::DoNothing(),
        /*failure_callback=*/base::DoNothing(),
        /*supplied_trigger_id=*/std::nullopt,
        HatsService::SurveyOptions(
            /*custom_invitation=*/std::nullopt,
            messages::MessageIdentifier::PROMPT_HATS_SAFETY_HUB));
  }

  return false;
}
