// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/origin_robots_rules.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_redirect {

constexpr char kLitePagesURL[] = "https://litepages.googlezip.net/robots?u=";
constexpr char kFooOrigin[] = "https://foo.com/";
constexpr char kTestResponse[] = "TEST RESPONSE";

class RobotsRulesFetcherState {
 public:
  OriginRobotsRules::RobotsRulesReceivedCallback
  GetRobotsRulesReceivedCallback() {
    return base::BindOnce(&RobotsRulesFetcherState::OnRobotsRulesReceived,
                          weak_ptr_factory_.GetWeakPtr());
  }

  OriginRobotsRules::NotifyResponseErrorCallback GetResponseErrorCallback() {
    return base::BindOnce(&RobotsRulesFetcherState::OnResponseErrorReceived,
                          weak_ptr_factory_.GetWeakPtr());
  }

 private:
  friend class SubresourceRedirectOriginRobotsRulesTest;

  void OnResponseErrorReceived(int response_code, base::TimeDelta retry_after) {
    EXPECT_FALSE(response_error_received_.has_value());
    response_error_received_ =
        std::pair<int, base::TimeDelta>(response_code, retry_after);
  }

  void OnRobotsRulesReceived(base::Optional<std::string> rules) {
    robots_rules_received_.push_back(rules);
  }

  base::Optional<std::pair<int, base::TimeDelta>> response_error_received_;
  std::vector<base::Optional<std::string>> robots_rules_received_;
  base::WeakPtrFactory<RobotsRulesFetcherState> weak_ptr_factory_{this};
};

class SubresourceRedirectOriginRobotsRulesTest : public testing::Test {
 public:
  SubresourceRedirectOriginRobotsRulesTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect,
          {{"enable_login_robots_based_compression", "true"},
           {"enable_public_image_hints_based_compression", "false"},
           {"enable_login_robots_for_low_memory", "true"}}}},
        {});
  }

  void CreateRobotsRulesFetcher(const std::string& origin) {
    rules_fetcher_state_ = std::make_unique<RobotsRulesFetcherState>();
    origin_robots_rules_ = std::make_unique<OriginRobotsRules>(
        shared_url_loader_factory_, url::Origin::Create(GURL(origin)),
        rules_fetcher_state_->GetResponseErrorCallback());
  }

  void GetRobotsRules() {
    origin_robots_rules_->GetRobotsRules(
        rules_fetcher_state_->GetRobotsRulesReceivedCallback());
  }

  bool SimulateResponse(const std::string& lite_pages_url,
                        std::string robots_origin,
                        const std::string& content,
                        int net_error = net::OK,
                        net::HttpStatusCode http_status = net::HTTP_OK,
                        bool is_cache_hit = false,
                        const std::string& retry_after = "") {
    GURL url(lite_pages_url +
             net::EscapeQueryParamValue(robots_origin + "robots.txt", true));
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(http_status);
    head->was_fetched_via_cache = is_cache_hit;
    head->mime_type = "text/html";
    head->headers->SetHeader("Retry-After", retry_after);
    network::URLLoaderCompletionStatus status;
    status.error_code = net_error;
    status.decoded_body_length = content.size();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        url, status, std::move(head), content);
  }

  base::Optional<std::pair<int, base::TimeDelta>> GetResponseErrorReceived() {
    return rules_fetcher_state_->response_error_received_;
  }

  std::vector<base::Optional<std::string>> GetRobotsRulesReceived() {
    return rules_fetcher_state_->robots_rules_received_;
  }

 protected:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<OriginRobotsRules> origin_robots_rules_;
  std::unique_ptr<RobotsRulesFetcherState> rules_fetcher_state_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(SubresourceRedirectOriginRobotsRulesTest, TestSuccessfulResponse) {
  CreateRobotsRulesFetcher(kFooOrigin);
  GetRobotsRules();
  // No robots rules received yet.
  EXPECT_EQ(0ULL, GetRobotsRulesReceived().size());

  EXPECT_TRUE(SimulateResponse(kLitePagesURL, kFooOrigin, kTestResponse));
  EXPECT_EQ(base::nullopt, GetResponseErrorReceived());
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.CacheHit", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.NetErrorCode", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  EXPECT_THAT(GetRobotsRulesReceived(),
              testing::ElementsAre(base::Optional<std::string>(kTestResponse)));

  // Subsequent calls will return the response immediately.
  GetRobotsRules();
  GetRobotsRules();
  EXPECT_THAT(GetRobotsRulesReceived(),
              testing::ElementsAre(base::Optional<std::string>(kTestResponse),
                                   base::Optional<std::string>(kTestResponse),
                                   base::Optional<std::string>(kTestResponse)));
  EXPECT_EQ(base::nullopt, GetResponseErrorReceived());
}

TEST_F(SubresourceRedirectOriginRobotsRulesTest, TestSuccessfulCachedResponse) {
  CreateRobotsRulesFetcher(kFooOrigin);
  GetRobotsRules();
  EXPECT_TRUE(SimulateResponse(kLitePagesURL, kFooOrigin, kTestResponse,
                               net::OK, net::HTTP_OK, true /*is_cache_hit*/));
  RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.CacheHit", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.NetErrorCode", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", net::HTTP_OK, 1);
  EXPECT_EQ(base::nullopt, GetResponseErrorReceived());
  EXPECT_THAT(GetRobotsRulesReceived(),
              testing::ElementsAre(base::Optional<std::string>(kTestResponse)));

  GetRobotsRules();
  GetRobotsRules();
  EXPECT_THAT(GetRobotsRulesReceived(),
              testing::ElementsAre(base::Optional<std::string>(kTestResponse),
                                   base::Optional<std::string>(kTestResponse),
                                   base::Optional<std::string>(kTestResponse)));
  EXPECT_EQ(base::nullopt, GetResponseErrorReceived());
}

TEST_F(SubresourceRedirectOriginRobotsRulesTest, TestFailedResponse) {
  CreateRobotsRulesFetcher(kFooOrigin);
  GetRobotsRules();
  EXPECT_TRUE(SimulateResponse(kLitePagesURL, kFooOrigin, kTestResponse,
                               net::OK, net::HTTP_INTERNAL_SERVER_ERROR));
  RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.CacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.NetErrorCode", 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      net::HTTP_INTERNAL_SERVER_ERROR, 1);
  EXPECT_THAT(
      *GetResponseErrorReceived(),
      testing::Pair(net::HTTP_INTERNAL_SERVER_ERROR, base::TimeDelta()));
  EXPECT_THAT(GetRobotsRulesReceived(), testing::ElementsAre(base::nullopt));

  // Subsequent calls will return the response immediately.
  GetRobotsRules();
  GetRobotsRules();
  EXPECT_THAT(
      GetRobotsRulesReceived(),
      testing::ElementsAre(base::nullopt, base::nullopt, base::nullopt));
}

TEST_F(SubresourceRedirectOriginRobotsRulesTest,
       TestFailedResponseWithRetryAfter) {
  CreateRobotsRulesFetcher(kFooOrigin);
  GetRobotsRules();
  EXPECT_TRUE(SimulateResponse(kLitePagesURL, kFooOrigin, kTestResponse,
                               net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                               false /*is_cache_hit*/, "120"));
  RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.CacheHit", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.NetErrorCode", 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
      net::HTTP_INTERNAL_SERVER_ERROR, 1);
  EXPECT_THAT(*GetResponseErrorReceived(),
              testing::Pair(net::HTTP_INTERNAL_SERVER_ERROR,
                            base::TimeDelta::FromSeconds(120)));
  EXPECT_THAT(GetRobotsRulesReceived(), testing::ElementsAre(base::nullopt));

  // Subsequent calls will return the response immediately.
  GetRobotsRules();
  GetRobotsRules();
  EXPECT_THAT(
      GetRobotsRulesReceived(),
      testing::ElementsAre(base::nullopt, base::nullopt, base::nullopt));
}

TEST_F(SubresourceRedirectOriginRobotsRulesTest, TestNetErrorFailedResponse) {
  CreateRobotsRulesFetcher(kFooOrigin);
  GetRobotsRules();
  EXPECT_TRUE(SimulateResponse(kLitePagesURL, kFooOrigin, kTestResponse,
                               net::ERR_ADDRESS_UNREACHABLE));
  RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.CacheHit", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "SubresourceRedirect.RobotsRulesFetcher.NetErrorCode",
      -net::ERR_ADDRESS_UNREACHABLE, 1);
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.RobotsRulesFetcher.ResponseCode", 0);
  EXPECT_EQ(base::nullopt, GetResponseErrorReceived());
  EXPECT_THAT(GetRobotsRulesReceived(), testing::ElementsAre(base::nullopt));

  // Subsequent calls will return the response immediately.
  GetRobotsRules();
  GetRobotsRules();
  EXPECT_THAT(
      GetRobotsRulesReceived(),
      testing::ElementsAre(base::nullopt, base::nullopt, base::nullopt));
}

}  // namespace subresource_redirect
