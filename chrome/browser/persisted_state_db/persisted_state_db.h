// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_

#include <queue>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace content {
class BrowserContext;
}  // namespace content

template <typename T>
class ProfileProtoDB;

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

  // Destroy PersistedStateDB object.
  void Destroy(JNIEnv* env);

 private:
  ProfileProtoDB<persisted_state_db::PersistedStateContentProto>* proto_db_;

  base::WeakPtrFactory<PersistedStateDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_H_
