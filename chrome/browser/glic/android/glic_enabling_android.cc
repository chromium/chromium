// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_enabling.h"

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/android/jni_headers/GlicEnabling_jni.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace glic {

namespace {
std::vector<
    std::unique_ptr<GlicEnabling::ScopedBypassEnablementChecksForTesting>>&
GetScopedBypassStack() {
  static base::NoDestructor<std::vector<
      std::unique_ptr<GlicEnabling::ScopedBypassEnablementChecksForTesting>>>
      stack;
  return *stack;
}
}  // namespace

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

bool JNI_GlicEnabling_IsPolicyEnforced(JNIEnv* env, Profile* profile) {
  if (!profile) {
    return false;
  }
  PrefService* prefs = profile->GetPrefs();
  return prefs &&
         prefs->IsManagedPreference(optimization_guide::prefs::kGeminiSettings);
}

void JNI_GlicEnabling_SetBypassEnablementChecksForTesting(JNIEnv* env,
                                                          bool bypass) {
  auto& stack = GetScopedBypassStack();
  if (bypass) {
    stack.push_back(std::make_unique<
                    GlicEnabling::ScopedBypassEnablementChecksForTesting>());
  } else if (!stack.empty()) {
    stack.pop_back();
  }
}

jboolean JNI_GlicEnabling_ExperimentalOptInIsNeeded(JNIEnv* env,
                                                    Profile* profile) {
  CHECK(glic::GlicEnabling::IsEnabledForProfile(profile));

  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  CHECK(glic_service);
  return glic_service->enabling().GetRequiredExperimentalOptIn() !=
         glic::RequiredExperimentalOptIn::kNotNeeded;
}

}  // namespace glic
DEFINE_JNI(GlicEnabling)
