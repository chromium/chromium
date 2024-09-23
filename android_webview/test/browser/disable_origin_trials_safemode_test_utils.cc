// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#include "content/public/common/content_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/webview_instrumentation_test_native_jni/DisableOriginTrialsSafeModeTestUtils_jni.h"

namespace android_webview {

static jboolean
JNI_DisableOriginTrialsSafeModeTestUtils_IsNonDeprecationTrialDisabled(
    JNIEnv* env) {
  const char kFrobulateThirdPartyTrialName[] = "FrobulateThirdParty";
  content::ContentClient* client = content::GetContentClientForTesting();
  blink::OriginTrialPolicy* policy = client->GetOriginTrialPolicy();
  return policy->IsFeatureDisabled(kFrobulateThirdPartyTrialName);
}

static jboolean
JNI_DisableOriginTrialsSafeModeTestUtils_IsDeprecationTrialDisabled(
    JNIEnv* env) {
  const char kFrobulateDeprecationTrialName[] = "FrobulateDeprecation";
  content::ContentClient* client = content::GetContentClientForTesting();
  blink::OriginTrialPolicy* policy = client->GetOriginTrialPolicy();
  return policy->IsFeatureDisabled(kFrobulateDeprecationTrialName);
}

static jboolean JNI_DisableOriginTrialsSafeModeTestUtils_DoesPolicyExist(
    JNIEnv* env) {
  content::ContentClient* client = content::GetContentClientForTesting();
  blink::OriginTrialPolicy* policy = client->GetOriginTrialPolicy();
  return policy != nullptr;
}

static jboolean JNI_DisableOriginTrialsSafeModeTestUtils_IsFlagSet(
    JNIEnv* env) {
  content::ContentClient* client = content::GetContentClientForTesting();
  auto* policy =
      (embedder_support::OriginTrialPolicyImpl*)client->GetOriginTrialPolicy();
  return policy->GetAllowOnlyDeprecationTrials();
}
}  // namespace android_webview
