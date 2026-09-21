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
#include "third_party/jni_zero/default_conversions.h"

// Must come after headers that provide symbols used by @JniType.
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
    jni_zero::RunRunnable(joncomplete_for_testing);
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
    const std::string& key,
    const jni_zero::JavaRef<jbyteArray>& byte_array,
    const jni_zero::JavaRef<jobject>& oncomplete_for_testing) {
  std::string data;
  base::android::JavaByteArrayToString(env, byte_array, &data);
  persisted_state_db::PersistedStateContentProto proto;
  proto.set_key(key);
  proto.set_content_data(data);
  proto_db_->InsertContent(
      key, proto,
      base::BindOnce(
          &OnUpdateCallback,
          base::android::ScopedJavaGlobalRef<jobject>(oncomplete_for_testing)));
}

void PersistedStateDB::Load(const std::string& key,
                            const jni_zero::JavaRef<jobject>& callback) {
  proto_db_->LoadContentWithPrefix(
      key,
      base::BindOnce(&OnLoadCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

void PersistedStateDB::Delete(
    const std::string& key,
    const jni_zero::JavaRef<jobject>& oncomplete_for_testing) {
  proto_db_->DeleteContentWithPrefix(
      key, base::BindOnce(&OnUpdateCallback,
                          base::android::ScopedJavaGlobalRef<jobject>(
                              oncomplete_for_testing)));
}

void PersistedStateDB::PerformMaintenance(
    const std::vector<std::string>& keys_to_keep,
    const std::string& key_substring_to_match,
    const jni_zero::JavaRef<jobject>& oncomplete_for_testing) {
  proto_db_->PerformMaintenance(
      keys_to_keep, key_substring_to_match,
      base::BindOnce(
          &OnUpdateCallback,
          base::android::ScopedJavaGlobalRef<jobject>(oncomplete_for_testing)));
}

void PersistedStateDB::Destroy() {
  proto_db_->Destroy();
  delete this;
}

static void JNI_LevelDBPersistedDataStorage_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& jprofile) {
  Java_LevelDBPersistedDataStorage_setNativePtr(
      env, obj,
      reinterpret_cast<intptr_t>(new PersistedStateDB(
          content::BrowserContextFromJavaHandle(jprofile))));
}

DEFINE_JNI(LevelDBPersistedDataStorage)
