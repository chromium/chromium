// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_enabling.h"

#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/android/jni_headers/GlicEnabling_jni.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {
bool JNI_GlicEnabling_IsEnabledByFlags(JNIEnv* env) {
  return GlicEnabling::IsEnabledByGlobalCriteria();
}
bool JNI_GlicEnabling_IsProfileEligible(JNIEnv* env, Profile* profile) {
  return GlicEnabling::IsProfileEligible(profile);
}
bool JNI_GlicEnabling_IsEnabledForProfile(JNIEnv* env, Profile* profile) {
  return GlicEnabling::IsEnabledForProfile(profile);
}
bool JNI_GlicEnabling_WasPreviouslyNotAllowed(JNIEnv* env, Profile* profile) {
  return GlicEnabling::WasPreviouslyNotAllowed(profile);
}
bool JNI_GlicEnabling_ShouldShowSettingsPage(JNIEnv* env, Profile* profile) {
  return GlicEnabling::ShouldShowSettingsPage(profile);
}
bool JNI_GlicEnabling_IsReadyForProfile(JNIEnv* env, Profile* profile) {
  return GlicEnabling::IsReadyForProfile(profile);
}
bool JNI_GlicEnabling_ShouldShowWebActuationToggle(JNIEnv* env,
                                                   Profile* profile) {
  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  return glic_service &&
         glic_service->enabling().ShouldShowWebActuationToggle();
}

bool JNI_GlicEnabling_IsDisabledByPolicy(JNIEnv* env, Profile* profile) {
  return GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();
}

bool JNI_GlicEnabling_IsProfileManaged(JNIEnv* env, Profile* profile) {
  policy::ManagementService* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  return management_service && management_service->IsManaged();
}

void JNI_GlicEnabling_SetBypassEnablementChecksForTesting(JNIEnv* env,
                                                          bool bypass) {
  GlicEnabling::SetBypassEnablementChecksForTesting(bypass);
}
}  // namespace glic
DEFINE_JNI(GlicEnabling)
