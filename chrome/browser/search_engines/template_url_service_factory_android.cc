// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/android/chrome_jni_headers/TemplateUrlServiceFactory_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/switches.h"

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

static jboolean IsDefaultSearchEngineGoogle(JNIEnv* env) {
  const TemplateURL* default_search_provider =
      GetTemplateUrlService()->GetDefaultSearchProvider();
  return default_search_provider &&
         default_search_provider->url_ref().HasGoogleBaseURLs(
             GetTemplateUrlService()->search_terms_data());
}

static jboolean JNI_TemplateUrlServiceFactory_DoesDefaultSearchEngineHaveLogo(
    JNIEnv* env) {
  // |kSearchProviderLogoURL| applies to all search engines (Google or
  // third-party).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          search_provider_logos::switches::kSearchProviderLogoURL)) {
    return true;
  }

  // Google always has a logo.
  if (IsDefaultSearchEngineGoogle(env))
    return true;

  // Third-party search engines can have a doodle specified via the command
  // line, or a static logo or doodle from the TemplateURLService.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          search_provider_logos::switches::kThirdPartyDoodleURL)) {
    return true;
  }
  const TemplateURL* default_search_provider =
      GetTemplateUrlService()->GetDefaultSearchProvider();
  return default_search_provider &&
         (default_search_provider->doodle_url().is_valid() ||
          default_search_provider->logo_url().is_valid());
}
