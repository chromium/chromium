// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_ATTRIBUTE_UPDATE_DETAILS_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_ATTRIBUTE_UPDATE_DETAILS_ANDROID_H_

#include <utility>

#include "base/android/jni_string.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/EntityAttributeUpdateDetails_jni.h"

namespace jni_zero {
template <>
inline autofill::EntityAttributeUpdateDetails
FromJniType<autofill::EntityAttributeUpdateDetails>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  return autofill::EntityAttributeUpdateDetails(
      autofill::Java_EntityAttributeUpdateDetails_getAttributeName(env,
                                                                   j_object),
      autofill::Java_EntityAttributeUpdateDetails_getAttributeValue(env,
                                                                    j_object),
      autofill::Java_EntityAttributeUpdateDetails_getOldAttributeValue(
          env, j_object),
      static_cast<autofill::EntityAttributeUpdateType>(
          autofill::Java_EntityAttributeUpdateDetails_getUpdateType(env,
                                                                    j_object)));
}

template <>
inline ScopedJavaLocalRef<jobject>
ToJniType<autofill::EntityAttributeUpdateDetails>(
    JNIEnv* env,
    const autofill::EntityAttributeUpdateDetails& update_type) {
  return autofill::Java_EntityAttributeUpdateDetails_Constructor(
      env, update_type.attribute_name(), update_type.attribute_value(),
      update_type.old_attribute_value().value_or(u""),
      std::to_underlying(update_type.update_type()));
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_ATTRIBUTE_UPDATE_DETAILS_ANDROID_H_
