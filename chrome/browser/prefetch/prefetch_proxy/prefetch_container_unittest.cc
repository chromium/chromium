// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_container.h"

#include "base/time/time.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
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
  size_t test_prediction_index = 4;

  PrefetchContainer prefetch_container(test_url, test_prediction_index);

  EXPECT_EQ(prefetch_container.GetUrl(), test_url);
  EXPECT_EQ(prefetch_container.GetOriginalPredictionIndex(),
            test_prediction_index);
  EXPECT_FALSE(prefetch_container.AllowedToPrefetchSubresources());
  EXPECT_FALSE(prefetch_container.IsDecoy());

  prefetch_container.SetAllowedToPrefetchSubresources(true);
  EXPECT_TRUE(prefetch_container.AllowedToPrefetchSubresources());

  prefetch_container.SetIsDecoy(true);
  EXPECT_TRUE(prefetch_container.IsDecoy());
}

TEST_F(PrefetchContainerTest, PrefetchStatus) {
  PrefetchContainer prefetch_container(GURL("https://www.test.com/"), 0);

  EXPECT_FALSE(prefetch_container.HasPrefetchStatus());

  prefetch_container.SetPrefetchStatus(
      PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe);

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe);
}

TEST_F(PrefetchContainerTest, CookieListener) {
  GURL test_url = GURL("https://www.test.com/");
  PrefetchContainer prefetch_container(test_url, 0);

  EXPECT_FALSE(prefetch_container.HaveCookiesChanged());

  prefetch_container.RegisterCookieListener(cookie_manager());

  EXPECT_FALSE(prefetch_container.HaveCookiesChanged());

  EXPECT_TRUE(SetCookie(test_url, "testing"));

  EXPECT_TRUE(prefetch_container.HaveCookiesChanged());
}

TEST_F(PrefetchContainerTest, HandlePrefetchedResponse) {
  PrefetchContainer prefetch_container(GURL("https://www.test.com/"), 0);
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
  PrefetchContainer prefetch_container(GURL("https://www.test.com/"), 0);

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
  PrefetchContainer prefetch_container(GURL("https://www.test.com/"), 0);
  EXPECT_EQ(prefetch_container.GetNoStatePrefetchStatus(),
            PrefetchContainer::NoStatePrefetchStatus::kNotStarted);

  prefetch_container.SetAllowedToPrefetchSubresources(true);

  prefetch_container.SetNoStatePrefetchStatus(
      PrefetchContainer::NoStatePrefetchStatus::kInProgress);

  EXPECT_EQ(prefetch_container.GetNoStatePrefetchStatus(),
            PrefetchContainer::NoStatePrefetchStatus::kInProgress);

  prefetch_container.SetNoStatePrefetchStatus(
      PrefetchContainer::NoStatePrefetchStatus::kSucceeded);

  EXPECT_EQ(prefetch_container.GetNoStatePrefetchStatus(),
            PrefetchContainer::NoStatePrefetchStatus::kSucceeded);
}

}  // namespace
