// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_prefs.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/composeplate/android/jni_headers/ComposeplateUtils_jni.h"

// static
jboolean JNI_ComposeplateUtils_IsAimEntrypointEligible(JNIEnv* env,
                                                       Profile* profile) {
  DCHECK(profile);

  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  if (!aim_eligibility_service) {
    return false;
  }

  // If the server eligibility is enabled, check overall eligibility alone.
  // The service will control locale rollout so there's no need to check locale.
  if (aim_eligibility_service->IsServerEligibilityEnabled()) {
    return aim_eligibility_service->IsAimEligible();
  }

  if (!aim_eligibility_service->IsAimLocallyEligible()) {
    return false;
  }

  return aim_eligibility_service->IsCountry("us") &&
         aim_eligibility_service->IsLanguage("en");
}
