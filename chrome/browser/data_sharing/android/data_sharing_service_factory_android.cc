// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/data_sharing_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/data_sharing/jni_headers/DataSharingServiceFactory_jni.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_DataSharingServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);
  data_sharing::DataSharingService* service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  return data_sharing::DataSharingService::GetJavaObject(service);
}
