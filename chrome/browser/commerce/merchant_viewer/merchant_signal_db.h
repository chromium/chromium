// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_SIGNAL_DB_H_
#define CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_SIGNAL_DB_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/proto/merchant_signal_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace merchant_signal_db {
class MerchantSignalContentProto;
}  // namespace merchant_signal_db

template <typename T>
class SessionProtoDB;

class MerchantSignalDB {
 public:
  explicit MerchantSignalDB(content::BrowserContext* browser_context);
  MerchantSignalDB(const MerchantSignalDB&) = delete;
  MerchantSignalDB& operator=(const MerchantSignalDB&) = delete;
  ~MerchantSignalDB();

  // Save signal for key.
  void Save(JNIEnv* env,
            const base::android::JavaParamRef<jstring>& jkey,
            const jlong jtimestamp,
            const base::android::JavaParamRef<jobject>& jcallback);

  // Load signal corresponding to key.
  void Load(JNIEnv* env,
            const base::android::JavaParamRef<jstring>& jkey,
            const base::android::JavaParamRef<jobject>& jcallback);

  // Load signal whose keys have specific prefix.
  void LoadWithPrefix(JNIEnv* env,
                      const base::android::JavaParamRef<jstring>& jprefix,
                      const base::android::JavaParamRef<jobject>& jcallback);

  // Delete entry corresponding to key.
  void Delete(JNIEnv* env,
              const base::android::JavaParamRef<jstring>& jkey,
              const base::android::JavaParamRef<jobject>& jcallback);

  // Delete all entries in the database.
  void DeleteAll(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jcallback);

 private:
  raw_ptr<SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>>
      proto_db_;
  base::WeakPtrFactory<MerchantSignalDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_SIGNAL_DB_H_
