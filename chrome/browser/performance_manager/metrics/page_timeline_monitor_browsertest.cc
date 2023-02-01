// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager::metrics {

class PageTimelineMonitorBrowserTest : public InProcessBrowserTest {
 public:
  PageTimelineMonitorBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kPageTimelineMonitor);
  }

  void SetUpOnMainThread() override { SetUpOnPMSequence(); }

  void SetUpOnPMSequence() {
    PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce([](Graph* graph) {
          auto* monitor = graph->GetRegisteredObjectAs<PageTimelineMonitor>();
          monitor->SetShouldCollectSliceCallbackForTesting(
              base::BindRepeating([]() { return true; }));
        }));
  }

  // Triggers slice collection on the PM sequence, validates that
  // `expected_entries_count` UKM entries have been recorded, then tries to find
  // a UKM entry in a state matching `expected_entry_state`. Finding the entry
  // is necessary because the order in which the entries are recorded is not
  // deterministic. Returns the `tab_id` field of the found entry.
  int CollectSliceOnPMSequence(size_t expected_entries_count,
                               int64_t expected_entry_state) {
    int tab_id = 0;
    base::RunLoop run_loop;
    PerformanceManager::CallOnGraph(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure quit_closure, size_t expected_entries_count,
               int64_t expected_entry_state, int* tab_id, Graph* graph) {
              ukm::TestAutoSetUkmRecorder ukm_recorder;

              auto* monitor =
                  graph->GetRegisteredObjectAs<PageTimelineMonitor>();
              monitor->CollectSlice();

              auto entries = ukm_recorder.GetEntriesByName(
                  ukm::builders::PerformanceManager_PageTimelineState::
                      kEntryName);
              EXPECT_EQ(entries.size(), expected_entries_count);

              for (size_t i = 0; i < expected_entries_count; ++i) {
                if (*(ukm_recorder.GetEntryMetric(
                        entries[i], "CurrentState")) == expected_entry_state) {
                  *tab_id = *(ukm_recorder.GetEntryMetric(entries[i], "TabId"));
                  break;
                }
              }

              // Getting here with `tab_id` == 0 means there was not entry that
              // matched the expected value.
              EXPECT_NE(*tab_id, 0);

              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), expected_entries_count,
            expected_entry_state, &tab_id));
    run_loop.Run();

    return tab_id;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageTimelineMonitorBrowserTest,
                       TestDiscardedTabsRecordedInCorrectState) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WindowedNotificationObserver load(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  content::OpenURLParams page(embedded_test_server()->GetURL("/title1.html"),
                              content::Referrer(),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_TYPED, false);
  auto* contents = browser()->OpenURL(page);
  load.Wait();

  // Expect 2 entries (one for the background and one for the foreground page).
  // The background page is what we care about, it should be in the "background"
  // state.
  int tab_id = CollectSliceOnPMSequence(2UL, /*background*/ 2);

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node,
             base::OnceClosure quit_closure) {
            EXPECT_TRUE(page_node);

            mechanism::PageDiscarder discarder;
            discarder.DiscardPageNodes(
                {page_node.get()},
                ::mojom::LifecycleUnitDiscardReason::PROACTIVE,
                base::BindOnce(
                    [](base::OnceClosure quit_closure, bool success) {
                      EXPECT_TRUE(success);
                      std::move(quit_closure).Run();
                    },
                    std::move(quit_closure)));
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          std::move(quit_closure)));
  run_loop.Run();

  auto* new_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents->WasDiscarded());

  // Expect new 2 entries (1 for the background and 1 for the foreground page).
  // The background page is what we care about, it should be in the "discarded"
  // state. The discarded tab should also have the same tab id it had before it
  // was discarded.
  EXPECT_EQ(tab_id, CollectSliceOnPMSequence(2UL, /*discarded*/ 5));
}

IN_PROC_BROWSER_TEST_F(PageTimelineMonitorBrowserTest,
                       TestFrozenToDiscardedTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WindowedNotificationObserver load(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  content::OpenURLParams page(embedded_test_server()->GetURL("/title1.html"),
                              content::Referrer(),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_TYPED, false);
  auto* contents = browser()->OpenURL(page);
  load.Wait();

  // Expect 2 entries (one for the background and one for the foreground page).
  // The background page is what we care about, it should be in the "background"
  // state.
  int tab_id = CollectSliceOnPMSequence(2UL, /*background*/ 2);

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node,
             base::OnceClosure quit_closure) {
            EXPECT_TRUE(page_node);

            static_cast<PageNodeImpl*>(page_node.get())
                ->GetMainFrameNodeImpl()
                ->SetLifecycleState(mojom::LifecycleState::kFrozen);

            mechanism::PageDiscarder discarder;
            discarder.DiscardPageNodes(
                {page_node.get()},
                ::mojom::LifecycleUnitDiscardReason::PROACTIVE,
                base::BindOnce(
                    [](base::OnceClosure quit_closure, bool success) {
                      EXPECT_TRUE(success);
                      std::move(quit_closure).Run();
                    },
                    std::move(quit_closure)));
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          std::move(quit_closure)));
  run_loop.Run();

  auto* new_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents->WasDiscarded());

  // Expect new 2 entries (1 for the background and 1 for the foreground page).
  // The background page is what we care about, it should be in the "discarded"
  // state. The discarded tab should also have the same tab id it had before it
  // was discarded.
  EXPECT_EQ(tab_id, CollectSliceOnPMSequence(2UL, /*discarded*/ 5));
}

}  // namespace performance_manager::metrics
