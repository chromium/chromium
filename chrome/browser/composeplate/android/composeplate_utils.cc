// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/composeplate/android/jni_headers/ComposeplateUtils_jni.h"

// static
static bool JNI_ComposeplateUtils_IsAimEntrypointEligible(JNIEnv* env,
                                                          Profile* profile) {
  DCHECK(profile);
  AimEligibilityService* aim_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (!base::FeatureList::IsEnabled(omnibox::kAim3pEntrypoint)) {
    return aim_service != nullptr && aim_service->IsAimEligible();
  }

  return OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
      aim_service, AiModeButtonServiceFactory::GetForProfile(profile),
      TemplateURLServiceFactory::GetForProfile(profile));
}

// static
static bool JNI_ComposeplateUtils_IsEnabledByPolicy(JNIEnv* env,
                                                    Profile* profile) {
  DCHECK(profile);
  // TODO(crbug.com/469142288): this should only disable sharing; for now the
  // resolution is that in M144 we disable all of the fusebox.
  return contextual_search::ContextualSearchService::IsContextSharingEnabled(
      profile->GetPrefs());
}

DEFINE_JNI(ComposeplateUtils)
