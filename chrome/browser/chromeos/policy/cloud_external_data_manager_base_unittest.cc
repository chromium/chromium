// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "crypto/sha2.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

// A string policy.
const char kStringPolicy[] = "StringPolicy";
// A policy that may reference up to 10 bytes of external data.
const char k10BytePolicy[] = "10BytePolicy";
// A policy that may reference up to 20 bytes of external data.
const char k20BytePolicy[] = "20BytePolicy";
// A nonexistent policy.
const char kUnknownPolicy[] = "UnknownPolicy";

const char k10BytePolicyURL[] = "http://localhost/10_bytes";
const char k20BytePolicyURL[] = "http://localhost/20_bytes";

const char k10ByteData[] = "10 bytes..";
const char k20ByteData[] = "20 bytes............";

const PolicyDetails kPolicyDetails[] = {
//  is_deprecated  is_device_policy  id    max_external_data_size
  { false,         false,             1,                        0 },
  { false,         false,             2,                       10 },
  { false,         false,             3,                       20 },
};

const char kCacheKey[] = "data";

}  // namespace

class CloudExternalDataManagerBaseTest : public testing::Test {
 protected:
  CloudExternalDataManagerBaseTest();

  void SetUp() override;
  void TearDown() override;

  void SetUpExternalDataManager();

  std::unique_ptr<base::DictionaryValue> ConstructMetadata(
      const std::string& url,
      const std::string& hash);
  void SetExternalDataReference(
      const std::string& policy,
      std::unique_ptr<base::DictionaryValue> metadata);

  ExternalDataFetcher::FetchCallback ConstructFetchCallback(int id);
  void ResetCallbackData();

  void OnFetchDone(int id,
                   std::unique_ptr<std::string> data,
                   const base::FilePath& file_path);

  void FetchAll();

  void SetFakeResponse(const std::string& url,
                       const std::string& repsonse_data,
                       net::HttpStatusCode response_code);

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ResourceCache> resource_cache_;
  MockCloudPolicyStore cloud_policy_store_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<CloudExternalDataManagerBase> external_data_manager_;

  std::map<int, std::unique_ptr<std::string>> callback_data_;
  PolicyDetailsMap policy_details_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataManagerBaseTest);
};

CloudExternalDataManagerBaseTest::CloudExternalDataManagerBaseTest() {
}

void CloudExternalDataManagerBaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  resource_cache_.reset(new ResourceCache(
      temp_dir_.GetPath(), task_environment_.GetMainThreadTaskRunner(),
      /* max_cache_size */ base::nullopt));
  SetUpExternalDataManager();

  // Set |kStringPolicy| to a string value.
  cloud_policy_store_.policy_map_.Set(
      kStringPolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(std::string()),
      nullptr);
  // Make |k10BytePolicy| reference 10 bytes of external data.
  SetExternalDataReference(
      k10BytePolicy,
      ConstructMetadata(k10BytePolicyURL,
                        crypto::SHA256HashString(k10ByteData)));
  // Make |k20BytePolicy| reference 20 bytes of external data.
  SetExternalDataReference(
      k20BytePolicy,
      ConstructMetadata(k20BytePolicyURL,
                        crypto::SHA256HashString(k20ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();

  url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);

  policy_details_.SetDetails(kStringPolicy, &kPolicyDetails[0]);
  policy_details_.SetDetails(k10BytePolicy, &kPolicyDetails[1]);
  policy_details_.SetDetails(k20BytePolicy, &kPolicyDetails[2]);
}

void CloudExternalDataManagerBaseTest::TearDown() {
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  ResetCallbackData();
}

void CloudExternalDataManagerBaseTest::SetUpExternalDataManager() {
  external_data_manager_ = std::make_unique<CloudExternalDataManagerBase>(
      policy_details_.GetCallback(),
      task_environment_.GetMainThreadTaskRunner());
  external_data_manager_->SetExternalDataStore(
      std::make_unique<CloudExternalDataStore>(
          kCacheKey, task_environment_.GetMainThreadTaskRunner(),
          resource_cache_.get()));
  external_data_manager_->SetPolicyStore(&cloud_policy_store_);
}

std::unique_ptr<base::DictionaryValue>
CloudExternalDataManagerBaseTest::ConstructMetadata(const std::string& url,
                                                    const std::string& hash) {
  std::unique_ptr<base::DictionaryValue> metadata(new base::DictionaryValue);
  metadata->SetKey("url", base::Value(url));
  metadata->SetKey("hash",
                   base::Value(base::HexEncode(hash.c_str(), hash.size())));
  return metadata;
}

void CloudExternalDataManagerBaseTest::SetExternalDataReference(
    const std::string& policy,
    std::unique_ptr<base::DictionaryValue> metadata) {
  cloud_policy_store_.policy_map_.Set(
      policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      std::move(metadata),
      std::make_unique<ExternalDataFetcher>(
          external_data_manager_->weak_factory_.GetWeakPtr(), policy));
}

ExternalDataFetcher::FetchCallback
CloudExternalDataManagerBaseTest::ConstructFetchCallback(int id) {
  return base::Bind(&CloudExternalDataManagerBaseTest::OnFetchDone,
                    base::Unretained(this),
                    id);
}

void CloudExternalDataManagerBaseTest::ResetCallbackData() {
  callback_data_.clear();
}

void CloudExternalDataManagerBaseTest::OnFetchDone(
    int id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  callback_data_[id] = std::move(data);
}

void CloudExternalDataManagerBaseTest::FetchAll() {
  external_data_manager_->FetchAll();
}

void CloudExternalDataManagerBaseTest::SetFakeResponse(
    const std::string& url,
    const std::string& response_data,
    net::HttpStatusCode response_code) {
  test_url_loader_factory_.AddResponse(url, response_data, response_code);
}

// Verifies that when no valid external data reference has been set for a
// policy, the attempt to retrieve the external data fails immediately.
TEST_F(CloudExternalDataManagerBaseTest, FailToFetchInvalid) {
  external_data_manager_->Connect(url_loader_factory_);

  // Attempt to retrieve external data for |kStringPolicy|, which is a string
  // policy that does not reference any external data.
  external_data_manager_->Fetch(kStringPolicy, ConstructFetchCallback(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  EXPECT_TRUE(callback_data_.find(0) != callback_data_.end());
  EXPECT_FALSE(callback_data_[0]);
  ResetCallbackData();

  // Attempt to retrieve external data for |kUnknownPolicy|, which is not a
  // known policy.
  external_data_manager_->Fetch(kUnknownPolicy, ConstructFetchCallback(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  EXPECT_TRUE(callback_data_.find(1) != callback_data_.end());
  EXPECT_FALSE(callback_data_[1]);
  ResetCallbackData();

  // Set an invalid external data reference for |k10BytePolicy|.
  SetExternalDataReference(k10BytePolicy,
                           ConstructMetadata(std::string(), std::string()));
  cloud_policy_store_.NotifyStoreLoaded();

  // Attempt to retrieve external data for |k10BytePolicy|, which now has an
  // invalid reference.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  EXPECT_TRUE(callback_data_.find(2) != callback_data_.end());
  EXPECT_FALSE(callback_data_[2]);
  ResetCallbackData();
}

// Verifies that external data referenced by a policy is downloaded and cached
// when first requested. Subsequent requests are served from the cache without
// further download attempts.
TEST_F(CloudExternalDataManagerBaseTest, DownloadAndCache) {
  // Serve valid external data for |k10BytePolicy|.
  SetFakeResponse(k10BytePolicyURL, k10ByteData, net::HTTP_OK);
  external_data_manager_->Connect(url_loader_factory_);

  // Retrieve external data for |k10BytePolicy|. Verify that a download happens
  // and the callback is invoked with the downloaded data.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[0]);
  EXPECT_EQ(k10ByteData, *callback_data_[0]);
  ResetCallbackData();

  // Stop serving external data for |k10BytePolicy|.
  test_url_loader_factory_.ClearResponses();

  // Retrieve external data for |k10BytePolicy| again. Verify that no download
  // is attempted but the callback is still invoked with the expected data,
  // served from the cache.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[1]);
  EXPECT_EQ(k10ByteData, *callback_data_[1]);
  ResetCallbackData();

  // Explicitly tell the external_data_manager_ to not make any download
  // attempts.
  external_data_manager_->Disconnect();

  // Retrieve external data for |k10BytePolicy| again. Verify that even though
  // downloads are not allowed, the callback is still invoked with the expected
  // data, served from the cache.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[2]);
  EXPECT_EQ(k10ByteData, *callback_data_[2]);
  ResetCallbackData();

  // Verify that the downloaded data is present in the cache.
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  std::string data;
  EXPECT_FALSE(
      CloudExternalDataStore(kCacheKey,
                             task_environment_.GetMainThreadTaskRunner(),
                             resource_cache_.get())
          .Load(k10BytePolicy, crypto::SHA256HashString(k10ByteData), 10, &data)
          .empty());
  EXPECT_EQ(k10ByteData, data);
}

// Verifies that a request to download and cache all external data referenced by
// policies is carried out correctly. Subsequent requests for the data are
// served from the cache without further download attempts.
TEST_F(CloudExternalDataManagerBaseTest, DownloadAndCacheAll) {
  // Serve valid external data for |k10BytePolicy| and |k20BytePolicy|.
  SetFakeResponse(k10BytePolicyURL, k10ByteData, net::HTTP_OK);
  SetFakeResponse(k20BytePolicyURL, k20ByteData, net::HTTP_OK);
  external_data_manager_->Connect(url_loader_factory_);

  // Request that external data referenced by all policies be downloaded.
  FetchAll();
  base::RunLoop().RunUntilIdle();

  // Stop serving external data for |k10BytePolicy| and |k20BytePolicy|.
  test_url_loader_factory_.ClearResponses();

  // Retrieve external data for |k10BytePolicy| and |k20BytePolicy|. Verify that
  // no downloads are attempted but the callbacks are still invoked with the
  // expected data, served from the cache.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(0));
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, callback_data_.size());
  ASSERT_TRUE(callback_data_[0]);
  EXPECT_EQ(k10ByteData, *callback_data_[0]);
  ASSERT_TRUE(callback_data_[1]);
  EXPECT_EQ(k20ByteData, *callback_data_[1]);
  ResetCallbackData();

  // Explicitly tell the external_data_manager_ to not make any download
  // attempts.
  external_data_manager_->Disconnect();

  // Retrieve external data for |k10BytePolicy| and |k20BytePolicy|. Verify that
  // even though downloads are not allowed, the callbacks are still invoked with
  // the expected data, served from the cache.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(2));
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, callback_data_.size());
  ASSERT_TRUE(callback_data_[2]);
  EXPECT_EQ(k10ByteData, *callback_data_[2]);
  ASSERT_TRUE(callback_data_[3]);
  EXPECT_EQ(k20ByteData, *callback_data_[3]);
  ResetCallbackData();

  // Verify that the downloaded data is present in the cache.
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  CloudExternalDataStore cache(kCacheKey,
                               task_environment_.GetMainThreadTaskRunner(),
                               resource_cache_.get());
  std::string data;
  EXPECT_FALSE(
      cache
          .Load(k10BytePolicy, crypto::SHA256HashString(k10ByteData), 10, &data)
          .empty());
  EXPECT_EQ(k10ByteData, data);
  EXPECT_FALSE(
      cache
          .Load(k20BytePolicy, crypto::SHA256HashString(k20ByteData), 20, &data)
          .empty());
  EXPECT_EQ(k20ByteData, data);
}

// Verifies that when the external data referenced by a policy is not present in
// the cache and downloads are not allowed, a request to retrieve the data is
// enqueued and carried out when downloads become possible.
TEST_F(CloudExternalDataManagerBaseTest, DownloadAfterConnect) {
  // Attempt to retrieve external data for |k10BytePolicy|. Verify that the
  // callback is not invoked as the request remains pending.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_data_.empty());
  ResetCallbackData();

  // Serve valid external data for |k10BytePolicy| and allow the
  // external_data_manager_ to perform downloads.
  SetFakeResponse(k10BytePolicyURL, k10ByteData, net::HTTP_OK);
  external_data_manager_->Connect(url_loader_factory_);

  // Verify that a download happens and the callback is invoked with the
  // downloaded data.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[0]);
  EXPECT_EQ(k10ByteData, *callback_data_[0]);
  ResetCallbackData();
}

// Verifies that when the external data referenced by a policy is not present in
// the cache and cannot be downloaded at this time, a request to retrieve the
// data is enqueued to be retried later.
TEST_F(CloudExternalDataManagerBaseTest, DownloadError) {
  // Make attempts to download the external data for |k20BytePolicy| fail with
  // an error.
  SetFakeResponse(k20BytePolicyURL, std::string(),
                  net::HTTP_INTERNAL_SERVER_ERROR);
  external_data_manager_->Connect(url_loader_factory_);

  // Attempt to retrieve external data for |k20BytePolicy|. Verify that the
  // callback is not invoked as the download attempt fails and the request
  // remains pending.
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_data_.empty());
  ResetCallbackData();

  // Modify the external data reference for |k20BytePolicy|, allowing the
  // download to be retried immediately.
  SetExternalDataReference(
      k20BytePolicy,
      ConstructMetadata(k20BytePolicyURL,
                        crypto::SHA256HashString(k10ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();

  // Attempt to retrieve external data for |k20BytePolicy| again. Verify that
  // no callback is invoked still as the download attempt fails again and the
  // request remains pending.
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_data_.empty());
  ResetCallbackData();

  // Modify the external data reference for |k20BytePolicy|, allowing the
  // download to be retried immediately.
  SetExternalDataReference(
      k20BytePolicy,
      ConstructMetadata(k20BytePolicyURL,
                        crypto::SHA256HashString(k20ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();

  // Serve external data for |k20BytePolicy| that does not match the hash
  // specified in its current external data reference.
  SetFakeResponse(k20BytePolicyURL, k10ByteData, net::HTTP_OK);

  // Attempt to retrieve external data for |k20BytePolicy| again. Verify that
  // no callback is invoked still as the downloaded succeeds but returns data
  // that does not match the external data reference.
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_data_.empty());
  ResetCallbackData();

  // Modify the external data reference for |k20BytePolicy|, allowing the
  // download to be retried immediately. The external data reference now matches
  // the data being served.
  SetExternalDataReference(
      k20BytePolicy,
      ConstructMetadata(k20BytePolicyURL,
                        crypto::SHA256HashString(k10ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();

  // Attempt to retrieve external data for |k20BytePolicy| again. Verify that
  // the current callback and the three previously enqueued callbacks are
  // invoked with the downloaded data now.
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(3));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4u, callback_data_.size());
  ASSERT_TRUE(callback_data_[0]);
  EXPECT_EQ(k10ByteData, *callback_data_[0]);
  ASSERT_TRUE(callback_data_[1]);
  EXPECT_EQ(k10ByteData, *callback_data_[1]);
  ASSERT_TRUE(callback_data_[2]);
  EXPECT_EQ(k10ByteData, *callback_data_[2]);
  ASSERT_TRUE(callback_data_[3]);
  EXPECT_EQ(k10ByteData, *callback_data_[3]);
  ResetCallbackData();
}

// Verifies that when the external data referenced by a policy is present in the
// cache, a request to retrieve it is served from the cache without any download
// attempts.
TEST_F(CloudExternalDataManagerBaseTest, LoadFromCache) {
  // Store valid external data for |k10BytePolicy| in the cache.
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(CloudExternalDataStore(
                   kCacheKey, task_environment_.GetMainThreadTaskRunner(),
                   resource_cache_.get())
                   .Store(k10BytePolicy, crypto::SHA256HashString(k10ByteData),
                          k10ByteData)
                   .empty());

  // Instantiate an external_data_manager_ that uses the primed cache.
  SetUpExternalDataManager();
  external_data_manager_->Connect(url_loader_factory_);

  // Retrieve external data for |k10BytePolicy|. Verify that no download is
  // attempted but the callback is still invoked with the expected data, served
  // from the cache.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[0]);
  EXPECT_EQ(k10ByteData, *callback_data_[0]);
  ResetCallbackData();
}

// Verifies that cache entries which do not correspond to the external data
// referenced by any policy are pruned on startup.
TEST_F(CloudExternalDataManagerBaseTest, PruneCacheOnStartup) {
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<CloudExternalDataStore> cache(new CloudExternalDataStore(
      kCacheKey, task_environment_.GetMainThreadTaskRunner(),
      resource_cache_.get()));
  // Store valid external data for |k10BytePolicy| in the cache.
  EXPECT_FALSE(cache
                   ->Store(k10BytePolicy, crypto::SHA256HashString(k10ByteData),
                           k10ByteData)
                   .empty());
  // Store external data for |k20BytePolicy| that does not match the hash in its
  // external data reference.
  EXPECT_FALSE(cache
                   ->Store(k20BytePolicy, crypto::SHA256HashString(k10ByteData),
                           k10ByteData)
                   .empty());
  // Store external data for |kUnknownPolicy|, which is not a known policy and
  // therefore, cannot be referencing any external data.
  EXPECT_FALSE(cache
                   ->Store(kUnknownPolicy,
                           crypto::SHA256HashString(k10ByteData), k10ByteData)
                   .empty());
  cache.reset();

  // Instantiate and destroy an ExternalDataManager that uses the primed cache.
  SetUpExternalDataManager();
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();

  cache.reset(new CloudExternalDataStore(
      kCacheKey, task_environment_.GetMainThreadTaskRunner(),
      resource_cache_.get()));
  std::string data;
  // Verify that the valid external data for |k10BytePolicy| is still in the
  // cache.
  EXPECT_FALSE(cache
                   ->Load(k10BytePolicy, crypto::SHA256HashString(k10ByteData),
                          10, &data)
                   .empty());
  EXPECT_EQ(k10ByteData, data);
  // Verify that the external data for |k20BytePolicy| and |kUnknownPolicy| has
  // been pruned from the cache.
  EXPECT_TRUE(cache
                  ->Load(k20BytePolicy, crypto::SHA256HashString(k10ByteData),
                         20, &data)
                  .empty());
  EXPECT_TRUE(cache
                  ->Load(kUnknownPolicy, crypto::SHA256HashString(k10ByteData),
                         20, &data)
                  .empty());
}

// Verifies that when the external data referenced by a policy is present in the
// cache and the reference changes, the old data is pruned from the cache.
TEST_F(CloudExternalDataManagerBaseTest, PruneCacheOnChange) {
  // Store valid external data for |k20BytePolicy| in the cache.
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<CloudExternalDataStore> cache(new CloudExternalDataStore(
      kCacheKey, task_environment_.GetMainThreadTaskRunner(),
      resource_cache_.get()));
  EXPECT_FALSE(cache
                   ->Store(k20BytePolicy, crypto::SHA256HashString(k20ByteData),
                           k20ByteData)
                   .empty());
  cache.reset();

  // Instantiate an ExternalDataManager that uses the primed cache.
  SetUpExternalDataManager();
  external_data_manager_->Connect(url_loader_factory_);

  // Modify the external data reference for |k20BytePolicy|.
  SetExternalDataReference(
      k20BytePolicy,
      ConstructMetadata(k20BytePolicyURL,
                        crypto::SHA256HashString(k10ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();

  // Verify that the old external data for |k20BytePolicy| has been pruned from
  // the cache.
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  cache.reset(new CloudExternalDataStore(
      kCacheKey, task_environment_.GetMainThreadTaskRunner(),
      resource_cache_.get()));
  std::string data;
  EXPECT_TRUE(cache
                  ->Load(k20BytePolicy, crypto::SHA256HashString(k20ByteData),
                         20, &data)
                  .empty());
}

// Verifies that corrupt cache entries are detected and deleted when accessed.
TEST_F(CloudExternalDataManagerBaseTest, CacheCorruption) {
  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<CloudExternalDataStore> cache(new CloudExternalDataStore(
      kCacheKey, task_environment_.GetMainThreadTaskRunner(),
      resource_cache_.get()));
  // Store external data for |k10BytePolicy| that exceeds the maximal external
  // data size allowed for that policy.
  EXPECT_FALSE(cache
                   ->Store(k10BytePolicy, crypto::SHA256HashString(k20ByteData),
                           k20ByteData)
                   .empty());
  // Store external data for |k20BytePolicy| that is corrupted and does not
  // match the expected hash.
  EXPECT_FALSE(cache
                   ->Store(k20BytePolicy, crypto::SHA256HashString(k20ByteData),
                           k10ByteData)
                   .empty());
  cache.reset();

  SetUpExternalDataManager();
  // Serve external data for |k10BytePolicy| that exceeds the maximal external
  // data size allowed for that policy.
  SetFakeResponse(k10BytePolicyURL, k20ByteData, net::HTTP_OK);
  external_data_manager_->Connect(url_loader_factory_);

  // Modify the external data reference for |k10BytePolicy| to match the
  // external data being served.
  SetExternalDataReference(
      k10BytePolicy,
      ConstructMetadata(k10BytePolicyURL,
                        crypto::SHA256HashString(k20ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();

  // Retrieve external data for |k10BytePolicy|. Verify that the callback is
  // not invoked as the cached and downloaded external data exceed the maximal
  // size allowed for this policy and the request remains pending.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_data_.empty());
  ResetCallbackData();

  // Serve valid external data for |k20BytePolicy|.
  SetFakeResponse(k20BytePolicyURL, k20ByteData, net::HTTP_OK);

  // Retrieve external data for |k20BytePolicy|. Verify that the callback is
  // invoked with the valid downloaded data, not the invalid data in the cache.
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[1]);
  EXPECT_EQ(k20ByteData, *callback_data_[1]);
  ResetCallbackData();

  external_data_manager_.reset();
  base::RunLoop().RunUntilIdle();
  cache.reset(new CloudExternalDataStore(
      kCacheKey, task_environment_.GetMainThreadTaskRunner(),
      resource_cache_.get()));
  std::string data;
  // Verify that the invalid external data for |k10BytePolicy| has been pruned
  // from the cache. Load() will return |false| in two cases:
  // 1) The cache entry for |k10BytePolicy| has been pruned.
  // 2) The cache entry for |k10BytePolicy| still exists but the cached data
  //    does not match the expected hash or exceeds the maximum size allowed.
  // To test for the former, Load() is called with a maximum data size and hash
  // that would allow the data originally written to the cache to be loaded.
  // When this fails, it is certain that the original data is no longer present
  // in the cache.
  EXPECT_TRUE(cache
                  ->Load(k10BytePolicy, crypto::SHA256HashString(k20ByteData),
                         20, &data)
                  .empty());
  // Verify that the invalid external data for |k20BytePolicy| has been replaced
  // with the downloaded valid data in the cache.
  EXPECT_FALSE(cache
                   ->Load(k20BytePolicy, crypto::SHA256HashString(k20ByteData),
                          20, &data)
                   .empty());
  EXPECT_EQ(k20ByteData, data);
}

// Verifies that when the external data reference for a policy changes while a
// download of the external data for that policy is pending, the download is
// immediately retried using the new reference.
TEST_F(CloudExternalDataManagerBaseTest, PolicyChangeWhileDownloadPending) {
  // Make attempts to download the external data for |k10BytePolicy| and
  // |k20BytePolicy| fail with an error.
  SetFakeResponse(k10BytePolicyURL, std::string(),
                  net::HTTP_INTERNAL_SERVER_ERROR);
  SetFakeResponse(k20BytePolicyURL, std::string(),
                  net::HTTP_INTERNAL_SERVER_ERROR);
  external_data_manager_->Connect(url_loader_factory_);

  // Attempt to retrieve external data for |k10BytePolicy| and |k20BytePolicy|.
  // Verify that no callbacks are invoked as the download attempts fail and the
  // requests remain pending.
  external_data_manager_->Fetch(k10BytePolicy, ConstructFetchCallback(0));
  external_data_manager_->Fetch(k20BytePolicy, ConstructFetchCallback(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_data_.empty());
  ResetCallbackData();

  // Modify the external data reference for |k10BytePolicy| to be invalid.
  // Verify that the callback is invoked as the policy no longer has a valid
  // external data reference.
  cloud_policy_store_.policy_map_.Erase(k10BytePolicy);
  cloud_policy_store_.NotifyStoreLoaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  EXPECT_TRUE(callback_data_.find(0) != callback_data_.end());
  EXPECT_FALSE(callback_data_[0]);
  ResetCallbackData();

  // Serve valid external data for |k20BytePolicy|.
  test_url_loader_factory_.ClearResponses();
  SetFakeResponse(k20BytePolicyURL, k10ByteData, net::HTTP_OK);

  // Modify the external data reference for |k20BytePolicy| to match the
  // external data now being served. Verify that the callback is invoked with
  // the downloaded data.
  SetExternalDataReference(
      k20BytePolicy,
      ConstructMetadata(k20BytePolicyURL,
                        crypto::SHA256HashString(k10ByteData)));
  cloud_policy_store_.NotifyStoreLoaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, callback_data_.size());
  ASSERT_TRUE(callback_data_[1]);
  EXPECT_EQ(k10ByteData, *callback_data_[1]);
  ResetCallbackData();
}

}  // namespace policy
