// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/privacy_sandbox/android/jni_headers/PrivacySandboxBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using jni_zero::ScopedJavaLocalRef;

static bool JNI_PrivacySandboxBridge_IsRelatedWebsiteSetsDataAccessEnabled(
    Profile* profile) {
  return PrivacySandboxServiceFactory::GetForProfile(profile)
      ->IsRelatedWebsiteSetsDataAccessEnabled();
}

static bool JNI_PrivacySandboxBridge_IsRelatedWebsiteSetsDataAccessManaged(
    Profile* profile) {
  return PrivacySandboxServiceFactory::GetForProfile(profile)
      ->IsRelatedWebsiteSetsDataAccessManaged();
}

static void JNI_PrivacySandboxBridge_SetRelatedWebsiteSetsDataAccessEnabled(
    Profile* profile,
    bool enabled) {
  PrivacySandboxServiceFactory::GetForProfile(profile)
      ->SetRelatedWebsiteSetsDataAccessEnabled(enabled);
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetRelatedWebsiteSetOwner(
    JNIEnv* env,
    Profile* profile,
    const std::string& member_origin) {
  auto rws_owner = PrivacySandboxServiceFactory::GetForProfile(profile)
                       ->GetRelatedWebsiteSetOwner(GURL(member_origin));

  if (!rws_owner.has_value()) {
    return nullptr;
  }

  return ConvertUTF8ToJavaString(env, rws_owner->GetURL().GetHost());
}

static bool JNI_PrivacySandboxBridge_IsPartOfManagedRelatedWebsiteSet(
    Profile* profile,
    const std::string& origin) {
  return PrivacySandboxServiceFactory::GetForProfile(profile)
      ->IsPartOfManagedRelatedWebsiteSet(net::SchemefulSite(GURL(origin)));
}

DEFINE_JNI(PrivacySandboxBridge)
