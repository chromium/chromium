// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search_engines/android/jni_headers/TemplateUrlServiceFactory_jni.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_TemplateUrlServiceFactory_GetTemplateUrlService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  auto* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  return TemplateURLServiceFactory::GetForProfile(profile)->GetJavaObject();
}
