// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace content {
class BrowserContext;
}  // namespace content

template <typename T>
class SessionProtoDB;

// PersistedStateDB is leveldb backend store for NonCriticalPersistedTabData.
// NonCriticalPersistedTabData is an extension of TabState where data for
// new features which are not critical to the core functionality of the app
// are acquired and persisted across restarts. The intended key format is
// <NonCriticalPersistedTabData id>_<Tab id>

// NonCriticalPersistedTabData is stored in key/value pairs.
class PersistedStateDB {
 public:
  explicit PersistedStateDB(content::BrowserContext* browser_context);
  PersistedStateDB(const PersistedStateDB&) = delete;
  PersistedStateDB& operator=(const PersistedStateDB&) = delete;
  ~PersistedStateDB();

  // Save byte array for key.
  void Save(JNIEnv* env,
            const base::android::JavaParamRef<jstring>& jkey,
            const base::android::JavaParamRef<jbyteArray>& byte_array,
            const base::android::JavaRef<jobject>& jcallback);

  // Load byte array corresponding to key.
  void Load(JNIEnv* env,
            const base::android::JavaParamRef<jstring>& jkey,
            const base::android::JavaRef<jobject>& jcallback);

  // Delete entry corresponding to key.
  void Delete(JNIEnv* env,
              const base::android::JavaParamRef<jstring>& jkey,
              const base::android::JavaRef<jobject>& jcallback);

  // Delete entries which have keys which match jsubstring_to_match
  // except for those in jkeys_to_keep.
  void PerformMaintenance(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jkeys_to_keep,
      const base::android::JavaParamRef<jstring>& jsubstring_to_match,
      const base::android::JavaRef<jobject>& joncomplete_for_testing);

  // Destroy PersistedStateDB object.
  void Destroy(JNIEnv* env);

 private:
  raw_ptr<SessionProtoDB<persisted_state_db::PersistedStateContentProto>>
      proto_db_;

  base::WeakPtrFactory<PersistedStateDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_
