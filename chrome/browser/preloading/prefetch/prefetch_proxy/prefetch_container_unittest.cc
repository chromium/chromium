// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_container.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class PrefetchContainerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;

    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsHostCookie());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        *cookie.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));

    // This will run until the cookie is set.
    run_loop.Run();

    // This will run until the cookie listener gets the cookie change.
    base::RunLoop().RunUntilIdle();

    return result;
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

 private:
  // Cookie manager for all tests
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

TEST_F(PrefetchContainerTest, ConstructContainer) {
  GURL test_url = GURL("https://www.test.com/");
  PrefetchType test_type = PrefetchType(/*use_isolated_network_context=*/true,
                                        /*use_prefetch_proxy=*/true,
                                        /*can_prefetch_subresources=*/false);
  size_t test_prediction_index = 4;

  PrefetchContainer prefetch_container(test_url, test_type,
                                       test_prediction_index);

  EXPECT_EQ(prefetch_container.GetUrl(), test_url);
  EXPECT_EQ(prefetch_container.GetOriginalPredictionIndex(),
            test_prediction_index);
  EXPECT_EQ(prefetch_container.GetPrefetchType(), test_type);
  EXPECT_FALSE(prefetch_container.IsDecoy());

  prefetch_container.SetIsDecoy(true);
  EXPECT_TRUE(prefetch_container.IsDecoy());
}

TEST_F(PrefetchContainerTest, PrefetchStatus) {
  PrefetchContainer prefetch_container(
      GURL("https://www.test.com/"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   /*can_prefetch_subresources=*/false),
      0);

  EXPECT_FALSE(prefetch_container.HasPrefetchStatus());

  prefetch_container.SetPrefetchStatus(
      PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe);

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe);
}

TEST_F(PrefetchContainerTest, CookieListener) {
  GURL test_url = GURL("https://www.test.com/");
  PrefetchContainer prefetch_container(
      test_url,
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   /*can_prefetch_subresources=*/false),
      0);

  EXPECT_FALSE(prefetch_container.HaveCookiesChanged());

  base::RunLoop run_loop;

  prefetch_container.RegisterCookieListener(
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& expected_url,
             const GURL& cookie_changed_url) {
            if (expected_url != cookie_changed_url)
              return;

            run_loop->Quit();
          },
          &run_loop, test_url),
      cookie_manager());

  EXPECT_FALSE(prefetch_container.HaveCookiesChanged());

  EXPECT_TRUE(SetCookie(test_url, "testing"));
  run_loop.Run();

  EXPECT_TRUE(prefetch_container.HaveCookiesChanged());
}

TEST_F(PrefetchContainerTest, HandlePrefetchedResponse) {
  PrefetchContainer prefetch_container(
      GURL("https://www.test.com/"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   /*can_prefetch_subresources=*/false),
      0);
  EXPECT_FALSE(prefetch_container.HasPrefetchedResponse());

  std::string body = "test_body";
  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response =
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>(body));

  prefetch_container.SetPrefetchedResponse(std::move(prefetched_response));
  ASSERT_TRUE(prefetch_container.HasPrefetchedResponse());

  std::unique_ptr<PrefetchedMainframeResponseContainer> cloned_response =
      prefetch_container.ClonePrefetchedResponse();
  ASSERT_TRUE(cloned_response);
  EXPECT_EQ(*cloned_response->TakeBody(), body);
  ASSERT_TRUE(prefetch_container.HasPrefetchedResponse());

  std::unique_ptr<PrefetchedMainframeResponseContainer> taken_response =
      prefetch_container.ReleasePrefetchedResponse();
  ASSERT_TRUE(taken_response);
  EXPECT_EQ(*taken_response->TakeBody(), body);
  EXPECT_FALSE(prefetch_container.HasPrefetchedResponse());
}

TEST_F(PrefetchContainerTest, IsPrefetchedResponseValid) {
  PrefetchContainer prefetch_container(
      GURL("https://www.test.com/"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   /*can_prefetch_subresources=*/false),
      0);

  EXPECT_FALSE(prefetch_container.HasPrefetchedResponse());
  EXPECT_FALSE(
      prefetch_container.IsPrefetchedResponseValid(base::TimeDelta::Max()));

  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response =
      std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>(""));

  prefetch_container.SetPrefetchedResponse(std::move(prefetched_response));
  ASSERT_TRUE(prefetch_container.HasPrefetchedResponse());

  EXPECT_TRUE(
      prefetch_container.IsPrefetchedResponseValid(base::TimeDelta::Max()));
  EXPECT_FALSE(prefetch_container.IsPrefetchedResponseValid(base::TimeDelta()));
}

TEST_F(PrefetchContainerTest, NoStatePrefetchStatus) {
  PrefetchContainer prefetch_container(
      GURL("https://www.test.com/"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   /*can_prefetch_subresources=*/true),
      0);
  EXPECT_EQ(prefetch_container.GetNoStatePrefetchStatus(),
            PrefetchContainer::NoStatePrefetchStatus::kNotStarted);

  prefetch_container.SetNoStatePrefetchStatus(
      PrefetchContainer::NoStatePrefetchStatus::kInProgress);

  EXPECT_EQ(prefetch_container.GetNoStatePrefetchStatus(),
            PrefetchContainer::NoStatePrefetchStatus::kInProgress);

  prefetch_container.SetNoStatePrefetchStatus(
      PrefetchContainer::NoStatePrefetchStatus::kSucceeded);

  EXPECT_EQ(prefetch_container.GetNoStatePrefetchStatus(),
            PrefetchContainer::NoStatePrefetchStatus::kSucceeded);
}

TEST_F(PrefetchContainerTest, ChangePrefetchType) {
  GURL test_url = GURL("https://www.test.com/");

  PrefetchType cross_origin_private =
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   /*can_prefetch_subresources=*/false);
  PrefetchType cross_origin_private_with_subresources = PrefetchType(
      /*use_isolated_network_context=*/true,
      /*use_prefetch_proxy=*/true,
      /*can_prefetch_subresources=*/true);
  PrefetchType cross_origin_public =
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/false,
                   /*can_prefetch_subresources=*/false);
  PrefetchType same_origin_public = PrefetchType(
      /*use_isolated_network_context=*/false,
      /*use_prefetch_proxy=*/false,
      /*can_prefetch_subresources=*/false);

  PrefetchContainer prefetch_container(test_url, cross_origin_private, 0);

  base::HistogramTester histogram_tester;

  // Test invalid state changes.
  prefetch_container.ChangePrefetchType(cross_origin_public);

  EXPECT_EQ(prefetch_container.GetPrefetchType(), cross_origin_private);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.WasPrefetchTypeStateChangeValid", false, 1);

  prefetch_container.ChangePrefetchType(same_origin_public);

  EXPECT_EQ(prefetch_container.GetPrefetchType(), cross_origin_private);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.WasPrefetchTypeStateChangeValid", false, 2);

  // Test valid state change.
  prefetch_container.ChangePrefetchType(cross_origin_private_with_subresources);

  EXPECT_EQ(prefetch_container.GetPrefetchType(),
            cross_origin_private_with_subresources);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.WasPrefetchTypeStateChangeValid", 3);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.WasPrefetchTypeStateChangeValid", false, 2);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.WasPrefetchTypeStateChangeValid", true, 1);
}

}  // namespace
