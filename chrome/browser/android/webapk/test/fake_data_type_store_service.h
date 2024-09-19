// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_DATA_TYPE_STORE_SERVICE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_DATA_TYPE_STORE_SERVICE_H_

#include <memory>

#include "chrome/browser/android/webapk/webapk_database.h"
#include "components/sync/model/data_type_store_service.h"

namespace webapk {

class WebApkProto;

// syncer::DataTypeStoreService subclass with direct read/write access to the
// registry.
class FakeDataTypeStoreService : public syncer::DataTypeStoreService {
 public:
  FakeDataTypeStoreService();
  FakeDataTypeStoreService(const FakeDataTypeStoreService&) = delete;
  FakeDataTypeStoreService& operator=(const FakeDataTypeStoreService&) = delete;
  ~FakeDataTypeStoreService() override;

  syncer::DataTypeStore* GetStore();

  const base::FilePath& GetSyncDataPath() const override;
  syncer::RepeatingDataTypeStoreFactory GetStoreFactory() override;
  syncer::RepeatingDataTypeStoreFactory GetStoreFactoryForAccountStorage()
      override;
  scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() override;

  Registry ReadRegistry();

  void WriteProtos(const std::vector<const WebApkProto*>& protos);
  void WriteRegistry(const Registry& registry);

 private:
  std::unique_ptr<syncer::DataTypeStore> store_;
  base::FilePath sync_data_path_;
};
}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_DATA_TYPE_STORE_SERVICE_H_
