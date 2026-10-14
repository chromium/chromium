// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/ai_subscription_tier_names.h"

#include <cstdint>
#include <string>

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/subscription_eligibility/jni_headers/AiSubscriptionTierNames_jni.h"

namespace subscription_eligibility {

static std::u16string JNI_AiSubscriptionTierNames_GetShortName(JNIEnv* env,
                                                               int32_t tier) {
  return GetAiSubscriptionTierShortName(tier);
}

static std::u16string JNI_AiSubscriptionTierNames_GetFullName(JNIEnv* env,
                                                              int32_t tier) {
  return GetAiSubscriptionTierFullName(tier);
}

DEFINE_JNI(AiSubscriptionTierNames)

}  // namespace subscription_eligibility
