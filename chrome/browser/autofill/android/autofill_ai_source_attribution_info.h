// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SOURCE_ATTRIBUTION_INFO_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SOURCE_ATTRIBUTION_INFO_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

namespace autofill {

// The C++ counterpart to the Java class of the same name.
struct AutofillAiSourceAttributionInfo {
  using SourceType =
      EntityInstance::PersonalContextRecordTypePayload::Source::Type;

  AutofillAiSourceAttributionInfo(SourceType source_type,
                                  GURL url,
                                  std::u16string title);
  explicit AutofillAiSourceAttributionInfo(
      const EntityInstance::PersonalContextRecordTypePayload::Source& source);
  AutofillAiSourceAttributionInfo(const AutofillAiSourceAttributionInfo&) =
      default;
  AutofillAiSourceAttributionInfo& operator=(
      const AutofillAiSourceAttributionInfo&) = default;
  AutofillAiSourceAttributionInfo(AutofillAiSourceAttributionInfo&&) = default;
  AutofillAiSourceAttributionInfo& operator=(
      AutofillAiSourceAttributionInfo&&) = default;
  ~AutofillAiSourceAttributionInfo() = default;

  friend bool operator==(const AutofillAiSourceAttributionInfo&,
                         const AutofillAiSourceAttributionInfo&) = default;

  SourceType source_type;
  GURL url;
  // Formatted display title (e.g. email subject for Gmail, or localized date
  // for Photos). This is a derived presentation string and cannot be parsed
  // back into a Photos timestamp.
  std::u16string title;
};

}  // namespace autofill

namespace jni_zero {

template <>
autofill::AutofillAiSourceAttributionInfo
FromJniType<autofill::AutofillAiSourceAttributionInfo>(
    JNIEnv* env,
    const JavaRef<jobject>& jobj);

template <>
ScopedJavaLocalRef<jobject>
ToJniType<autofill::AutofillAiSourceAttributionInfo>(
    JNIEnv* env,
    const autofill::AutofillAiSourceAttributionInfo& source_attribution_info);

}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_AUTOFILL_AI_SOURCE_ATTRIBUTION_INFO_H_
