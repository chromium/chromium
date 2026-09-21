// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/metrics/metrics_service.h"
#include "ui/accessibility/accessibility_features.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/readaloud/android/features_jni_headers/ReadAloudFeatures_jni.h"

namespace readaloud {

static std::string JNI_ReadAloudFeatures_GetMetricsId() {
  if (g_browser_process && g_browser_process->metrics_service()) {
    return g_browser_process->metrics_service()->GetClientId();
  }
  return "";
}

static std::string JNI_ReadAloudFeatures_GetServerExperimentFlag() {
  base::FieldTrial* trial =
      base::FeatureList::GetInstance()->GetAssociatedFieldTrialByFeatureName(
          chrome::android::kReadAloudServerExperiments.name);
  if (!trial) {
    return "";
  }
  return base::StrCat({trial->trial_name(), "_", trial->group_name()});
}

static bool JNI_ReadAloudFeatures_IsServerSynthesizerEnabled() {
  return features::IsReadAloudServerSynthesizerEnabled();
}

}  // namespace readaloud

DEFINE_JNI(ReadAloudFeatures)
