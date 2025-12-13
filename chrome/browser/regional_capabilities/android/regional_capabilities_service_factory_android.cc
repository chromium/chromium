// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/regional_capabilities/android/jni_headers/RegionalCapabilitiesServiceFactory_jni.h"

namespace regional_capabilities {

static base::android::ScopedJavaLocalRef<jobject>
JNI_RegionalCapabilitiesServiceFactory_GetRegionalCapabilitiesService(
    JNIEnv* env,
    Profile* profile) {
  DCHECK(profile);
  return RegionalCapabilitiesServiceFactory::GetForProfile(profile)
      ->GetJavaObject();
}

}  // namespace regional_capabilities

DEFINE_JNI(RegionalCapabilitiesServiceFactory)
