// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ONBOARDING_COORDINATOR_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ONBOARDING_COORDINATOR_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace autofill_assistant {

static void JNI_BaseOnboardingCoordinator_FetchOnboardingDefinition(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jonboarding_coordinator,
    const base::android::JavaParamRef<jstring>& jintent,
    const base::android::JavaParamRef<jstring>& jlocale,
    jint timeout_ms);

}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ONBOARDING_COORDINATOR_BRIDGE_H_
