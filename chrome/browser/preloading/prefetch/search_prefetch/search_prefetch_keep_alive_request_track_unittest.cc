// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_keep_alive_request_tracker.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kBeaconHostUrl[] = "https://www.example.com/beacon";

class SearchPrefetchKeepAliveRequestTrackerTest : public testing::Test {
 public:
  SearchPrefetchKeepAliveRequestTrackerTest() = default;

 protected:
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

  void EnableFeature() {
    feature_list()->InitAndEnableFeatureWithParameters(
        kSearchPrefetchBeaconLogging,
        {
            {"search_prefetch_beacon_host", "www.example.com"},
        });
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

network::ResourceRequest CreateKeepAliveRequest(const GURL& url) {
  network::ResourceRequest request;
  request.keepalive = true;
  request.url = url;
  return request;
}

TEST_F(SearchPrefetchKeepAliveRequestTrackerTest,
       MaybeCreate_FeatureNotEnabled) {
  feature_list()->InitAndDisableFeature(kSearchPrefetchBeaconLogging);
  network::ResourceRequest request =
      CreateKeepAliveRequest(GURL(kBeaconHostUrl));
  EXPECT_EQ(
      nullptr,
      SearchPrefetchKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
          request, /*browser_context=*/nullptr));
}

TEST_F(SearchPrefetchKeepAliveRequestTrackerTest,
       MaybeCreate_NotSearchPrefetchHost) {
  EnableFeature();
  network::ResourceRequest request =
      CreateKeepAliveRequest(GURL("https://www.example.com/others"));
  EXPECT_EQ(
      nullptr,
      SearchPrefetchKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
          request, /*browser_context=*/nullptr));
}

TEST_F(SearchPrefetchKeepAliveRequestTrackerTest,
       MaybeCreate_NotPrefetchBeacon) {
  EnableFeature();
  network::ResourceRequest request =
      CreateKeepAliveRequest(GURL("https://www.example.com/beacon?pf=a"));

  EXPECT_EQ(
      nullptr,
      SearchPrefetchKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
          request, /*browser_context=*/nullptr));
}

TEST_F(SearchPrefetchKeepAliveRequestTrackerTest,
       MaybeCreate_ReturnsTrackerForPrefetchBeacon) {
  EnableFeature();

  network::ResourceRequest request = CreateKeepAliveRequest(
      GURL("https://www.example.com/beacon?b=2&pf=cs&a=1"));
  EXPECT_NE(
      nullptr,
      SearchPrefetchKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
          request, /*browser_context=*/nullptr));
}

}  // namespace
