// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/important_sites_usage_counter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

using ImportantDomainInfo = ImportantSitesUtil::ImportantDomainInfo;
using content::DOMStorageContext;
using storage::QuotaManager;

class ImportantSitesUsageCounterTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override {
    // Release the quota manager and wait for the database to be closed.
    quota_manager_.reset();
    content::RunAllTasksUntilIdle();
  }

  TestingProfile* profile() { return &profile_; }

  QuotaManager* CreateQuotaManager() {
    quota_manager_ = base::MakeRefCounted<QuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        content::GetIOThreadTaskRunner({}).get(),
        /*special_storage_policy=*/nullptr, storage::GetQuotaSettingsFunc());
    return quota_manager_.get();
  }

  void RegisterClient(base::span<const storage::MockOriginData> origin_data) {
    auto client = base::MakeRefCounted<storage::MockQuotaClient>(
        quota_manager_->proxy(), origin_data,
        storage::QuotaClientType::kFileSystem);
    quota_manager_->proxy()->RegisterClient(
        client, storage::QuotaClientType::kFileSystem,
        {blink::mojom::StorageType::kTemporary,
         blink::mojom::StorageType::kPersistent});
    client->TouchAllOriginsAndNotify();
  }

  void CreateLocalStorage(
      base::Time creation_time,
      int length,
      const base::FilePath::StringPieceType& storage_origin) {
    // Note: This test depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path =
        profile()->GetPath().AppendASCII("Local Storage");
    base::CreateDirectory(storage_path);

    std::string data(length, ' ');
    // Write file to local storage.
    base::FilePath file_path = storage_path.Append(storage_origin);
    base::WriteFile(file_path, data.c_str(), length);
    base::TouchFile(file_path, creation_time, creation_time);
  }

  void FetchCompleted(std::vector<ImportantDomainInfo> domain_info) {
    domain_info_ = std::move(domain_info);
    run_loop_->Quit();
  }

  void WaitForResult() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  const std::vector<ImportantDomainInfo>& domain_info() { return domain_info_; }

 private:
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  scoped_refptr<QuotaManager> quota_manager_;
  std::vector<ImportantDomainInfo> domain_info_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(ImportantSitesUsageCounterTest, PopulateUsage) {
  std::vector<ImportantDomainInfo> important_sites;
  ImportantDomainInfo i1;
  i1.registerable_domain = "example.com";
  ImportantDomainInfo i2;
  i2.registerable_domain = "somethingelse.com";
  important_sites.push_back(std::move(i1));
  important_sites.push_back(std::move(i2));

  static const storage::MockOriginData kOrigins[] = {
      {"http://example.com/", blink::mojom::StorageType::kTemporary, 1},
      {"https://example.com/", blink::mojom::StorageType::kTemporary, 2},
      {"https://maps.example.com/", blink::mojom::StorageType::kTemporary, 4},
      {"http://google.com/", blink::mojom::StorageType::kPersistent, 8},
  };

  QuotaManager* quota_manager = CreateQuotaManager();
  RegisterClient(kOrigins);

  base::Time now = base::Time::Now();
  CreateLocalStorage(now, 16,
                     FILE_PATH_LITERAL("https_example.com_443.localstorage"));
  CreateLocalStorage(now, 32,
                     FILE_PATH_LITERAL("https_bing.com_443.localstorage"));
  DOMStorageContext* dom_storage_context =
      content::BrowserContext::GetDefaultStoragePartition(profile())
          ->GetDOMStorageContext();

  ImportantSitesUsageCounter::GetUsage(
      std::move(important_sites), quota_manager, dom_storage_context,
      base::BindOnce(&ImportantSitesUsageCounterTest::FetchCompleted,
                     base::Unretained(this)));
  WaitForResult();

  EXPECT_EQ(2U, domain_info().size());
  // The first important site is example.com. It uses 1B quota storage for
  // http://example.com/, 2B for https://example.com and 4B for
  // https://maps.example.com. On top of that it uses 16B local storage.
  EXPECT_EQ("example.com", domain_info()[0].registerable_domain);
  EXPECT_EQ(1 + 2 + 4 + 16, domain_info()[0].usage);
  // The second important site is somethingelse.com but it doesn't use any
  // quota. We still expect it to be returned and not dropped.
  EXPECT_EQ("somethingelse.com", domain_info()[1].registerable_domain);
  EXPECT_EQ(0, domain_info()[1].usage);
}
