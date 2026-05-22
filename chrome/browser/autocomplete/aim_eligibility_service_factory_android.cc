// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/search_engines/android/jni_headers/AimEligibilityServiceFactory_jni.h"

static bool JNI_AimEligibilityServiceFactory_IsAimStarterPackEnabled(
    JNIEnv* env,
    Profile* profile) {
  DCHECK(profile);
  return OmniboxFieldTrial::IsAimStarterPackEnabled(
      AimEligibilityServiceFactory::GetForProfile(profile));
}

DEFINE_JNI(AimEligibilityServiceFactory)
