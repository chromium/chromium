// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_INSTANCE_WITH_LABELS_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_INSTANCE_WITH_LABELS_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// The C++ counterpart to the Java class of the same name.
struct EntityInstanceWithLabels {
  EntityInstanceWithLabels(std::string guid,
                           std::u16string entity_instance_label,
                           std::u16string entity_instance_sublabel,
                           bool stored_in_wallet);
  ~EntityInstanceWithLabels();
  EntityInstanceWithLabels(const EntityInstanceWithLabels&);
  EntityInstanceWithLabels& operator=(const EntityInstanceWithLabels&);
  EntityInstanceWithLabels(EntityInstanceWithLabels&&);
  EntityInstanceWithLabels& operator=(EntityInstanceWithLabels&&);

  std::string guid;
  std::u16string entity_instance_label;
  std::u16string entity_instance_sublabel;
  bool stored_in_wallet;
};

}  // namespace autofill

namespace jni_zero {

template <>
autofill::EntityInstanceWithLabels
FromJniType<autofill::EntityInstanceWithLabels>(JNIEnv* env,
                                                const JavaRef<jobject>& jobj);

template <>
ScopedJavaLocalRef<jobject> ToJniType<autofill::EntityInstanceWithLabels>(
    JNIEnv* env,
    const autofill::EntityInstanceWithLabels& native_instance);

}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_INSTANCE_WITH_LABELS_H_
