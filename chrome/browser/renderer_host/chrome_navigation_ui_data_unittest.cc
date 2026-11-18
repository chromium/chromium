// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"

#include <memory>
#include <vector>

#include "base/test/gtest_util.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_navigation_ui_data.h"
#endif

namespace {

using ChromeNavigationUIDataTest = ChromeRenderViewHostTestHarness;

// Verifies `ChromeNavigationUIData::CloneUnboundOrNew()` handles null and
// unbound inputs and `CHECK`-fails if passed an already-bound or loader-mutated
// instance.
TEST_F(ChromeNavigationUIDataTest, CloneUnboundOrNew) {
  constexpr int64_t kBookmarkId = 77;
  std::unique_ptr<ChromeNavigationUIData> from_null =
      ChromeNavigationUIData::CloneUnboundOrNew(nullptr);
  ASSERT_NE(nullptr, from_null);
  EXPECT_FALSE(from_null->is_bound_for_testing());
  EXPECT_EQ(std::nullopt, from_null->bookmark_id());

  auto source = std::make_unique<ChromeNavigationUIData>();
  source->set_bookmark_id(kBookmarkId);
  source->set_is_using_https_as_default_scheme(true);
  source->set_force_no_https_upgrade(true);
  source->set_navigation_initiated_from_sync(true);

  std::unique_ptr<ChromeNavigationUIData> cloned_unbound =
      ChromeNavigationUIData::CloneUnboundOrNew(source.get());
  ASSERT_NE(nullptr, cloned_unbound);
  EXPECT_NE(source.get(), cloned_unbound.get());
  EXPECT_FALSE(cloned_unbound->is_bound_for_testing());
  EXPECT_EQ(kBookmarkId, cloned_unbound->bookmark_id());
  EXPECT_TRUE(cloned_unbound->is_using_https_as_default_scheme());
  EXPECT_TRUE(cloned_unbound->force_no_https_upgrade());
  EXPECT_TRUE(cloned_unbound->navigation_initiated_from_sync());

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/"), web_contents());
  navigation->Start();
  content::NavigationHandle* handle = navigation->GetNavigationHandle();
  ASSERT_NE(nullptr, handle);
  ChromeNavigationUIData bound_source(handle);
  ASSERT_TRUE(bound_source.is_bound_for_testing());
  EXPECT_CHECK_DEATH(ChromeNavigationUIData::CloneUnboundOrNew(&bound_source));

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  ChromeNavigationUIData offline_source;
  offline_source.SetOfflinePageNavigationUIData(
      offline_pages::OfflinePageNavigationUIData(/*is_offline_page=*/true));
  EXPECT_CHECK_DEATH(
      ChromeNavigationUIData::CloneUnboundOrNew(&offline_source));
#endif
}

// Verifies the `ChromeNavigationUIData(content::NavigationHandle*)` constructor
// and `is_no_state_prefetching()` across `Clone()`.
TEST_F(ChromeNavigationUIDataTest,
       HandleConstructorAndNoStatePrefetchEnvironmentData) {
  constexpr int64_t kBookmarkId = 123;
  const GURL kPrefetchUrl("https://example.com/prefetch");
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, no_state_prefetch_manager);

  std::unique_ptr<prerender::NoStatePrefetchHandle> prefetch_handle =
      no_state_prefetch_manager->StartPrefetchingFromLinkRelPrerender(
          /*process_id=*/-1, /*route_id=*/-1, kPrefetchUrl,
          blink::mojom::PrerenderTriggerType::kLinkRelPrerender,
          content::Referrer(), url::Origin::Create(kPrefetchUrl),
          /*initiator_navigation_state=*/nullptr, gfx::Size(640, 480));
  const std::vector<content::WebContents*> prefetch_contents =
      no_state_prefetch_manager->GetAllNoStatePrefetchingContentsForTesting();
  ASSERT_EQ(1u, prefetch_contents.size());
  content::WebContents* prefetch_web_contents = prefetch_contents[0];
  ASSERT_NE(nullptr, prefetch_web_contents);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      kPrefetchUrl, prefetch_web_contents);
  navigation->Start();
  content::NavigationHandle* handle = navigation->GetNavigationHandle();
  ASSERT_NE(nullptr, handle);

  ChromeNavigationUIData handle_constructed(handle);
  handle_constructed.set_bookmark_id(kBookmarkId);
  EXPECT_TRUE(handle_constructed.is_bound_for_testing());
  EXPECT_TRUE(handle_constructed.is_no_state_prefetching());
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  ASSERT_NE(nullptr, handle_constructed.GetExtensionNavigationUIData());
  EXPECT_EQ(
      prefetch_web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      handle_constructed.GetExtensionNavigationUIData()->frame_tree_node_id());
#endif

  std::unique_ptr<content::NavigationUIData> cloned =
      handle_constructed.Clone();
  const auto* cloned_chrome =
      static_cast<const ChromeNavigationUIData*>(cloned.get());
  EXPECT_TRUE(cloned_chrome->is_bound_for_testing());
  EXPECT_EQ(kBookmarkId, cloned_chrome->bookmark_id());
  EXPECT_TRUE(cloned_chrome->is_no_state_prefetching());

  no_state_prefetch_manager->CancelAllPrerenders();
}

}  // namespace
