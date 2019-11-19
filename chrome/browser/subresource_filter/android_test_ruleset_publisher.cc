// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/subresource_filter/jni_headers/TestRulesetPublisher_jni.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"

namespace {

void OnRulesetPublished(
    std::unique_ptr<subresource_filter::testing::TestRulesetCreator> creator,
    subresource_filter::RulesetService* service,
    base::android::ScopedJavaGlobalRef<jobject> publisher) {
  // Ensure the callback does not retain |publisher| by resetting it.
  service->SetRulesetPublishedCallbackForTesting(base::RepeatingClosure());
  creator.reset();
  Java_TestRulesetPublisher_onRulesetPublished(
      base::android::AttachCurrentThread(), publisher);
}

}  // namespace

void JNI_TestRulesetPublisher_CreateAndPublishRulesetDisallowingSuffixForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& publisher_param,
    const base::android::JavaParamRef<jstring>& suffix) {
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  auto creator =
      std::make_unique<subresource_filter::testing::TestRulesetCreator>();
  std::string suffix_str = base::android::ConvertJavaStringToUTF8(env, suffix);
  creator->CreateRulesetToDisallowURLsWithPathSuffix(suffix_str,
                                                     &test_ruleset_pair);

  subresource_filter::UnindexedRulesetInfo unindexed_ruleset_info;
  unindexed_ruleset_info.content_version =
      base::NumberToString(base::Hash(suffix_str));
  unindexed_ruleset_info.ruleset_path = test_ruleset_pair.unindexed.path;

  base::android::ScopedJavaGlobalRef<jobject> publisher;
  publisher.Reset(env, publisher_param);
  auto* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  ruleset_service->SetRulesetPublishedCallbackForTesting(base::BindRepeating(
      &OnRulesetPublished, base::Passed(&creator), ruleset_service, publisher));
  ruleset_service->IndexAndStoreAndPublishRulesetIfNeeded(
      unindexed_ruleset_info);
}
