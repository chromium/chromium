// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicMetrics_jni.h"

namespace glic {

static std::string JNI_GlicMetrics_GetMetricsInfix(JNIEnv* env,
                                                   Profile* profile) {
  return std::string(GetGlicProviderSpec(profile).GetMetricsInfix());
}

}  // namespace glic

DEFINE_JNI(GlicMetrics)
