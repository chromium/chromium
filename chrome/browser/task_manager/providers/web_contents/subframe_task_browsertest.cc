// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace task_manager {

namespace {

// URL of a test page on a.com that has two cross-site iframes to b.com and
// c.com.
const char kCrossSitePageUrl[] = "/cross-site/a.com/iframe_cross_site.html";

// URL of a test page on a.com that has no cross-site iframes.
const char kSimplePageUrl[] = "/cross-site/a.com/title2.html";

std::string PrefixExpectedBFCacheTitle(const std::string& title,
                                       bool is_subframe) {
  const auto msg_id = is_subframe
                          ? IDS_TASK_MANAGER_BACK_FORWARD_CACHE_SUBFRAME_PREFIX
                          : IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX;
  return l10n_util::GetStringFUTF8(msg_id, base::UTF8ToUTF16(title));
}

std::string PrefixExpectedTabTitle(const std::string& title) {
  return l10n_util::GetStringFUTF8(IDS_TASK_MANAGER_TAB_PREFIX,
                                   base::UTF8ToUTF16(title));
}

std::string PrefixExpectedTabIncognitoTitle(const std::string& title) {
  return l10n_util::GetStringFUTF8(IDS_TASK_MANAGER_TAB_INCOGNITO_PREFIX,
                                   base::UTF8ToUTF16(title));
}

std::string PrefixExpectedSubframeTitle(const std::string& title) {
  return l10n_util::GetStringFUTF8(IDS_TASK_MANAGER_SUBFRAME_PREFIX,
                                   base::UTF8ToUTF16(title));
}

std::string PrefixExpectedPdfSubframeTitle(const std::string& title) {
  return l10n_util::GetStringFUTF8(IDS_TASK_MANAGER_PDF_SUBFRAME_PREFIX,
                                   base::UTF8ToUTF16(title));
}

std::string PrefixExpectedPdfSubframeIncognitoTitle(const std::string& title) {
  return l10n_util::GetStringFUTF8(
      IDS_TASK_MANAGER_PDF_SUBFRAME_INCOGNITO_PREFIX, base::UTF8ToUTF16(title));
}

// Filter out tool tasks.
std::vector<raw_ptr<Task, VectorExperimental>> NonToolTasks(
    std::vector<raw_ptr<Task, VectorExperimental>> tasks) {
  std::u16string tool_prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TOOL_PREFIX, u"");

  std::vector<raw_ptr<Task, VectorExperimental>> non_tool_tasks;
  std::ranges::copy_if(tasks, std::back_inserter(non_tool_tasks),
                       [&](const auto& task) {
                         return !task->title().starts_with(tool_prefix);
                       });
  return non_tool_tasks;
}

}  // namespace

// A test for OOPIFs and how they show up in the task manager as
// SubframeTasks.
class SubframeTaskBrowserTest : public InProcessBrowserTest {
 public:
  SubframeTaskBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features*/ {omnibox::kWebUIOmniboxPopup,
                              omnibox::internal::kWebUIOmniboxAimPopup},
        /*disabled_features*/ {});
  }
  SubframeTaskBrowserTest(const SubframeTaskBrowserTest&) = delete;
  SubframeTaskBrowserTest& operator=(const SubframeTaskBrowserTest&) = delete;
  ~SubframeTaskBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void NavigateTo(const char* page_url) const {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(page_url)));
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Makes sure that, if sites are isolated, the task manager will show the
// expected SubframeTasks, and they will be shown as running on different
// processes as expected.
IN_PROC_BROWSER_TEST_F(SubframeTaskBrowserTest, TaskManagerShowsSubframeTasks) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();

  // Currently only the about:blank page.
  ASSERT_THAT(MockWebContentsTaskManager::TaskTitles(
                  NonToolTasks(task_manager.tasks())),
              testing::ElementsAre(PrefixExpectedTabTitle("about:blank")));
  NavigateTo(kCrossSitePageUrl);

  // Whether sites are isolated or not, we expect to have at least one tab
  // contents task.
  auto tasks = NonToolTasks(task_manager.tasks());
  ASSERT_GE(tasks.size(), 1u);
  const Task* cross_site_task = tasks[0];
  EXPECT_EQ(cross_site_task->GetType(), Task::RENDERER);

  if (!content::AreAllSitesIsolatedForTesting()) {
    ASSERT_THAT(
        MockWebContentsTaskManager::TaskTitles(tasks),
        testing::ElementsAre(PrefixExpectedTabTitle("cross-site iframe test")));
  } else {
    // Sites are isolated. We expect, in addition to the above task, two more
    // SubframeTasks, one for b.com and another for c.com.
    ASSERT_THAT(
        MockWebContentsTaskManager::TaskTitles(tasks),
        testing::ElementsAre(PrefixExpectedTabTitle("cross-site iframe test"),
                             PrefixExpectedSubframeTitle("http://b.com/"),
                             PrefixExpectedSubframeTitle("http://c.com/")));

    const Task* subframe_task_1 = tasks[1];
    const Task* subframe_task_2 = tasks[2];
    EXPECT_EQ(subframe_task_1->GetType(), Task::RENDERER);
    EXPECT_EQ(subframe_task_2->GetType(), Task::RENDERER);

    // All tasks must be running on different processes.
    EXPECT_NE(subframe_task_1->process_id(), subframe_task_2->process_id());
    EXPECT_NE(subframe_task_1->process_id(), cross_site_task->process_id());
    EXPECT_NE(subframe_task_2->process_id(), cross_site_task->process_id());
  }

  // If we navigate to the simple page on a.com which doesn't have cross-site
  // iframes, we expect not to have any SubframeTasks, except if the previous
  // page is saved in the back-forward cache.
  NavigateTo(kSimplePageUrl);

  tasks = NonToolTasks(task_manager.tasks());
  ASSERT_EQ(
      tasks.size(),
      content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ? 4u : 1u);

  // Main page and two cross-origin iframes.
  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    ASSERT_THAT(
        MockWebContentsTaskManager::TaskTitles(tasks),
        testing::ElementsAre(
            PrefixExpectedBFCacheTitle("http://a.com/", /*is_subframe=*/false),
            PrefixExpectedBFCacheTitle("http://b.com/",
                                       /*is_subframe=*/true),
            PrefixExpectedBFCacheTitle("http://c.com/",
                                       /*is_subframe=*/true),
            PrefixExpectedTabTitle("Title Of Awesomeness")));
  }
  // When navigation to |kSimplePageUrl| happens, tasks are first created for
  // page a.com and two cross-origin iframes b.com and c.com from
  // |RenderFrameHostStateChange|, then the task for |kSimplePageUrl| is created
  // from |DidFinishNavigation| when the navigation completes. Thus |.back()|.
  const Task* simple_page_task = tasks.back();
  EXPECT_EQ(simple_page_task->GetType(), Task::RENDERER);
  EXPECT_EQ(base::UTF16ToUTF8(simple_page_task->title()),
            PrefixExpectedTabTitle("Title Of Awesomeness"));
}

// Allows listening to unresponsive task events.
class HungWebContentsTaskManager : public MockWebContentsTaskManager {
 public:
  HungWebContentsTaskManager() : unresponsive_task_(nullptr) {}
  void TaskUnresponsive(Task* task) override { unresponsive_task_ = task; }

  Task* unresponsive_task() { return unresponsive_task_; }

 private:
  raw_ptr<Task, DanglingUntriaged> unresponsive_task_;
};

// If sites are isolated, makes sure that subframe tasks can react to
// unresponsive renderers.
IN_PROC_BROWSER_TEST_F(SubframeTaskBrowserTest, TaskManagerHungSubframe) {
  // This test only makes sense if we have subframe processes.
  if (!content::AreAllSitesIsolatedForTesting())
    return;

  HungWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();

  NavigateTo(kCrossSitePageUrl);

  // We expect SubframeTasks for b.com and c.com, in either order.
  auto tasks = NonToolTasks(task_manager.tasks());
  ASSERT_THAT(MockWebContentsTaskManager::TaskTitles(tasks),
              testing::ElementsAre(
                  PrefixExpectedTabTitle("cross-site iframe test").c_str(),
                  PrefixExpectedSubframeTitle("http://b.com/"),
                  PrefixExpectedSubframeTitle("http://c.com/")));

  const Task* subframe_task_1 = tasks[1];
  const Task* subframe_task_2 = tasks[2];
  EXPECT_EQ(subframe_task_1->GetType(), Task::RENDERER);
  EXPECT_EQ(subframe_task_2->GetType(), Task::RENDERER);

  // Nothing should have hung yet.
  EXPECT_EQ(task_manager.unresponsive_task(), nullptr);

  // Simulate a hang in one of the subframe processes.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* subframe1 = ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(subframe1);
  SimulateUnresponsiveRenderer(web_contents,
                               subframe1->GetView()->GetRenderWidgetHost());

  // Verify task_observer saw one of the two subframe tasks.  (There's a race,
  // so it could be either one.)
  Task* unresponsive_task = task_manager.unresponsive_task();
  EXPECT_NE(unresponsive_task, nullptr);
  EXPECT_TRUE(unresponsive_task == subframe_task_1 ||
              unresponsive_task == subframe_task_2);
}

#if BUILDFLAG(ENABLE_PDF)
class SubframeTaskPDFBrowserTest : public base::test::WithFeatureOverride,
                                   public PDFExtensionTestBase {
 public:
  SubframeTaskPDFBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const override { return GetParam(); }

  // PDFExtensionTestBase:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({omnibox::kWebUIOmniboxPopup, {}});
    enabled.push_back({omnibox::internal::kWebUIOmniboxAimPopup, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_P(SubframeTaskPDFBrowserTest,
                       TaskManagerShowsPDFSubframeTask) {
  MockWebContentsTaskManager task_manager;
  task_manager.StartObserving();

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));

  // The PDF viewer has 3 frames: the main embedder frame, the PDF extension
  // frame, and the PDF content frame.

  // The PDF content frame will exclude port except when origin isolation is
  // enabled.
  GURL server_url = embedded_test_server()->base_url();
  GURL::Replacements clear_port;
  clear_port.ClearPort();
  GURL server_url_without_port = server_url.ReplaceComponents(clear_port);

  auto tasks = NonToolTasks(task_manager.tasks());
  ASSERT_THAT(
      MockWebContentsTaskManager::TaskTitles(tasks),
      testing::ElementsAre(
          PrefixExpectedTabTitle(
              embedded_test_server()->GetURL("/pdf/test.pdf").GetContent()),
          testing::_,
          testing::AnyOf(
              PrefixExpectedPdfSubframeTitle(server_url.spec()),
              PrefixExpectedPdfSubframeTitle(server_url_without_port.spec()))));

  EXPECT_EQ(tasks[2]->GetType(), Task::RENDERER);
}

IN_PROC_BROWSER_TEST_P(SubframeTaskPDFBrowserTest,
                       TaskManagerShowsIncognitoPDFSubframeTask) {
  MockWebContentsTaskManager task_manager;
  task_manager.StartObserving();

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser, embedded_test_server()->GetURL("/pdf/test.pdf")));
  ASSERT_TRUE(EnsureFullPagePDFHasLoadedWithValidFrameTree(
      incognito_browser->tab_strip_model()->GetActiveWebContents(),
      /*allow_multiple_frames=*/false));

  // There are 4 tasks. There is an about:blank frame from the first browser.
  // Then, the PDF viewer in the incognito browser has 3 frames: the main
  // embedder frame, the PDF extension frame, and the PDF content frame.

  // The PDF content frame will exclude port except when origin isolation is
  // enabled.
  GURL server_url = embedded_test_server()->base_url();
  GURL::Replacements clear_port;
  clear_port.ClearPort();
  GURL server_url_without_port = server_url.ReplaceComponents(clear_port);

  auto tasks = NonToolTasks(task_manager.tasks());
  ASSERT_THAT(
      MockWebContentsTaskManager::TaskTitles(tasks),
      testing::ElementsAre(
          PrefixExpectedTabTitle("about:blank"),
          PrefixExpectedTabIncognitoTitle(
              embedded_test_server()->GetURL("/pdf/test.pdf").GetContent()),
          testing::_,
          // The PDF content frame should have the incognito PDF subframe
          // prefix.
          testing::AnyOf(
              PrefixExpectedPdfSubframeIncognitoTitle(server_url.spec()),
              PrefixExpectedPdfSubframeIncognitoTitle(
                  server_url_without_port.spec()))));
  EXPECT_EQ(tasks[3]->GetType(), Task::RENDERER);
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SubframeTaskPDFBrowserTest);
#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace task_manager
