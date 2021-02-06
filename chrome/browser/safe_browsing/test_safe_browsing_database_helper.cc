// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "components/safe_browsing/core/db/v4_database.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/db/v4_test_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"

namespace {

// UI manager that never actually shows any interstitials, but emulates as if
// the user chose to proceed through them.
class FakeSafeBrowsingUIManager
    : public safe_browsing::TestSafeBrowsingUIManager {
 public:
  FakeSafeBrowsingUIManager() {}

 protected:
  ~FakeSafeBrowsingUIManager() override {}

  void DisplayBlockingPage(const UnsafeResource& resource) override {
    resource.callback_thread->PostTask(
        FROM_HERE, base::BindOnce(resource.callback, true /* proceed */,
                                  true /* showed_interstitial */));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingUIManager);
};

}  // namespace

// This class automatically inserts lists into the store map when initializing
// the test database.
class InsertingDatabaseFactory : public safe_browsing::TestV4DatabaseFactory {
 public:
  explicit InsertingDatabaseFactory(
      safe_browsing::TestV4StoreFactory* store_factory,
      const std::vector<safe_browsing::ListIdentifier>& lists_to_insert)
      : lists_to_insert_(lists_to_insert), store_factory_(store_factory) {}

  std::unique_ptr<safe_browsing::V4Database> Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<safe_browsing::StoreMap> store_map) override {
    const base::FilePath base_store_path(FILE_PATH_LITERAL("UrlDb.store"));
    for (const auto& id : lists_to_insert_) {
      if (!base::Contains(*store_map, id)) {
        const base::FilePath store_path =
            base_store_path.InsertBeforeExtensionASCII(base::StringPrintf(
                " (%d)", base::GetUniquePathNumber(base_store_path)));
        (*store_map)[id] =
            store_factory_->CreateV4Store(db_task_runner, store_path);
      }
    }

    for (const auto& it : *store_map)
      lists_.push_back(it.first);
    return safe_browsing::TestV4DatabaseFactory::Create(db_task_runner,
                                                        std::move(store_map));
  }

  const std::vector<safe_browsing::ListIdentifier> lists() { return lists_; }

 private:
  std::vector<safe_browsing::ListIdentifier> lists_to_insert_;
  std::vector<safe_browsing::ListIdentifier> lists_;
  safe_browsing::TestV4StoreFactory* store_factory_;
};

TestSafeBrowsingDatabaseHelper::TestSafeBrowsingDatabaseHelper()
    : TestSafeBrowsingDatabaseHelper(
          std::make_unique<
              safe_browsing::TestV4GetHashProtocolManagerFactory>(),
          std::vector<safe_browsing::ListIdentifier>()) {}

TestSafeBrowsingDatabaseHelper::TestSafeBrowsingDatabaseHelper(
    std::unique_ptr<safe_browsing::TestV4GetHashProtocolManagerFactory>
        v4_get_hash_factory,
    std::vector<safe_browsing::ListIdentifier> lists_to_insert)
    : v4_get_hash_factory_(v4_get_hash_factory.get()) {
  sb_factory_ =
      std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
  sb_factory_->SetTestUIManager(new FakeSafeBrowsingUIManager());
  sb_factory_->UseV4LocalDatabaseManager();
  safe_browsing::SafeBrowsingService::RegisterFactory(sb_factory_.get());

  auto store_factory = std::make_unique<safe_browsing::TestV4StoreFactory>();
  auto v4_db_factory = std::make_unique<InsertingDatabaseFactory>(
      store_factory.get(), lists_to_insert);

  v4_db_factory_ = v4_db_factory.get();

  safe_browsing::V4Database::RegisterStoreFactoryForTest(
      std::move(store_factory));
  safe_browsing::V4Database::RegisterDatabaseFactoryForTest(
      std::move(v4_db_factory));

  if (v4_get_hash_factory) {
    safe_browsing::V4GetHashProtocolManager::RegisterFactory(
        std::move(v4_get_hash_factory));
  }
}

TestSafeBrowsingDatabaseHelper::~TestSafeBrowsingDatabaseHelper() {
  safe_browsing::V4GetHashProtocolManager::RegisterFactory(nullptr);
  safe_browsing::V4Database::RegisterDatabaseFactoryForTest(nullptr);
  safe_browsing::V4Database::RegisterStoreFactoryForTest(nullptr);
  safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
}

void TestSafeBrowsingDatabaseHelper::AddFullHashToDbAndFullHashCache(
    const GURL& bad_url,
    const safe_browsing::ListIdentifier& list_id,
    const safe_browsing::ThreatMetadata& threat_metadata) {
  // Should only be called if we are mocking the v4 hash factory.
  DCHECK(v4_get_hash_factory_);

  LocallyMarkPrefixAsBad(bad_url, list_id);

  safe_browsing::FullHashInfo full_hash_info =
      GetFullHashInfoWithMetadata(bad_url, list_id, threat_metadata);
  v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
}

void TestSafeBrowsingDatabaseHelper::LocallyMarkPrefixAsBad(
    const GURL& url,
    const safe_browsing::ListIdentifier& list_id) {
  safe_browsing::FullHash full_hash =
      safe_browsing::V4ProtocolManagerUtil::GetFullHash(url);
  v4_db_factory_->MarkPrefixAsBad(list_id, full_hash);
}

bool TestSafeBrowsingDatabaseHelper::HasListSynced(
    const safe_browsing::ListIdentifier& list_id) {
  return base::Contains(v4_db_factory_->lists(), list_id);
}
