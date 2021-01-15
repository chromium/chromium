// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::StorageType;

class BrowsingDataQuotaHelperTest : public testing::Test {
 public:
  typedef BrowsingDataQuotaHelper::QuotaInfo QuotaInfo;
  typedef BrowsingDataQuotaHelper::QuotaInfoArray QuotaInfoArray;

  BrowsingDataQuotaHelperTest() = default;

  ~BrowsingDataQuotaHelperTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::QuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        content::GetIOThreadTaskRunner({}).get(),
        /*quota_change_callback=*/base::DoNothing(),
        /*special_storage_policy=*/nullptr, storage::GetQuotaSettingsFunc());
    helper_ = base::WrapRefCounted(
        new BrowsingDataQuotaHelperImpl(quota_manager_.get()));
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
        base::BindOnce(&BrowsingDataQuotaHelperTest::FetchCompleted,
                       weak_factory_.GetWeakPtr()));
  }

  void RegisterClient(base::span<const storage::MockOriginData> origin_data) {
    auto mock_quota_client = std::make_unique<storage::MockQuotaClient>(
        quota_manager_->proxy(), origin_data,
        storage::QuotaClientType::kFileSystem);
    storage::MockQuotaClient* mock_quota_client_ptr = mock_quota_client.get();

    mojo::PendingRemote<storage::mojom::QuotaClient> quota_client;
    mojo::MakeSelfOwnedReceiver(std::move(mock_quota_client),
                                quota_client.InitWithNewPipeAndPassReceiver());
    quota_manager_->proxy()->RegisterClient(
        std::move(quota_client), storage::QuotaClientType::kFileSystem,
        {blink::mojom::StorageType::kTemporary,
         blink::mojom::StorageType::kPersistent,
         blink::mojom::StorageType::kSyncable});
    mock_quota_client_ptr->TouchAllOriginsAndNotify();
  }

  void SetPersistentHostQuota(const std::string& host, int64_t quota) {
    quota_ = -1;
    quota_manager_->SetPersistentHostQuota(
        host, quota,
        base::BindOnce(&BrowsingDataQuotaHelperTest::GotPersistentHostQuota,
                       weak_factory_.GetWeakPtr()));
  }

  void GetPersistentHostQuota(const std::string& host) {
    quota_ = -1;
    quota_manager_->GetPersistentHostQuota(
        host,
        base::BindOnce(&BrowsingDataQuotaHelperTest::GotPersistentHostQuota,
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

  base::ScopedTempDir temp_dir_;

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::QuotaManager> quota_manager_;

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
  static const storage::MockOriginData kOrigins[] = {
      {"http://example.com/", StorageType::kTemporary, 1},
      {"https://example.com/", StorageType::kTemporary, 10},
      {"http://example.com/", StorageType::kPersistent, 100},
      {"https://example.com/", StorageType::kSyncable, 1},
      {"http://example2.com/", StorageType::kTemporary, 1000},
  };

  RegisterClient(kOrigins);
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
  static const storage::MockOriginData kOrigins[] = {
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

  RegisterClient(kOrigins);
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
