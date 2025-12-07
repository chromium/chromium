// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_auth_cache_status.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"

namespace {

// Helper function to create a ResourceLoadInfo object.
blink::mojom::ResourceLoadInfoPtr CreateResourceLoadInfo(
    const GURL& url,
    bool did_use_server_http_auth) {
  blink::mojom::ResourceLoadInfoPtr resource_load_info =
      blink::mojom::ResourceLoadInfo::New();
  resource_load_info->final_url = url;
  resource_load_info->did_use_server_http_auth = did_use_server_http_auth;
  return resource_load_info;
}

class HttpAuthCacheStatusTest : public ChromeRenderViewHostTestHarness {
 public:
  HttpAuthCacheStatusTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InitializePageLoadMetricsForWebContents(web_contents());
    HttpAuthCacheStatus::CreateForWebContents(web_contents());
    http_auth_cache_status_ =
        HttpAuthCacheStatus::FromWebContents(web_contents());
  }

  void TearDown() override {
    http_auth_cache_status_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::HistogramTester histogram_tester_;
  raw_ptr<HttpAuthCacheStatus> http_auth_cache_status_;
};

// This test verifies that when a same-partition subresource load completes with
// HTTP auth, the use counter is not incremented.
TEST_F(HttpAuthCacheStatusTest, UseCounterNotIncrementedSamePartition) {
  NavigateAndCommit(GURL("https://www.google.com/"));
  // Simulate a subresource load with HTTP Auth.
  http_auth_cache_status_->ResourceLoadComplete(
      main_rfh(), content::GlobalRequestID(),
      *CreateResourceLoadInfo(GURL("https://www.google.com/subresource"),
                              /*did_use_server_http_auth=*/true));

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<base::HistogramBase::Sample32>(
          blink::mojom::WebFeature::
              kDidUseServerHttpAuthOnCrossPartitionRequest),
      0);
}

// This test verifies that when a subresource load completes without HTTP auth,
// the use counter is not incremented.
TEST_F(HttpAuthCacheStatusTest, UseCounterNotIncrementedNoHttpAuth) {
  NavigateAndCommit(GURL("https://www.google.com/"));
  // Simulate a subresource load without HTTP Auth.
  http_auth_cache_status_->ResourceLoadComplete(
      main_rfh(), content::GlobalRequestID(),
      *CreateResourceLoadInfo(GURL("https://www.google.com/subresource"),
                              /*did_use_server_http_auth=*/false));

  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<base::HistogramBase::Sample32>(
          blink::mojom::WebFeature::
              kDidUseServerHttpAuthOnCrossPartitionRequest),
      0);
}

// This test verifies that when a subresource load completes with HTTP auth,
// the use counter is incremented for a cross-partition resource.
TEST_F(HttpAuthCacheStatusTest, UseCounterIncrementedCrossPartition) {
  NavigateAndCommit(GURL("https://www.google.com/"));
  // Simulate a cross-origin subresource load with HTTP Auth.
  http_auth_cache_status_->ResourceLoadComplete(
      main_rfh(), content::GlobalRequestID(),
      *CreateResourceLoadInfo(GURL("https://www.example.com/subresource"),
                              /*did_use_server_http_auth=*/true));
  histogram_tester_.ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<base::HistogramBase::Sample32>(
          blink::mojom::WebFeature::
              kDidUseServerHttpAuthOnCrossPartitionRequest),
      1);
}

}  // namespace
