// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/test/fake_data_type_store_service.h"

#include <memory>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "url/gurl.h"

namespace webapk {
FakeDataTypeStoreService::FakeDataTypeStoreService() = default;

FakeDataTypeStoreService::~FakeDataTypeStoreService() = default;

syncer::DataTypeStore* FakeDataTypeStoreService::GetStore() {
  // Lazily instantiate to avoid performing blocking operations in tests that
  // never use WebApks at all.
  if (!store_) {
    store_ = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
  }
  return store_.get();
}

const base::FilePath& FakeDataTypeStoreService::GetSyncDataPath() const {
  // Constructing an empty path on-the-fly doesn't work here because the method
  // must return a reference. Instead return a reference to the empty member.
  return sync_data_path_;
}

syncer::RepeatingDataTypeStoreFactory
FakeDataTypeStoreService::GetStoreFactory() {
  return syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(GetStore());
}

syncer::RepeatingDataTypeStoreFactory
FakeDataTypeStoreService::GetStoreFactoryForAccountStorage() {
  return syncer::RepeatingDataTypeStoreFactory();
}

scoped_refptr<base::SequencedTaskRunner>
FakeDataTypeStoreService::GetBackendTaskRunner() {
  return nullptr;
}

Registry FakeDataTypeStoreService::ReadRegistry() {
  Registry registry;
  base::RunLoop run_loop;

  GetStore()->ReadAllData(base::BindLambdaForTesting(
      [&](const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::DataTypeStore::RecordList> data_records) {
        DCHECK(!error);

        for (const syncer::DataTypeStore::Record& record : *data_records) {
          std::unique_ptr<WebApkProto> proto = std::make_unique<WebApkProto>();
          const bool parsed = proto->ParseFromString(record.value);
          if (!parsed) {
            DLOG(ERROR) << "WebApks LevelDB parse error: can't parse proto.";
          }

          registry.emplace(record.id, std::move(proto));
        }
        run_loop.Quit();
      }));

  run_loop.Run();
  return registry;
}

void FakeDataTypeStoreService::WriteProtos(
    const std::vector<const WebApkProto*>& protos) {
  base::RunLoop run_loop;

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      GetStore()->CreateWriteBatch();

  for (const WebApkProto* proto : protos) {
    GURL manifest_id(proto->sync_data().manifest_id());
    DCHECK(!manifest_id.is_empty());
    DCHECK(manifest_id.is_valid());

    write_batch->WriteData(GenerateAppIdFromManifestId(manifest_id),
                           proto->SerializeAsString());
  }

  GetStore()->CommitWriteBatch(
      std::move(write_batch),
      base::BindLambdaForTesting(
          [&](const std::optional<syncer::ModelError>& error) {
            DCHECK(!error);
            run_loop.Quit();
          }));

  run_loop.Run();
}

void FakeDataTypeStoreService::WriteRegistry(const Registry& registry) {
  std::vector<const WebApkProto*> protos;
  for (const Registry::value_type& kv : registry) {
    const WebApkProto* webapk = kv.second.get();
    protos.push_back(webapk);
  }

  WriteProtos(protos);
}

}  // namespace webapk
