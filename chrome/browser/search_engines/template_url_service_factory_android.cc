// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/android/jni_headers/TemplateUrlServiceFactory_jni.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"

namespace {
Profile* GetOriginalProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}
}  // namespace

static TemplateURLService* GetTemplateUrlService() {
  return TemplateURLServiceFactory::GetForProfile(GetOriginalProfile());
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_TemplateUrlServiceFactory_GetTemplateUrlService(JNIEnv* env) {
  return GetTemplateUrlService()->GetJavaObject();
}
