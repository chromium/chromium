// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/prewarm_http_disk_cache_manager.h"

#include <tuple>

#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

namespace {
using Job = std::tuple<url::Origin, GURL, net::IsolationInfo::RequestType>;
using Jobs = std::queue<Job>;

const auto kMainFrameRequestType = net::IsolationInfo::RequestType::kMainFrame;
const auto kSubresourceRequestType = net::IsolationInfo::RequestType::kOther;
}  // namespace

class PrewarmHttpDiskCacheManagerTest : public testing::Test {
 public:
  PrewarmHttpDiskCacheManagerTest()
      : prewarm_http_disk_cache_manager_(std::make_unique<
                                         PrewarmHttpDiskCacheManager>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))) {}
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
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<PrewarmHttpDiskCacheManager> prewarm_http_disk_cache_manager_;
};

TEST_F(PrewarmHttpDiskCacheManagerTest, OneMainResourceUrl) {
  const GURL kMainResourceUrl("https://a.com/");
  const GURL kSubresourceUrl1("https://p.com/");
  const GURL kSubresourceUrl2("https://q.com/");
  const url::Origin kOrigin = url::Origin::Create(kMainResourceUrl);
  test_url_loader_factory_.AddResponse(kMainResourceUrl.spec(), "");
  test_url_loader_factory_.AddResponse(kSubresourceUrl1.spec(), "");
  test_url_loader_factory_.AddResponse(kSubresourceUrl2.spec(), "");
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(
      kMainResourceUrl, {kSubresourceUrl1, kSubresourceUrl2});
  EXPECT_EQ(Jobs({{kOrigin, kMainResourceUrl, kMainFrameRequestType},
                  {kOrigin, kSubresourceUrl1, kSubresourceRequestType},
                  {kOrigin, kSubresourceUrl2, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin, kSubresourceUrl1, kSubresourceRequestType},
                  {kOrigin, kSubresourceUrl2, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin, kSubresourceUrl2, kSubresourceRequestType}}),
            GetQueuedJobs());
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
  test_url_loader_factory_.AddResponse(kMainResourceUrl1.spec(), "");
  test_url_loader_factory_.AddResponse(kMainResourceUrl2.spec(), "");
  test_url_loader_factory_.AddResponse(kSubresourceUrl1.spec(), "");
  test_url_loader_factory_.AddResponse(kSubresourceUrl2.spec(), "");
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(
      kMainResourceUrl1, {kSubresourceUrl1, kSubresourceUrl2});
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(kMainResourceUrl1,
                                                          {kSubresourceUrl1});
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(kMainResourceUrl2,
                                                          {kSubresourceUrl1});
  EXPECT_EQ(Jobs({{kOrigin1, kMainResourceUrl1, kMainFrameRequestType},
                  {kOrigin1, kSubresourceUrl1, kSubresourceRequestType},
                  {kOrigin1, kSubresourceUrl2, kSubresourceRequestType},
                  {kOrigin2, kMainResourceUrl2, kMainFrameRequestType},
                  {kOrigin2, kSubresourceUrl1, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin1, kSubresourceUrl1, kSubresourceRequestType},
                  {kOrigin1, kSubresourceUrl2, kSubresourceRequestType},
                  {kOrigin2, kMainResourceUrl2, kMainFrameRequestType},
                  {kOrigin2, kSubresourceUrl1, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin1, kSubresourceUrl2, kSubresourceRequestType},
                  {kOrigin2, kMainResourceUrl2, kMainFrameRequestType},
                  {kOrigin2, kSubresourceUrl1, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin2, kMainResourceUrl2, kMainFrameRequestType},
                  {kOrigin2, kSubresourceUrl1, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin2, kSubresourceUrl1, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_TRUE(GetQueuedJobs().empty());
}

TEST_F(PrewarmHttpDiskCacheManagerTest, IsolationInfo) {
  const GURL kMainResourceUrl("https://a.com/");
  const GURL kSubresourceUrl("https://p.com/");
  const url::Origin kOrigin = url::Origin::Create(kMainResourceUrl);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(request.url == kMainResourceUrl ||
                    request.url == kSubresourceUrl);
        auto expected_request_type = request.url == kMainResourceUrl
                                         ? kMainFrameRequestType
                                         : kSubresourceRequestType;
        auto expected_isolation_info = net::IsolationInfo::Create(
            expected_request_type, kOrigin, kOrigin,
            net::SiteForCookies::FromOrigin(kOrigin));
        EXPECT_TRUE(request.trusted_params->isolation_info.IsEqualForTesting(
            expected_isolation_info));
        test_url_loader_factory_.AddResponse(request.url.spec(), "");
      }));
  prewarm_http_disk_cache_manager_->MaybePrewarmResources(kMainResourceUrl,
                                                          {kSubresourceUrl});
  EXPECT_EQ(Jobs({{kOrigin, kMainResourceUrl, kMainFrameRequestType},
                  {kOrigin, kSubresourceUrl, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_EQ(Jobs({{kOrigin, kSubresourceUrl, kSubresourceRequestType}}),
            GetQueuedJobs());
  ProcessOneUrl();
  EXPECT_TRUE(GetQueuedJobs().empty());
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 2u);
}

}  // namespace predictors
