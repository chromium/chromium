// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_url_loader_interceptor.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace {

const gfx::Size kSize(640, 480);

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL TestURL() {
  return GURL("https://test.com/path");
}

}  // namespace

// These tests leak mojo objects (like the PrefetchProxyURLLoader) because
// they do not have valid mojo channels, which would normally delete the bound
// objects on destruction. This is expected and cannot be easily fixed without
// rewriting these as browsertests. The trade off for the speed and flexibility
// of unittests is an intentional decision.
#if defined(LEAK_SANITIZER)
#define DISABLE_ASAN(x) DISABLED_##x
#else
#define DISABLE_ASAN(x) x
#endif

class TestPrefetchProxyURLLoaderInterceptor
    : public PrefetchProxyURLLoaderInterceptor {
 public:
  explicit TestPrefetchProxyURLLoaderInterceptor(int frame_tree_node_id)
      : PrefetchProxyURLLoaderInterceptor(frame_tree_node_id) {}
  ~TestPrefetchProxyURLLoaderInterceptor() override = default;

  void SetHasPrefetchedResponse(const GURL& url, bool has_prefetch) {
    expected_url_ = url;
    has_prefetch_ = has_prefetch;
  }

  std::unique_ptr<PrefetchedMainframeResponseContainer> GetPrefetchedResponse(
      const GURL& url) override {
    EXPECT_EQ(expected_url_, url);
    if (has_prefetch_) {
      return std::make_unique<PrefetchedMainframeResponseContainer>(
          net::IsolationInfo(), network::mojom::URLResponseHead::New(),
          std::make_unique<std::string>("body"));
    }
    return nullptr;
  }

 private:
  GURL expected_url_;
  bool has_prefetch_ = false;
};

class PrefetchProxyURLLoaderInterceptorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PrefetchProxyURLLoaderInterceptorTest() = default;
  ~PrefetchProxyURLLoaderInterceptorTest() override = default;

  void TearDown() override {
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile());
    no_state_prefetch_manager->CancelAllPrerenders();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<prerender::NoStatePrefetchHandle> StartPrerender(
      const GURL& url) {
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile());

    return no_state_prefetch_manager->AddPrerenderFromNavigationPredictor(
        url,
        web_contents()->GetController().GetDefaultSessionStorageNamespace(),
        kSize);
  }

  void WaitForCallback() {
    if (was_intercepted_.has_value())
      return;

    base::RunLoop run_loop;
    waiting_for_callback_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void HandlerCallback(
      content::URLLoaderRequestInterceptor::RequestHandler callback) {
    was_intercepted_ = !callback.is_null();
    if (waiting_for_callback_closure_) {
      std::move(waiting_for_callback_closure_).Run();
    }
  }

  base::Optional<bool> was_intercepted() { return was_intercepted_; }

 private:
  base::Optional<bool> was_intercepted_;
  base::OnceClosure waiting_for_callback_closure_;
};

TEST_F(PrefetchProxyURLLoaderInterceptorTest, DISABLE_ASAN(WantIntercept)) {
  std::unique_ptr<TestPrefetchProxyURLLoaderInterceptor> interceptor =
      std::make_unique<TestPrefetchProxyURLLoaderInterceptor>(
          web_contents()->GetMainFrame()->GetFrameTreeNodeId());

  interceptor->SetHasPrefetchedResponse(TestURL(), true);

  network::ResourceRequest request;
  request.url = TestURL();
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor->MaybeCreateLoader(
      request, profile(),
      base::BindOnce(&PrefetchProxyURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_TRUE(was_intercepted().value());
}

TEST_F(PrefetchProxyURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotWantIntercept)) {
  std::unique_ptr<TestPrefetchProxyURLLoaderInterceptor> interceptor =
      std::make_unique<TestPrefetchProxyURLLoaderInterceptor>(
          web_contents()->GetMainFrame()->GetFrameTreeNodeId());

  interceptor->SetHasPrefetchedResponse(TestURL(), false);

  network::ResourceRequest request;
  request.url = TestURL();
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor->MaybeCreateLoader(
      request, profile(),
      base::BindOnce(&PrefetchProxyURLLoaderInterceptorTest::HandlerCallback,
                     base::Unretained(this)));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());
}

// Testing of the probe is done in browsertests.
