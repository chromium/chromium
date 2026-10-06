// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end tests for process priority. Each scenario asserts what priority
// the user should get (e.g. "a page loading in a background tab is at least
// kUserVisible"), independently of which voters implement it.

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using Priority = base::Process::Priority;

const char* PriorityToString(Priority priority) {
  switch (priority) {
    case Priority::kBestEffort:
      return "kBestEffort";
    case Priority::kUserVisible:
      return "kUserVisible";
    case Priority::kUserBlocking:
      return "kUserBlocking";
  }
}

// A top-level document is loading. kLoadingTimedOut is included: the
// navigation hasn't committed after a timeout, but the load is still in
// progress and the user is still waiting for it.
bool IsLoading(PageNode::LoadingState loading_state) {
  return loading_state == PageNode::LoadingState::kLoading ||
         loading_state == PageNode::LoadingState::kLoadingTimedOut;
}

// Records, for the whole graph, the priority of every process and the events
// needed to figure out after the fact which process was relevant to a page.
// This allows scenarios to start before the relevant process exists (e.g. a
// navigation that will commit in a process that isn't created yet).
//
// Events are grouped into "batches", one per task that recorded events.
// Priority changes within a single task are not observable outside of it (the
// priority is applied to the OS process asynchronously), so the priority of a
// process during a batch is its value at the end of that batch. This makes the
// results independent of the order in which graph observers are notified.
class ProcessPriorityRecorder : public PageNodeObserver,
                                public FrameNodeObserver,
                                public ProcessNodeObserver {
 public:
  explicit ProcessPriorityRecorder(Graph* graph) : graph_(graph) {
    // Snapshot the existing state, then record changes.
    for (const ProcessNode* process_node : graph_->GetAllProcessNodes()) {
      OnProcessNodeAdded(process_node);
    }
    for (const PageNode* page_node : graph_->GetAllPageNodes()) {
      OnPageNodeAdded(page_node);
    }
    for (const FrameNode* frame_node : graph_->GetAllFrameNodes()) {
      OnFrameNodeAdded(frame_node);
    }
    graph_->AddProcessNodeObserver(this);
    graph_->AddPageNodeObserver(this);
    graph_->AddFrameNodeObserver(this);
  }

  ProcessPriorityRecorder(const ProcessPriorityRecorder&) = delete;
  ProcessPriorityRecorder& operator=(const ProcessPriorityRecorder&) = delete;

  ~ProcessPriorityRecorder() override {
    graph_->RemoveFrameNodeObserver(this);
    graph_->RemovePageNodeObserver(this);
    graph_->RemoveProcessNodeObserver(this);
  }

  // Returns the lowest priority of the process hosting the main frame of
  // `contents` during its most recent load, starting from when that process
  // began hosting the page. Returns nullopt if that process never hosted the
  // page while it was loading.
  std::optional<Priority> GetMinPriorityDuringLastLoad(
      content::WebContents* contents) const {
    std::optional<Priority> min_priority;
    for (const Sample& sample : GetSamplesDuringLastLoad(contents)) {
      if (!min_priority || sample.priority < *min_priority) {
        min_priority = sample.priority;
      }
    }
    return min_priority;
  }

  // Returns the highest priority of the process hosting the main frame of
  // `contents` during its most recent load, starting from when that process
  // began hosting the page. Returns nullopt if that process never hosted the
  // page while it was loading.
  std::optional<Priority> GetMaxPriorityDuringLastLoad(
      content::WebContents* contents) const {
    std::optional<Priority> max_priority;
    for (const Sample& sample : GetSamplesDuringLastLoad(contents)) {
      if (!max_priority || sample.priority > *max_priority) {
        max_priority = sample.priority;
      }
    }
    return max_priority;
  }

  // Returns a human-readable description of the samples used by
  // GetMinPriorityDuringLastLoad() and GetMaxPriorityDuringLastLoad(), for
  // failure messages.
  std::string DescribeLastLoad(content::WebContents* contents) const {
    std::string description = "Priority during last load (batch: priority):";
    for (const Sample& sample : GetSamplesDuringLastLoad(contents)) {
      base::StrAppend(&description, {" ", base::NumberToString(sample.batch),
                                     ": ", PriorityToString(sample.priority)});
    }
    return description;
  }

  // ProcessNodeObserver:
  void OnProcessNodeAdded(const ProcessNode* process_node) override {
    const int process_id = next_id_++;
    process_ids_[process_node] = process_id;
    RecordEvent({.type = Event::Type::kPriority,
                 .process_id = process_id,
                 .priority = process_node->GetPriority()});
  }
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override {
    process_ids_.erase(process_node);
  }
  void OnPriorityChanged(const ProcessNode* process_node,
                         Priority previous_value) override {
    RecordEvent({.type = Event::Type::kPriority,
                 .process_id = process_ids_.at(process_node),
                 .priority = process_node->GetPriority()});
  }

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override {
    const int page_id = next_id_++;
    page_ids_[page_node] = page_id;
    RecordEvent({.type = Event::Type::kLoadingState,
                 .page_id = page_id,
                 .is_loading = IsLoading(page_node->GetLoadingState())});
  }
  void OnBeforePageNodeRemoved(const PageNode* page_node) override {
    page_ids_.erase(page_node);
  }
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override {
    RecordEvent({.type = Event::Type::kLoadingState,
                 .page_id = page_ids_.at(page_node),
                 .is_loading = IsLoading(page_node->GetLoadingState())});
  }

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override {
    if (!frame_node->IsMainFrame()) {
      return;
    }
    RecordEvent({.type = Event::Type::kMainFrameAdded,
                 .page_id = page_ids_.at(frame_node->GetPageNode()),
                 .process_id = process_ids_.at(frame_node->GetProcessNode())});
  }

 private:
  struct Event {
    enum class Type {
      // The priority of `process_id` is now `priority`.
      kPriority,
      // `page_id` is now loading or not, according to `is_loading`.
      kLoadingState,
      // A main frame of `page_id` was added in `process_id`.
      kMainFrameAdded,
    };
    Type type;
    int page_id = -1;
    int process_id = -1;
    Priority priority = Priority::kBestEffort;
    bool is_loading = false;
    // Set by RecordEvent().
    int batch = -1;
  };

  // The priority of a process at the end of a batch.
  struct Sample {
    int batch;
    Priority priority;
  };

  void RecordEvent(Event event) {
    event.batch = current_batch_;
    events_.push_back(event);
    if (!batch_end_pending_) {
      batch_end_pending_ = true;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ProcessPriorityRecorder::EndBatch,
                                    weak_factory_.GetWeakPtr()));
    }
  }

  void EndBatch() {
    ++current_batch_;
    batch_end_pending_ = false;
  }

  std::vector<Sample> GetSamplesDuringLastLoad(
      content::WebContents* contents) const {
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
    CHECK(page_node);
    const int page_id = page_ids_.at(page_node.get());
    const FrameNode* main_frame_node = page_node->GetPrimaryMainFrameNode();
    CHECK(main_frame_node);
    const int process_id = process_ids_.at(main_frame_node->GetProcessNode());

    // Find the batches of the last load: [load_start, load_end). If the page
    // is still loading, the load extends past the last recorded batch.
    std::optional<int> load_start;
    int load_end = current_batch_ + 1;
    bool was_loading = false;
    // The first batch in which `process_id` hosted a main frame of the page.
    std::optional<int> hosting_start;
    for (const Event& event : events_) {
      if (event.type == Event::Type::kLoadingState &&
          event.page_id == page_id) {
        if (!was_loading && event.is_loading) {
          load_start = event.batch;
          load_end = current_batch_ + 1;
        } else if (was_loading && !event.is_loading) {
          load_end = event.batch;
        }
        was_loading = event.is_loading;
      } else if (event.type == Event::Type::kMainFrameAdded &&
                 event.page_id == page_id && event.process_id == process_id &&
                 !hosting_start) {
        hosting_start = event.batch;
      }
    }
    CHECK(load_start) << "The page never loaded while being recorded.";
    CHECK(hosting_start);

    const int start = std::max(*load_start, *hosting_start);
    if (start >= load_end) {
      return {};
    }

    // The priority at the end of `start`, followed by the priority at the end
    // of every later batch of the load in which it changed.
    std::vector<Sample> samples;
    for (const Event& event : events_) {
      if (event.batch >= load_end) {
        break;
      }
      if (event.type != Event::Type::kPriority ||
          event.process_id != process_id) {
        continue;
      }
      const int batch = std::max(event.batch, start);
      if (!samples.empty() && samples.back().batch == batch) {
        // Only the last value of a batch is observable.
        samples.back().priority = event.priority;
      } else {
        samples.push_back({batch, event.priority});
      }
    }
    return samples;
  }

  const raw_ptr<Graph> graph_;

  // Recorder-assigned IDs for live nodes. Node addresses can be reused after a
  // node is deleted, so they can't be used to identify nodes in `events_`.
  std::map<const PageNode*, int> page_ids_;
  std::map<const ProcessNode*, int> process_ids_;
  int next_id_ = 0;

  std::vector<Event> events_;
  int current_batch_ = 0;
  bool batch_end_pending_ = false;

  base::WeakPtrFactory<ProcessPriorityRecorder> weak_factory_{this};
};

class ProcessPriorityBrowserTest : public InProcessBrowserTest {
 public:
  ProcessPriorityBrowserTest() {
    // Don't depend on the field trial testing config. Remove when
    // kPMLoadingPageVoterV2 is enabled by default.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPMLoadingPageVoterV2},
        /*disabled_features=*/{features::kPMLoadingPageVoter});
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Scenarios load cross-site so that the load happens in a new process,
    // which requires site-per-process on every configuration.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    recorder_ = std::make_unique<ProcessPriorityRecorder>(
        PerformanceManager::GetGraph());
  }

  void TearDownOnMainThread() override {
    recorder_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Waits until the top-level load of `contents` is finished according to
  // its PageNode.
  static void WaitForPageNodeLoaded(content::WebContents* contents) {
    ASSERT_TRUE(content::WaitForLoadStop(contents));
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
    ASSERT_TRUE(page_node);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return page_node->GetLoadingState() !=
                 PageNode::LoadingState::kLoadingNotStarted &&
             !IsLoading(page_node->GetLoadingState());
    }));
  }

  // Expects that the process hosting the main frame of `contents` had at
  // least `expected` priority during its most recent load.
  void ExpectMinPriorityDuringLastLoad(content::WebContents* contents,
                                       Priority expected) {
    std::optional<Priority> min_priority =
        recorder_->GetMinPriorityDuringLastLoad(contents);
    ASSERT_TRUE(min_priority) << recorder_->DescribeLastLoad(contents);
    EXPECT_GE(*min_priority, expected)
        << "Min priority: " << PriorityToString(*min_priority)
        << ", expected at least: " << PriorityToString(expected) << ". "
        << recorder_->DescribeLastLoad(contents);
  }

  // Expects that the process hosting the main frame of `contents` had at
  // most `expected` priority during its most recent load.
  void ExpectMaxPriorityDuringLastLoad(content::WebContents* contents,
                                       Priority expected) {
    std::optional<Priority> max_priority =
        recorder_->GetMaxPriorityDuringLastLoad(contents);
    ASSERT_TRUE(max_priority) << recorder_->DescribeLastLoad(contents);
    EXPECT_LE(*max_priority, expected)
        << "Max priority: " << PriorityToString(*max_priority)
        << ", expected at most: " << PriorityToString(expected) << ". "
        << recorder_->DescribeLastLoad(contents);
  }

  static content::RenderProcessHost* GetMainFrameProcess(
      content::WebContents* contents) {
    return contents->GetPrimaryMainFrame()->GetProcess();
  }

  TabStripModel* tab_strip_model() { return browser()->GetTabStripModel(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ProcessPriorityRecorder> recorder_;
};

// Priorities are still computed on ProcessNodes, but aren't applied to the
// actual processes. This is needed by scenarios that expect a low priority, so
// that the load isn't slowed down by it.
class ProcessPriorityNotAppliedBrowserTest : public ProcessPriorityBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProcessPriorityBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);
  }
};

}  // namespace

// A page loading in the active tab is kUserBlocking for the whole load.
IN_PROC_BROWSER_TEST_F(ProcessPriorityBrowserTest,
                       ActiveTabLoadStaysUserBlocking) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  content::WebContents* contents = tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* initial_process = GetMainFrameProcess(contents);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.test", "/title2.html")));
  WaitForPageNodeLoaded(contents);
  // The load happened in a new process.
  ASSERT_NE(initial_process, GetMainFrameProcess(contents));

  ExpectMinPriorityDuringLastLoad(contents, Priority::kUserBlocking);
}

// A link opened in a background tab with a middle click (same as Ctrl/Cmd +
// click) is at least kUserVisible for the whole load.
IN_PROC_BROWSER_TEST_F(ProcessPriorityBrowserTest,
                       MiddleClickBackgroundTabLoadStaysUserVisible) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  content::WebContents* opener = tab_strip_model()->GetActiveWebContents();
  const GURL link_url =
      embedded_test_server()->GetURL("b.test", "/title2.html");
  ASSERT_TRUE(content::ExecJs(
      opener,
      content::JsReplace(
          "document.body.innerHTML = '<a id=\"link\" href=\"' + $1 + '\" "
          "style=\"display:block;width:100vw;height:100vh\">link</a>';",
          link_url)));
  content::WaitForHitTestData(opener->GetPrimaryMainFrame());

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  content::SimulateMouseClickAt(
      opener, /*modifiers=*/0, blink::WebMouseEvent::Button::kMiddle,
      gfx::ToFlooredPoint(
          content::GetCenterCoordinatesOfElementWithId(opener, "link")));
  content::WebContents* contents = tab_added_waiter.Wait();
  WaitForPageNodeLoaded(contents);

  ASSERT_EQ(opener, tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(link_url, contents->GetLastCommittedURL());
  ASSERT_NE(GetMainFrameProcess(opener), GetMainFrameProcess(contents));

  ExpectMinPriorityDuringLastLoad(contents, Priority::kUserVisible);
}

// A link opened in a background tab from the context menu is at least
// kUserVisible for the whole load.
IN_PROC_BROWSER_TEST_F(ProcessPriorityBrowserTest,
                       ContextMenuBackgroundTabLoadStaysUserVisible) {
  const GURL opener_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  const GURL link_url =
      embedded_test_server()->GetURL("b.test", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener = tab_strip_model()->GetActiveWebContents();

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  std::unique_ptr<TestRenderViewContextMenu> menu =
      TestRenderViewContextMenu::Create(opener, opener_url, link_url);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  content::WebContents* contents = tab_added_waiter.Wait();
  WaitForPageNodeLoaded(contents);

  ASSERT_EQ(opener, tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(link_url, contents->GetLastCommittedURL());
  ASSERT_NE(GetMainFrameProcess(opener), GetMainFrameProcess(contents));

  ExpectMinPriorityDuringLastLoad(contents, Priority::kUserVisible);
}

// A browser-initiated load in an existing background tab (e.g. from an
// extension API) is at least kUserVisible for the whole load.
IN_PROC_BROWSER_TEST_F(ProcessPriorityBrowserTest,
                       BrowserInitiatedBackgroundTabLoadStaysUserVisible) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  content::WebContents* active_contents =
      tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.test", "/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, tab_strip_model()->count());
  content::WebContents* contents = tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(active_contents, contents);
  WaitForPageNodeLoaded(contents);
  content::RenderProcessHost* initial_process = GetMainFrameProcess(contents);

  content::TestNavigationObserver navigation_observer(contents);
  contents->GetController().LoadURL(
      embedded_test_server()->GetURL("c.test", "/title2.html"),
      content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  navigation_observer.Wait();
  WaitForPageNodeLoaded(contents);

  ASSERT_EQ(active_contents, tab_strip_model()->GetActiveWebContents());
  ASSERT_NE(initial_process, GetMainFrameProcess(contents));

  ExpectMinPriorityDuringLastLoad(contents, Priority::kUserVisible);
}

// A link opened in a background tab from the context menu, that goes through a
// client redirect (e.g. a link shortener), is at least kUserVisible for the
// whole load of the destination.
IN_PROC_BROWSER_TEST_F(ProcessPriorityBrowserTest,
                       ClientRedirectBackgroundTabLoadStaysUserVisible) {
  const GURL opener_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  const GURL destination_url =
      embedded_test_server()->GetURL("c.test", "/title2.html");
  const GURL link_url = embedded_test_server()->GetURL(
      "b.test", "/client-redirect?" + destination_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener = tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver destination_observer(destination_url);
  destination_observer.StartWatchingNewWebContents();
  std::unique_ptr<TestRenderViewContextMenu> menu =
      TestRenderViewContextMenu::Create(opener, opener_url, link_url);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);
  destination_observer.Wait();
  ASSERT_EQ(2, tab_strip_model()->count());
  content::WebContents* contents = tab_strip_model()->GetWebContentsAt(1);
  WaitForPageNodeLoaded(contents);

  ASSERT_EQ(opener, tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(destination_url, contents->GetLastCommittedURL());
  ASSERT_NE(GetMainFrameProcess(opener), GetMainFrameProcess(contents));

  ExpectMinPriorityDuringLastLoad(contents, Priority::kUserVisible);
}

// A load in a background tab that neither the user nor the browser asked for
// (a renderer-initiated navigation without user activation, that isn't a client
// redirect) isn't boosted.
IN_PROC_BROWSER_TEST_F(ProcessPriorityNotAppliedBrowserTest,
                       UnsolicitedBackgroundTabLoadIsNotBoosted) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title1.html")));
  content::WebContents* active_contents =
      tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("b.test", "/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, tab_strip_model()->count());
  content::WebContents* contents = tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(active_contents, contents);
  WaitForPageNodeLoaded(contents);
  content::RenderProcessHost* initial_process = GetMainFrameProcess(contents);

  // Move past the window in which a navigation is considered a client redirect
  // of the current (browser-initiated) document.
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  tick_clock.Advance(base::Seconds(11));
  PerformanceManagerTabHelper* tab_helper =
      PerformanceManagerTabHelper::FromWebContents(contents);
  ASSERT_TRUE(tab_helper);
  tab_helper->SetTickClockForTesting(&tick_clock);
  base::ScopedClosureRunner reset_tick_clock(
      base::BindOnce(&PerformanceManagerTabHelper::SetTickClockForTesting,
                     base::Unretained(tab_helper),
                     static_cast<const base::TickClock*>(nullptr)));

  content::TestNavigationObserver navigation_observer(contents);
  ASSERT_TRUE(content::ExecJs(
      contents,
      content::JsReplace("location.href = $1;", embedded_test_server()->GetURL(
                                                    "c.test", "/title2.html")),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  navigation_observer.Wait();
  WaitForPageNodeLoaded(contents);

  ASSERT_EQ(active_contents, tab_strip_model()->GetActiveWebContents());
  ASSERT_NE(initial_process, GetMainFrameProcess(contents));

  ExpectMaxPriorityDuringLastLoad(contents, Priority::kBestEffort);
}

}  // namespace performance_manager
