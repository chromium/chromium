// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/collaboration_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/collaboration/public/collaboration_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/collaboration/jni_headers/CollaborationServiceFactory_jni.h"

namespace collaboration {

static base::android::ScopedJavaLocalRef<jobject>
JNI_CollaborationServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);
  CollaborationService* service =
      CollaborationServiceFactory::GetForProfile(profile);
  return CollaborationService::GetJavaObject(service);
}

}  // namespace collaboration
