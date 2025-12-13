// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

// Android wrapper of the EntityDataManager which provides access from the
// Java layer.
class EntityDataManagerAndroid {
 public:
  EntityDataManagerAndroid(JNIEnv* env,
                           const jni_zero::JavaRef<jobject>& obj,
                           EntityDataManager* entity_data_manager);

  EntityDataManagerAndroid(const EntityDataManagerAndroid&) = delete;
  EntityDataManagerAndroid& operator=(const EntityDataManagerAndroid&) = delete;

  // Trigger the destruction of the C++ object from Java.
  void Destroy(JNIEnv* env);

  // Removes the entity instance represented by `guid`.
  void RemoveEntityInstance(JNIEnv* env, const std::string& guid);

 private:
  ~EntityDataManagerAndroid();

  EntityDataManager& entity_data_manager() {
    return entity_data_manager_.get();
  }

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the EntityDataManager.
  raw_ref<EntityDataManager> entity_data_manager_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_
