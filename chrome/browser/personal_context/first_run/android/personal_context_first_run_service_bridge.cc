// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the native methods of the Java proxy class
// org.chromium.chrome.browser.personal_context.first_run
// .PersonalContextFirstRunService.
// It acts as a bridge between the Android UI layer (e.g.,
// AtMemoryBottomSheetMediator) and the C++ KeyedService
// (PersonalContextFirstRunService). Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/personal_context/first_run/jni_headers/PersonalContextFirstRunService_jni.h"
#include "chrome/browser/personal_context/first_run/personal_context_first_run_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"

namespace {

personal_context::PersonalContextFirstRunService*
GetPersonalContextFirstRunService(
    const base::android::JavaRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  CHECK(profile);
  return PersonalContextFirstRunServiceFactory::GetForProfile(profile);
}

}  // namespace

static bool JNI_PersonalContextFirstRunService_ShouldShowAmbientAutofillNotice(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_profile) {
  auto* service = GetPersonalContextFirstRunService(j_profile);
  CHECK(service);
  return service->ShouldShowPersonalContextAmbientAutofillNotice();
}

static void
JNI_PersonalContextFirstRunService_AmbientAutofillNoticeAcknowledged(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_profile) {
  auto* service = GetPersonalContextFirstRunService(j_profile);
  CHECK(service);
  service->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();
}

static bool JNI_PersonalContextFirstRunService_ShouldShowAtMemoryNotice(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_profile) {
  auto* service = GetPersonalContextFirstRunService(j_profile);
  CHECK(service);
  return service->ShouldShowPersonalContextAtMemoryNotice();
}

static void JNI_PersonalContextFirstRunService_AtMemoryNoticeAcknowledged(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_profile) {
  auto* service = GetPersonalContextFirstRunService(j_profile);
  CHECK(service);
  service->MarkPersonalContextInAtMemoryNoticeAsAcknowledged();
}

DEFINE_JNI(PersonalContextFirstRunService)
