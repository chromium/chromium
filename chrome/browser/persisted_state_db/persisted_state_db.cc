// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/persisted_state_db.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "content/public/browser/android/browser_context_handle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/LevelDBPersistedDataStorage_jni.h"

namespace {

void OnUpdateCallback(
    const base::android::JavaRef<jobject>& joncomplete_for_testing,
    bool success) {
  if (!success)
    LOG(WARNING) << "There was an error modifying PersistedStateDB";
  // Callback for save and delete is only used in tests for synchronization.
  // Otherwise the callback is a no-op.
  if (joncomplete_for_testing)
    base::android::RunRunnableAndroid(joncomplete_for_testing);
}

void OnLoadCallback(
    const base::android::JavaRef<jobject>& jcallback,
    bool success,
    std::vector<SessionProtoDB<
        persisted_state_db::PersistedStateContentProto>::KeyAndValue> data) {
  if (!success)
    LOG(WARNING) << "There was an error loading from PersistedStateDB";
  base::android::RunObjectCallbackAndroid(
      jcallback, base::android::ToJavaByteArray(
                     base::android::AttachCurrentThread(),
                     data.empty() ? "" : data[0].second.content_data()));
}
}  // namespace

PersistedStateDB::PersistedStateDB(content::BrowserContext* browser_context)
    : proto_db_(
          SessionProtoDBFactory<
              persisted_state_db::PersistedStateContentProto>::GetInstance()
              ->GetForProfile(browser_context)) {}

PersistedStateDB::~PersistedStateDB() = default;

void PersistedStateDB::Save(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jbyteArray>& jbyte_array,
    const base::android::JavaRef<jobject>& joncomplete_for_testing) {
  const std::string& key = base::android::ConvertJavaStringToUTF8(env, jkey);
  std::string data;
  base::android::JavaByteArrayToString(env, jbyte_array, &data);
  persisted_state_db::PersistedStateContentProto proto;
  proto.set_key(key);
  proto.set_content_data(data);
  proto_db_->InsertContent(
      key, proto,
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(
                         joncomplete_for_testing)));
}

void PersistedStateDB::Load(JNIEnv* env,
                            const base::android::JavaParamRef<jstring>& jkey,
                            const base::android::JavaRef<jobject>& jcallback) {
  proto_db_->LoadContentWithPrefix(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnLoadCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void PersistedStateDB::Delete(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaRef<jobject>& joncomplete_for_testing) {
  proto_db_->DeleteContentWithPrefix(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(
                         joncomplete_for_testing)));
}

void PersistedStateDB::PerformMaintenance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& jkeys_to_keep,
    const base::android::JavaParamRef<jstring>& jkey_substring_to_match,
    const base::android::JavaRef<jobject>& joncomplete_for_testing) {
  std::vector<std::string> keys_to_keep;
  base::android::AppendJavaStringArrayToStringVector(env, jkeys_to_keep,
                                                     &keys_to_keep);
  proto_db_->PerformMaintenance(
      keys_to_keep,
      base::android::ConvertJavaStringToUTF8(jkey_substring_to_match),
      base::BindOnce(&OnUpdateCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(
                         joncomplete_for_testing)));
}

void PersistedStateDB::Destroy(JNIEnv* env) {
  proto_db_->Destroy();
}

static void JNI_LevelDBPersistedDataStorage_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Java_LevelDBPersistedDataStorage_setNativePtr(
      env, obj,
      reinterpret_cast<intptr_t>(new PersistedStateDB(
          content::BrowserContextFromJavaHandle(jprofile))));
}
