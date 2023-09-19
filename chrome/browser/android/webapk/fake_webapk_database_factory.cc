// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/fake_webapk_database_factory.h"

#include <memory>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "url/gurl.h"

namespace webapk {
FakeWebApkDatabaseFactory::FakeWebApkDatabaseFactory() = default;

FakeWebApkDatabaseFactory::~FakeWebApkDatabaseFactory() = default;

syncer::ModelTypeStore* FakeWebApkDatabaseFactory::GetStore() {
  // Lazily instantiate to avoid performing blocking operations in tests that
  // never use WebApks at all.
  if (!store_) {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
  }
  return store_.get();
}

syncer::OnceModelTypeStoreFactory FakeWebApkDatabaseFactory::GetStoreFactory() {
  return syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(GetStore());
}

Registry FakeWebApkDatabaseFactory::ReadRegistry() {
  Registry registry;
  base::RunLoop run_loop;

  GetStore()->ReadAllData(base::BindLambdaForTesting(
      [&](const absl::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::ModelTypeStore::RecordList> data_records) {
        DCHECK(!error);

        for (const syncer::ModelTypeStore::Record& record : *data_records) {
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

void FakeWebApkDatabaseFactory::WriteProtos(
    const std::vector<const WebApkProto*>& protos) {
  base::RunLoop run_loop;

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
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
          [&](const absl::optional<syncer::ModelError>& error) {
            DCHECK(!error);
            run_loop.Quit();
          }));

  run_loop.Run();
}

void FakeWebApkDatabaseFactory::WriteRegistry(const Registry& registry) {
  std::vector<const WebApkProto*> protos;
  for (const Registry::value_type& kv : registry) {
    const WebApkProto* webapk = kv.second.get();
    protos.push_back(webapk);
  }

  WriteProtos(protos);
}

}  // namespace webapk
