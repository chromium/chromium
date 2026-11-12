// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/autofill_ai_source_attribution_info.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "components/autofill/core/browser/ui/autofill_ai/entity_source_attribution_utils.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/main_autofill_jni_headers/AutofillAiSourceAttributionInfo_jni.h"

namespace autofill {

AutofillAiSourceAttributionInfo::AutofillAiSourceAttributionInfo(
    SourceType source_type,
    GURL url,
    std::u16string title)
    : source_type(source_type), url(std::move(url)), title(std::move(title)) {}

AutofillAiSourceAttributionInfo::AutofillAiSourceAttributionInfo(
    const EntityInstance::PersonalContextRecordTypePayload::Source& source)
    : source_type(source.type()),
      url(source.url),
      title(GetSourceDisplayTitle(source)) {}

}  // namespace autofill

namespace jni_zero {

using autofill::AutofillAiSourceAttributionInfo;

template <>
AutofillAiSourceAttributionInfo FromJniType<AutofillAiSourceAttributionInfo>(
    JNIEnv* env,
    const JavaRef<jobject>& jobj) {
  int raw_source_type =
      autofill::Java_AutofillAiSourceAttributionInfo_getSourceType(env, jobj);
  CHECK_GE(raw_source_type, 0);
  CHECK_LE(raw_source_type,
           std::to_underlying(
               AutofillAiSourceAttributionInfo::SourceType::kMaxValue));
  return AutofillAiSourceAttributionInfo(
      static_cast<AutofillAiSourceAttributionInfo::SourceType>(raw_source_type),
      autofill::Java_AutofillAiSourceAttributionInfo_getUrl(env, jobj),
      autofill::Java_AutofillAiSourceAttributionInfo_getTitle(env, jobj));
}

template <>
ScopedJavaLocalRef<jobject> ToJniType<AutofillAiSourceAttributionInfo>(
    JNIEnv* env,
    const AutofillAiSourceAttributionInfo& source_attribution_info) {
  return autofill::Java_AutofillAiSourceAttributionInfo_Constructor(
      env, std::to_underlying(source_attribution_info.source_type),
      source_attribution_info.url, source_attribution_info.title);
}

}  // namespace jni_zero
