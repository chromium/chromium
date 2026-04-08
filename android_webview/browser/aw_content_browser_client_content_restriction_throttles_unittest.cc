// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/common/aw_features.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_content_client_initializer.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace android_webview {
namespace {

constexpr char kTestUrl[] = "https://example.com";
constexpr char kNavigationThrottleNameForLogging[] =
    "AwContentRestrictionNavigationThrottle";

class AwContentBrowserClientContentRestrictionThrottlesTest
    : public testing::Test {
 protected:
  void SetUp() override {
    mojo::core::Init();
    test_content_client_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();
    feature_list_creator_.CreateLocalState();
    browser_process_ = std::make_unique<AwBrowserProcess>(&client_);
  }

  content::BrowserTaskEnvironment task_environment_;
  AwFeatureListCreator feature_list_creator_;
  AwContentBrowserClient client_{&feature_list_creator_};

  // These are needed to set up the `AwBrowserContext`.
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<AwBrowserProcess> browser_process_;
};

TEST_F(AwContentBrowserClientContentRestrictionThrottlesTest,
       CreateURLLoaderThrottle) {
  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  auto wc_getter_cb =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });
  size_t size_disabled = 0;
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        android_webview::features::kWebViewContentRestrictionSupport);
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
        client_.CreateURLLoaderThrottles(
            request, /*browser_context=*/&context, wc_getter_cb,
            /*navigation_ui_data=*/nullptr, content::FrameTreeNodeId(),
            /*navigation_id=*/std::nullopt);
    size_disabled = throttles.size();
  }

  size_t size_enabled = 0;
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        android_webview::features::kWebViewContentRestrictionSupport);
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
        client_.CreateURLLoaderThrottles(
            request, /*browser_context=*/&context, wc_getter_cb,
            /*navigation_ui_data=*/nullptr, content::FrameTreeNodeId(),
            /*navigation_id=*/std::nullopt);
    size_enabled = throttles.size();
  }

  EXPECT_EQ(size_enabled, size_disabled + 1);

  // Wait until all pending tasks are processed before the browser context is
  // destroyed.
  task_environment_.RunUntilIdle();
}

TEST_F(AwContentBrowserClientContentRestrictionThrottlesTest,
       CreateNavigationThrottle) {
  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);

  // Associate web contents with the browser context for downstream nav throttle
  // init.
  content::WebContents::CreateParams params(&context);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(params);
  content::MockNavigationHandle handle(web_contents.get());
  handle.set_url(GURL(kTestUrl));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        android_webview::features::kWebViewContentRestrictionSupport);
    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    client_.CreateThrottlesForNavigation(registry);

    EXPECT_FALSE(
        registry.ContainsHeldThrottle(kNavigationThrottleNameForLogging));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        android_webview::features::kWebViewContentRestrictionSupport);
    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    client_.CreateThrottlesForNavigation(registry);

    EXPECT_TRUE(
        registry.ContainsHeldThrottle(kNavigationThrottleNameForLogging));
  }

  // Wait until all pending tasks are processed before the browser context is
  // destroyed.
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace android_webview
