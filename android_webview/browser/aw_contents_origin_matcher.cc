// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_origin_matcher.h"


#include "components/js_injection/common/origin_matcher.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentsOriginMatcher_jni.h"

namespace android_webview {

AwContentsOriginMatcher::AwContentsOriginMatcher()
    : origin_matcher_(std::make_unique<js_injection::OriginMatcher>()) {}

AwContentsOriginMatcher::~AwContentsOriginMatcher() = default;

bool AwContentsOriginMatcher::MatchesOrigin(const url::Origin& origin) {
  base::AutoLock auto_lock(lock_);
  return origin_matcher_->Matches(origin);
}

std::vector<std::string> AwContentsOriginMatcher::UpdateRuleList(
    const std::vector<std::string>& rules) {
  std::vector<std::string> bad_rules;
  std::unique_ptr<js_injection::OriginMatcher> new_matcher =
      std::make_unique<js_injection::OriginMatcher>();
  for (const std::string& rule : rules) {
    if (!new_matcher->AddRuleFromString(rule))
      bad_rules.push_back(rule);
  }

  if (!bad_rules.empty())
    return bad_rules;

  {
    // Swap the pointer while locked, then release the lock before running the
    // destructor on the old (swapped-out) pointer.
    base::AutoLock auto_lock(lock_);
    origin_matcher_.swap(new_matcher);
  }
  return bad_rules;
}

jboolean AwContentsOriginMatcher::MatchesOrigin(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jorigin) {
  const url::Origin origin = url::Origin::Create(
      GURL(base::android::ConvertJavaStringToUTF8(env, jorigin)));
  return MatchesOrigin(origin);
}

base::android::ScopedJavaLocalRef<jobjectArray>
AwContentsOriginMatcher::UpdateRuleList(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& jrules) {
  std::vector<std::string> rules;
  base::android::AppendJavaStringArrayToStringVector(env, jrules, &rules);
  std::vector<std::string> bad_rules = UpdateRuleList(rules);
  return base::android::ToJavaArrayOfStrings(env, bad_rules);
}

void AwContentsOriginMatcher::Destroy(JNIEnv* env) {
  Release();
}

static jlong JNI_AwContentsOriginMatcher_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  AwContentsOriginMatcher* matcher = new AwContentsOriginMatcher();
  // We are handing over the raw pointer java so we manually increment the
  // reference count instead of using ref_pointer directly because leaving this
  // scope would decrement it.
  matcher->AddRef();
  return reinterpret_cast<intptr_t>(matcher);
}

}  // namespace android_webview
