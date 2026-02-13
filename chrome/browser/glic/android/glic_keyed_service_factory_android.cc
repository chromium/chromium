// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_keyed_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicKeyedServiceFactory_jni.h"

namespace glic {

static base::android::ScopedJavaLocalRef<jobject>
JNI_GlicKeyedServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  return GlicKeyedService::GetJavaObject(service);
}

}  // namespace glic

DEFINE_JNI(GlicKeyedServiceFactory)
