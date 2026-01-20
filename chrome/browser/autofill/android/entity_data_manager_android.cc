// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_data_manager_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "chrome/browser/autofill/android/entity_instance_android.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/EntityDataManager_jni.h"

namespace autofill {

EntityDataManagerAndroid::EntityDataManagerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    EntityDataManager* entity_data_manager)
    : weak_java_obj_(env, obj),
      entity_data_manager_(CHECK_DEREF(entity_data_manager)) {}

EntityDataManagerAndroid::~EntityDataManagerAndroid() = default;

static jlong JNI_EntityDataManager_Init(JNIEnv* env,
                                        const jni_zero::JavaRef<jobject>& obj,
                                        Profile* profile) {
  CHECK(profile);
  EntityDataManagerAndroid* entity_data_manager_android =
      new EntityDataManagerAndroid(
          env, obj, AutofillEntityDataManagerFactory::GetForProfile(profile));
  return reinterpret_cast<intptr_t>(entity_data_manager_android);
}

void EntityDataManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

jni_zero::ScopedJavaLocalRef<jobject>
EntityDataManagerAndroid::GetEntityInstance(const std::string& guid) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::optional_ref<const EntityInstance> entity =
      entity_data_manager_->GetEntityInstance(EntityInstance::EntityId(guid));
  if (!entity) {
    return nullptr;
  }

  return EntityInstanceAndroid::Create(env, EntityInstanceAndroid(*entity));
}

void EntityDataManagerAndroid::RemoveEntityInstance(JNIEnv* env,
                                                    const std::string& guid) {
  entity_data_manager().RemoveEntityInstance(EntityInstance::EntityId(guid));
}

void EntityDataManagerAndroid::AddOrUpdateEntityInstance(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jEntity) {
  EntityInstanceAndroid entity_android =
      EntityInstanceAndroid::FromJavaEntityInstance(env, jEntity);

  entity_data_manager().AddOrUpdateEntityInstance(
      entity_android.ToEntityInstance(entity_data_manager_->GetEntityInstance(
          EntityInstance::EntityId(entity_android.guid))));
}

}  // namespace autofill

DEFINE_JNI(EntityDataManager)
