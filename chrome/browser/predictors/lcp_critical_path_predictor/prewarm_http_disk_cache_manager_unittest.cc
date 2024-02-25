// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/prewarm_http_disk_cache_manager.h"

#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

namespace {
using Job = std::pair<url::Origin, GURL>;
using Jobs = std::queue<Job>;
}  // namespace

class PrewarmHttpDiskCacheManagerTest : public testing::Test {
 public:
  PrewarmHttpDiskCacheManagerTest()
      : profile_(std::make_unique<TestingProfile>()),
        prewarm_http_disk_cache_manager_(
            std::make_unique<PrewarmHttpDiskCacheManager>(
                profile_->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess())) {}
  ~PrewarmHttpDiskCacheManagerTest() override = default;

  PrewarmHttpDiskCacheManagerTest(const PrewarmHttpDiskCacheManagerTest&) =
      delete;
  PrewarmHttpDiskCacheManagerTest& operator=(
      const PrewarmHttpDiskCacheManagerTest&) = delete;

 protected:
  const Jobs& GetQueuedJobs() const {
    return prewarm_http_disk_cache_manager_->queued_jobs_;
  }
  void SetPrewarmFinishedCallback(base::OnceCallback<void()> callback) {
    prewarm_http_disk_cache_manager_->prewarm_finished_callback_for_testing_ =
        std::move(callback);
  }
  void ProcessOneUrl() {
    base::RunLoop run_loop;
    SetPrewarmFinishedCallback(
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  // base::test::ScopedFeatureList features_;
  // IO_MAINLOOP is needed for the EmbeddedTestServer.
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrewarmHttpDiskCacheManager> prewarm_http_disk_cache_manager_;
};

TEST_F(PrewarmHttpDiskCacheManagerTest, OneMainResourceUrl) {
  const GURL kMainResourceUrl("https://a.com/");
  const GURL kSubresourceUrl1("https://p.com/");
  const GURL kSubresourceUrl2("https://q.com/");
  const url::Origin kOrigin = url::Origin::Create(kMainResourceUrl);
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(
      kMainResourceUrl, {kSubresourceUrl1, kSubresourceUrl2});
  EXPECT_EQ(Jobs({{kOrigin, kMainResourceUrl},
                  {kOrigin, kSubresourceUrl1},
                  {kOrigin, kSubresourceUrl2}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin, kSubresourceUrl1}, {kOrigin, kSubresourceUrl2}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin, kSubresourceUrl2}}), GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_TRUE(GetQueuedJobs().empty());
}

TEST_F(PrewarmHttpDiskCacheManagerTest, TwoMainResourceUrls) {
  const GURL kMainResourceUrl1("https://a.com/");
  const GURL kMainResourceUrl2("https://b.com/");
  const GURL kSubresourceUrl1("https://p.com/");
  const GURL kSubresourceUrl2("https://q.com/");
  const url::Origin kOrigin1 = url::Origin::Create(kMainResourceUrl1);
  const url::Origin kOrigin2 = url::Origin::Create(kMainResourceUrl2);
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(
      kMainResourceUrl1, {kSubresourceUrl1, kSubresourceUrl2});
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(kMainResourceUrl1,
                                                          {kSubresourceUrl1});
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(kMainResourceUrl2,
                                                          {kSubresourceUrl1});
  EXPECT_EQ(Jobs({{kOrigin1, kMainResourceUrl1},
                  {kOrigin1, kSubresourceUrl1},
                  {kOrigin1, kSubresourceUrl2},
                  {kOrigin2, kMainResourceUrl2},
                  {kOrigin2, kSubresourceUrl1}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin1, kSubresourceUrl1},
                  {kOrigin1, kSubresourceUrl2},
                  {kOrigin2, kMainResourceUrl2},
                  {kOrigin2, kSubresourceUrl1}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin1, kSubresourceUrl2},
                  {kOrigin2, kMainResourceUrl2},
                  {kOrigin2, kSubresourceUrl1}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin2, kMainResourceUrl2}, {kOrigin2, kSubresourceUrl1}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin2, kSubresourceUrl1}}), GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_TRUE(GetQueuedJobs().empty());
}

}  // namespace predictors
