// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/public/glic_enabling.h"

#include "chrome/browser/glic/android/jni_headers/GlicEnabling_jni.h"
#include "chrome/browser/profiles/profile.h"
namespace glic {
bool JNI_GlicEnabling_IsEnabledByFlags(JNIEnv* env) {
  return GlicEnabling::IsEnabledByFlags();
}
bool JNI_GlicEnabling_IsProfileEligible(JNIEnv* env, Profile* profile) {
  return GlicEnabling::IsProfileEligible(profile);
}
bool JNI_GlicEnabling_IsEnabledForProfile(JNIEnv* env, Profile* profile) {
  return GlicEnabling::IsEnabledForProfile(profile);
}
bool JNI_GlicEnabling_ShouldShowSettingsPage(JNIEnv* env, Profile* profile) {
  return GlicEnabling::ShouldShowSettingsPage(profile);
}
bool JNI_GlicEnabling_IsReadyForProfile(JNIEnv* env, Profile* profile) {
  return GlicEnabling::IsReadyForProfile(profile);
}
}  // namespace glic
DEFINE_JNI(GlicEnabling)
