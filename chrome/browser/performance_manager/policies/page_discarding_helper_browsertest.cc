// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/performance_manager/policies/freezing_opt_out_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-forward.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace performance_manager::policies {

namespace {

using DiscardReason = PageDiscardingHelper::DiscardReason;

class FaviconWatcher final : public content::WebContentsObserver {
 public:
  explicit FaviconWatcher(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~FaviconWatcher() final = default;

  FaviconWatcher(const FaviconWatcher&) = delete;
  FaviconWatcher& operator=(const FaviconWatcher&) = delete;

  void Wait() { run_loop_.Run(); }

 private:
  // WebContentsObserver
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) final {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

// Waits for `page_node` to transition to the LoadingState::kLoadedIdle state.
class PageNodeIdleWaiter : public PageNodeObserver,
                           public GraphOwnedDefaultImpl {
 public:
  explicit PageNodeIdleWaiter(base::WeakPtr<PageNode> page_node)
      : page_node_(page_node) {
    PerformanceManager::GetGraph()->AddPageNodeObserver(this);
  }
  ~PageNodeIdleWaiter() override = default;

  void Wait() { run_loop_.Run(); }

 private:
  // PageNodeObserver:
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override {
    if ((page_node == page_node_.get()) &&
        page_node->GetLoadingState() == PageNode::LoadingState::kLoadedIdle) {
      PerformanceManager::GetGraph()->RemovePageNodeObserver(this);
      run_loop_.Quit();
      return;
    }
  }

  base::RunLoop run_loop_;
  const base::WeakPtr<PageNode> page_node_;
};

// Waits for resource coordinator to register a LifecycleUnitState::FROZEN state
// change.
class TabLifecycleUnitFreezeWaiter
    : public resource_coordinator::TabLifecycleObserver {
 public:
  TabLifecycleUnitFreezeWaiter() {
    resource_coordinator::TabLifecycleUnitExternal::AddTabLifecycleObserver(
        this);
  }
  ~TabLifecycleUnitFreezeWaiter() override {
    resource_coordinator::TabLifecycleUnitExternal::RemoveTabLifecycleObserver(
        this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // resource_coordinator::TabLifecycleObserver:
  void OnTabLifecycleStateChange(
      content::WebContents* contents,
      ::mojom::LifecycleUnitState previous_state,
      ::mojom::LifecycleUnitState new_state,
      std::optional<LifecycleUnitDiscardReason> discard_reason) override {
    if (new_state == ::mojom::LifecycleUnitState::FROZEN) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
};

class PageDiscardingHelperBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  PageDiscardingHelperBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(::features::kWebContentsDiscard,
                                              GetParam());
  }
  ~PageDiscardingHelperBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Opens a new page in the background, and returns its index in the tab strip.
  int OpenNewBackgroundPage() {
    // Load a page with title and favicon so that some tests can manipulate
    // them.
    content::OpenURLParams page(
        embedded_test_server()->GetURL("/favicon/title2_with_favicon.html"),
        content::Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui::PAGE_TRANSITION_TYPED, false);
    content::WebContents* contents =
        browser()->OpenURL(page, /*navigation_handle_callback=*/{});
    content::TestNavigationObserver observer(contents);
    observer.set_expected_initial_url(page.url);

    // Wait for the page and the initial favicon to finish loading.
    FaviconWatcher favicon_watcher(contents);
    observer.Wait();
    favicon_watcher.Wait();

    return browser()->tab_strip_model()->GetIndexOfWebContents(contents);
  }

  void UpdatePageTitle(int index) {
    constexpr char16_t kNewTitle[] = u"New title";
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    content::TitleWatcher title_watcher(contents, kNewTitle);
    ASSERT_TRUE(content::ExecJs(
        contents, base::StrCat({"document.title = '",
                                base::UTF16ToASCII(kNewTitle), "'"})));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), kNewTitle);
  }

  void UpdateFavicon(int index) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    // Change the favicon link from "icon.png" to "icon.svg".
    FaviconWatcher favicon_watcher(contents);
    ASSERT_TRUE(content::ExecJs(
        contents,
        "document.getElementsByTagName('link')[0].href = 'icon.svg'"));
    favicon_watcher.Wait();
  }

  base::WeakPtr<PageNode> GetPageNodeAtIndex(int index) {
    return PerformanceManager::GetPrimaryPageNodeForWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(index));
  }

  void ExpectImmediateDiscard(
      int index,
      DiscardReason discard_reason,
      bool expected_result,
      const base::Location& location = base::Location::Current()) {
    const char* discard_string;
    switch (discard_reason) {
      case DiscardReason::URGENT:
        discard_string = "Urgent";
        break;
      case DiscardReason::PROACTIVE:
        discard_string = "Proactive";
        break;
      case DiscardReason::EXTERNAL:
        discard_string = "External";
        break;
      case DiscardReason::SUGGESTED:
        discard_string = "Suggested";
        break;
      case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
        discard_string = "Frozen with growing memory";
        break;
    }
    SCOPED_TRACE(::testing::Message()
                 << discard_string << " discard from " << location.ToString());

    base::WeakPtr<PageNode> page_node = GetPageNodeAtIndex(index);
    ASSERT_TRUE(page_node);

    Graph* graph = PerformanceManager::GetGraph();
    auto* helper = PageDiscardingHelper::GetFromGraph(graph);
    ASSERT_TRUE(helper);

    std::optional<base::TimeTicks> first_discarded_at =
        helper->ImmediatelyDiscardMultiplePages({page_node.get()},
                                                discard_reason);
    EXPECT_EQ(first_discarded_at.has_value(), expected_result);
    EXPECT_EQ(
        browser()->tab_strip_model()->GetWebContentsAt(index)->WasDiscarded(),
        expected_result);
  }

  void ExpectOptedOutOfDiscardingAndFreezing(
      int index,
      FreezingOptOutChecker& freezing_opt_out_checker,
      bool expect_opted_out) {
    base::WeakPtr<PageNode> page_node = GetPageNodeAtIndex(index);

    ASSERT_TRUE(page_node);
    auto* helper = PageDiscardingHelper::GetFromGraph(page_node->GetGraph());
    ASSERT_TRUE(helper);
    EXPECT_EQ(
        helper->IsPageOptedOutOfDiscarding(page_node->GetBrowserContextID(),
                                           page_node->GetMainFrameUrl()),
        expect_opted_out);
    EXPECT_EQ(
        freezing_opt_out_checker.IsPageOptedOutOfFreezing(
            page_node->GetBrowserContextID(), page_node->GetMainFrameUrl()),
        expect_opted_out);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PageDiscardingHelperBrowserTest, DiscardSpecificPage) {
  // Test urgent and proactive discards in a loop to avoid the overhead of
  // starting a new browser every time.
  // TODO(crbug.com/40899366): Add tests for all the other heuristics in
  // PageDiscardingHelper::CanDiscard().
  for (auto discard_reason :
       {DiscardReason::EXTERNAL, DiscardReason::URGENT,
        DiscardReason::PROACTIVE, DiscardReason::SUGGESTED,
        DiscardReason::FROZEN_WITH_GROWING_MEMORY}) {
    {
      // A background page can be discarded.
      const int index1 = OpenNewBackgroundPage();
      ExpectImmediateDiscard(index1, discard_reason, true);

      // A foreground page blocks URGENT, PROACTIVE and SUGGESTED discards.
      const int index2 = OpenNewBackgroundPage();
      browser()->tab_strip_model()->ActivateTabAt(index2);
      switch (discard_reason) {
        case DiscardReason::EXTERNAL:
        case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
          ExpectImmediateDiscard(index2, discard_reason, true);
          break;
        case DiscardReason::URGENT:
        case DiscardReason::PROACTIVE:
        case DiscardReason::SUGGESTED:
          ExpectImmediateDiscard(index2, discard_reason, false);
          break;
      }
    }

    {
      // Updating title in background blocks PROACTIVE and SUGGESTED discards.
      const int index1 = OpenNewBackgroundPage();
      UpdatePageTitle(index1);
      switch (discard_reason) {
        case DiscardReason::EXTERNAL:
        case DiscardReason::URGENT:
        case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
          ExpectImmediateDiscard(index1, discard_reason, true);
          break;
        case DiscardReason::PROACTIVE:
        case DiscardReason::SUGGESTED:
          ExpectImmediateDiscard(index1, discard_reason, false);
          break;
      }

      // Updating favicon in the foreground does not block discards.
      const int index2 = OpenNewBackgroundPage();
      browser()->tab_strip_model()->ActivateTabAt(index2);
      UpdatePageTitle(index2);
      browser()->tab_strip_model()->ActivateTabAt(index1);
      ExpectImmediateDiscard(index2, discard_reason, true);
    }

    {
      // Updating favicon in background blocks PROACTIVE and SUGGESTED discards.
      const int index1 = OpenNewBackgroundPage();
      UpdateFavicon(index1);
      switch (discard_reason) {
        case DiscardReason::EXTERNAL:
        case DiscardReason::URGENT:
        case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
          ExpectImmediateDiscard(index1, discard_reason, true);
          break;
        case DiscardReason::PROACTIVE:
        case DiscardReason::SUGGESTED:
          ExpectImmediateDiscard(index1, discard_reason, false);
          break;
      }

      // Updating favicon in the foreground does not block discards.
      const int index2 = OpenNewBackgroundPage();
      browser()->tab_strip_model()->ActivateTabAt(index2);
      UpdateFavicon(index2);
      browser()->tab_strip_model()->ActivateTabAt(index1);
      ExpectImmediateDiscard(index2, discard_reason, true);
    }
  }
}

IN_PROC_BROWSER_TEST_P(PageDiscardingHelperBrowserTest, NoDiscardPatterns) {
  const std::string default_browser_context_id =
      browser()->profile()->UniqueId();
  const std::string base_url_pattern =
      embedded_test_server()->base_url().spec();

  // Test urgent and proactive discards in a loop to avoid the overhead of
  // starting a new browser every time.
  for (auto discard_reason :
       {DiscardReason::EXTERNAL, DiscardReason::URGENT,
        DiscardReason::PROACTIVE, DiscardReason::SUGGESTED,
        DiscardReason::FROZEN_WITH_GROWING_MEMORY}) {
    // Also test that FreezingOptOutChecker is hooked up to
    // PageDiscardingHelper correctly.
    base::test::TestFuture<std::string_view> policy_changed_future;
    auto policy_changed_callback = policy_changed_future.GetRepeatingCallback();

    auto* helper =
        PageDiscardingHelper::GetFromGraph(PerformanceManager::GetGraph());
    ASSERT_TRUE(helper);
    std::unique_ptr<FreezingOptOutChecker> freezing_opt_out_checker =
        std::make_unique<FreezingOptOutChecker>(helper->GetWeakPtr());

    helper->SetNoDiscardPatternsForProfile(default_browser_context_id,
                                           {base_url_pattern});

    // The callback wasn't set during SetNoDiscardPatternsForProfile(),
    // which should safely do nothing. Future calls should notify the
    // TestFuture.
    freezing_opt_out_checker->SetOptOutPolicyChangedCallback(
        std::move(policy_changed_callback));

    EXPECT_FALSE(policy_changed_future.IsReady());

    // Background page should be blocked from discarding because its url is
    // in NoDiscardPatterns (URGENT, PROACTIVE and SUGGESTED discards only).
    const int index1 = OpenNewBackgroundPage();
    ExpectOptedOutOfDiscardingAndFreezing(index1, *freezing_opt_out_checker,
                                          true);
    switch (discard_reason) {
      case DiscardReason::EXTERNAL:
      case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
        ExpectImmediateDiscard(index1, discard_reason, true);
        break;
      case DiscardReason::URGENT:
      case DiscardReason::PROACTIVE:
      case DiscardReason::SUGGESTED:
        ExpectImmediateDiscard(index1, discard_reason, false);
        break;
    }

    // Empty pattern list.
    helper->SetNoDiscardPatternsForProfile(default_browser_context_id, {});

    EXPECT_EQ(policy_changed_future.Take(), default_browser_context_id);

    // No longer blocked from discarding. (Need a new page because the first
    // may already be discarded.)
    const int index2 = OpenNewBackgroundPage();
    ExpectOptedOutOfDiscardingAndFreezing(index2, *freezing_opt_out_checker,
                                          false);
    ExpectImmediateDiscard(index2, discard_reason, true);

    // Delete pattern list.
    helper->ClearNoDiscardPatternsForProfile(default_browser_context_id);

    EXPECT_EQ(policy_changed_future.Take(), default_browser_context_id);

    // With no list available, page should be treated as if it's opted out.
    const int index3 = OpenNewBackgroundPage();
    ExpectOptedOutOfDiscardingAndFreezing(index3, *freezing_opt_out_checker,
                                          true);
    switch (discard_reason) {
      case DiscardReason::EXTERNAL:
      case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
        ExpectImmediateDiscard(index3, discard_reason, true);
        break;
      case DiscardReason::URGENT:
      case DiscardReason::PROACTIVE:
      case DiscardReason::SUGGESTED:
        ExpectImmediateDiscard(index3, discard_reason, false);
        break;
    }
  }
}

// Regression test for crbug.com/386801193. Ensure discarded tabs remain
// eligible for successive discard operations following a reactivation / reload.
IN_PROC_BROWSER_TEST_P(PageDiscardingHelperBrowserTest,
                       DiscardedTabEligibleForSuccessiveDiscards) {
  // Add a new background tab.
  OpenNewBackgroundPage();
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  tabs::TabInterface* tab1 = browser()->tab_strip_model()->GetTabAtIndex(0);
  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetTabAtIndex(1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveTab(), tab1);

  // Attempt to discard the background tab.
  const auto attempt_discard = [this]() {
    base::WeakPtr<PageNode> discard_target_page_node = GetPageNodeAtIndex(1);
    ASSERT_TRUE(discard_target_page_node);
    Graph* graph = PerformanceManager::GetGraph();
    auto* helper = PageDiscardingHelper::GetFromGraph(graph);
    ASSERT_TRUE(helper);
    EXPECT_EQ(CanDiscardResult::kEligible,
              helper->CanDiscard(discard_target_page_node.get(),
                                 DiscardReason::URGENT, base::TimeDelta()));
    std::optional<base::TimeTicks> first_discarded_at =
        helper->DiscardAPage(DiscardReason::URGENT, base::TimeDelta());

    EXPECT_TRUE(first_discarded_at.has_value());
  };
  attempt_discard();

  // Ensure the background tab has been discarded.
  EXPECT_FALSE(tab1->GetContents()->WasDiscarded());
  EXPECT_TRUE(tab2->GetContents()->WasDiscarded());

  // Activate and reload the discarded background page.
  content::TestNavigationObserver reload_waiter(tab2->GetContents(), 1);
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveTab(), tab2);
  reload_waiter.Wait();

  EXPECT_FALSE(tab1->GetContents()->WasDiscarded());
  EXPECT_FALSE(tab2->GetContents()->WasDiscarded());

  // Background the discarded tab again and attempt another discard.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveTab(), tab1);
  attempt_discard();

  // Ensure the background tab has been discarded again.
  EXPECT_FALSE(tab1->GetContents()->WasDiscarded());
  EXPECT_TRUE(tab2->GetContents()->WasDiscarded());
}

// Regression test for crbug.com/394242157. Ensure that discarding a frozen tab
// does not result in invalid lifecycle state transitions.
IN_PROC_BROWSER_TEST_P(PageDiscardingHelperBrowserTest,
                       DiscardingFrozenTabCorrectlyTransitionsLifecycleState) {
  // Add a new background tab.
  OpenNewBackgroundPage();
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  tabs::TabInterface* tab1 = browser()->tab_strip_model()->GetTabAtIndex(0);
  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetTabAtIndex(1);

  EXPECT_EQ(browser()->tab_strip_model()->GetActiveTab(), tab1);

  // Ensure the off-thread page node has registered the background tab as idle.
  PageNodeIdleWaiter page_node_idle_waiter(GetPageNodeAtIndex(1));
  page_node_idle_waiter.Wait();

  // Freeze the background tab and ensure the state transition has been
  // registered by the tab lifecycle unit.
  auto freezing_vote =
      std::make_unique<performance_manager::freezing::FreezingVote>(
          tab2->GetContents());

  TabLifecycleUnitFreezeWaiter freeze_waiter;
  freeze_waiter.Wait();

  // Discard the background tab.
  const auto attempt_discard = [this]() {
    base::WeakPtr<PageNode> discard_target_page_node = GetPageNodeAtIndex(1);
    ASSERT_TRUE(discard_target_page_node);
    Graph* graph = PerformanceManager::GetGraph();
    auto* helper = PageDiscardingHelper::GetFromGraph(graph);
    ASSERT_TRUE(helper);
    EXPECT_EQ(CanDiscardResult::kEligible,
              helper->CanDiscard(discard_target_page_node.get(),
                                 DiscardReason::URGENT, base::TimeDelta()));
    std::optional<base::TimeTicks> first_discarded_at =
        helper->DiscardAPage(DiscardReason::URGENT, base::TimeDelta());

    EXPECT_TRUE(first_discarded_at.has_value());
  };
  attempt_discard();

  // Assert the background tab has been successfully discarded.
  EXPECT_FALSE(tab1->GetContents()->WasDiscarded());
  EXPECT_TRUE(tab2->GetContents()->WasDiscarded());

  // Assert the lifecycle state transition is correctly registered in the
  // lifecycle unit.
  auto* lifecycle_unit =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
          tab2->GetContents());
  EXPECT_EQ(::mojom::LifecycleUnitState::DISCARDED,
            lifecycle_unit->GetTabState());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PageDiscardingHelperBrowserTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<
        PageDiscardingHelperBrowserTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

}  // namespace

}  // namespace performance_manager::policies
