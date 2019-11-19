// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_storage_client.h"

using blink::mojom::StorageType;
using content::BrowserThread;
using content::MockOriginData;
using content::MockStorageClient;

class BrowsingDataQuotaHelperTest : public testing::Test {
 public:
  typedef BrowsingDataQuotaHelper::QuotaInfo QuotaInfo;
  typedef BrowsingDataQuotaHelper::QuotaInfoArray QuotaInfoArray;

  BrowsingDataQuotaHelperTest() {}

  ~BrowsingDataQuotaHelperTest() override {}

  void SetUp() override {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    quota_manager_ = new storage::QuotaManager(
        false, dir_.GetPath(),
        base::CreateSingleThreadTaskRunner({BrowserThread::IO}).get(), nullptr,
        storage::GetQuotaSettingsFunc());
    helper_ = new BrowsingDataQuotaHelperImpl(quota_manager_.get());
  }

  void TearDown() override {
    helper_ = nullptr;
    quota_manager_ = nullptr;
    quota_info_.clear();
    content::RunAllTasksUntilIdle();
  }

 protected:
  const QuotaInfoArray& quota_info() const {
    return quota_info_;
  }

  bool fetching_completed() const {
    return fetching_completed_;
  }

  void StartFetching() {
    fetching_completed_ = false;
    helper_->StartFetching(
        base::Bind(&BrowsingDataQuotaHelperTest::FetchCompleted,
                   weak_factory_.GetWeakPtr()));
  }

  void RegisterClient(const MockOriginData* data, std::size_t data_len) {
    MockStorageClient* client =
        new MockStorageClient(quota_manager_->proxy(),
                              data,
                              storage::QuotaClient::kFileSystem,
                              data_len);
    quota_manager_->proxy()->RegisterClient(client);
    client->TouchAllOriginsAndNotify();
  }

  void SetPersistentHostQuota(const std::string& host, int64_t quota) {
    quota_ = -1;
    quota_manager_->SetPersistentHostQuota(
        host, quota,
        base::Bind(&BrowsingDataQuotaHelperTest::GotPersistentHostQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void GetPersistentHostQuota(const std::string& host) {
    quota_ = -1;
    quota_manager_->GetPersistentHostQuota(
        host,
        base::Bind(&BrowsingDataQuotaHelperTest::GotPersistentHostQuota,
                   weak_factory_.GetWeakPtr()));
  }

  void GotPersistentHostQuota(blink::mojom::QuotaStatusCode status,
                              int64_t quota) {
    EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
    quota_ = quota;
  }

  void RevokeHostQuota(const std::string& host) {
    helper_->RevokeHostQuota(host);
  }

  int64_t quota() { return quota_; }

 private:
  void FetchCompleted(const QuotaInfoArray& quota_info) {
    quota_info_ = quota_info;
    fetching_completed_ = true;
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::QuotaManager> quota_manager_;

  base::ScopedTempDir dir_;
  scoped_refptr<BrowsingDataQuotaHelper> helper_;

  bool fetching_completed_ = true;
  QuotaInfoArray quota_info_;
  int64_t quota_ = -1;
  base::WeakPtrFactory<BrowsingDataQuotaHelperTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataQuotaHelperTest);
};

TEST_F(BrowsingDataQuotaHelperTest, Empty) {
  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());
  EXPECT_TRUE(quota_info().empty());
}

TEST_F(BrowsingDataQuotaHelperTest, FetchData) {
  const MockOriginData kOrigins[] = {
      {"http://example.com/", StorageType::kTemporary, 1},
      {"https://example.com/", StorageType::kTemporary, 10},
      {"http://example.com/", StorageType::kPersistent, 100},
      {"https://example.com/", StorageType::kSyncable, 1},
      {"http://example2.com/", StorageType::kTemporary, 1000},
  };

  RegisterClient(kOrigins, base::size(kOrigins));
  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());

  std::set<QuotaInfo> expected, actual;
  actual.insert(quota_info().begin(), quota_info().end());
  expected.insert(QuotaInfo("example.com", 11, 100, 1));
  expected.insert(QuotaInfo("example2.com", 1000, 0, 0));
  EXPECT_TRUE(expected == actual);
}

TEST_F(BrowsingDataQuotaHelperTest, IgnoreExtensionsAndDevTools) {
  const MockOriginData kOrigins[] = {
      {"http://example.com/", StorageType::kTemporary, 1},
      {"https://example.com/", StorageType::kTemporary, 10},
      {"http://example.com/", StorageType::kPersistent, 100},
      {"https://example.com/", StorageType::kSyncable, 1},
      {"http://example2.com/", StorageType::kTemporary, 1000},
      {"chrome-extension://abcdefghijklmnopqrstuvwxyz/",
       StorageType::kTemporary, 10000},
      {"chrome-extension://abcdefghijklmnopqrstuvwxyz/",
       StorageType::kPersistent, 100000},
      {"devtools://abcdefghijklmnopqrstuvwxyz/", StorageType::kTemporary,
       10000},
      {"devtools://abcdefghijklmnopqrstuvwxyz/", StorageType::kPersistent,
       100000},
  };

  RegisterClient(kOrigins, base::size(kOrigins));
  StartFetching();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(fetching_completed());

  std::set<QuotaInfo> expected, actual;
  actual.insert(quota_info().begin(), quota_info().end());
  expected.insert(QuotaInfo("example.com", 11, 100, 1));
  expected.insert(QuotaInfo("example2.com", 1000, 0, 0));
  EXPECT_TRUE(expected == actual);
}

TEST_F(BrowsingDataQuotaHelperTest, RevokeHostQuota) {
  const std::string kHost1("example1.com");
  const std::string kHost2("example2.com");

  SetPersistentHostQuota(kHost1, 1);
  SetPersistentHostQuota(kHost2, 10);
  content::RunAllTasksUntilIdle();

  RevokeHostQuota(kHost1);
  content::RunAllTasksUntilIdle();

  GetPersistentHostQuota(kHost1);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, quota());

  GetPersistentHostQuota(kHost2);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(10, quota());
}
