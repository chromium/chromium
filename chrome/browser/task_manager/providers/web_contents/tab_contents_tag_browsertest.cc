// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace task_manager {

namespace {

// Defines a test page file path along with its expected task manager reported
// values.
struct TestPageData {
  const char* page_file;
  const char* title;
  Task::Type task_type;
  int expected_prefix_message;
};

// The below test files are available in src/chrome/test/data/
// TODO(afakhry): Add more test pages here as needed (e.g. pages that are hosted
// in the tabs as apps or extensions).
const TestPageData kTestPages[] = {
    {
        "/title1.html",
        "",
        Task::RENDERER,
        IDS_TASK_MANAGER_TAB_PREFIX
    },
    {
        "/title2.html",
        "Title Of Awesomeness",
        Task::RENDERER,
        IDS_TASK_MANAGER_TAB_PREFIX
    },
    {
        "/title3.html",
        "Title Of More Awesomeness",
        Task::RENDERER,
        IDS_TASK_MANAGER_TAB_PREFIX
    },
};

const size_t kTestPagesLength = std::size(kTestPages);

// Blocks till the current page uses a specific icon URL.
class FaviconWaiter : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconWaiter(favicon::ContentFaviconDriver* driver)
      : driver_(driver) {
    driver_->AddObserver(this);
  }

  FaviconWaiter(const FaviconWaiter&) = delete;
  FaviconWaiter& operator=(const FaviconWaiter&) = delete;

  void WaitForFaviconWithURL(const GURL& url) {
    if (GetCurrentFaviconURL() == url) {
      driver_->RemoveObserver(this);
      return;
    }

    target_favicon_url_ = url;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  GURL GetCurrentFaviconURL() {
    content::NavigationController& controller =
        driver_->web_contents()->GetController();
    content::NavigationEntry* entry = controller.GetLastCommittedEntry();
    return entry ? entry->GetFavicon().url : GURL();
  }

  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    if (notification_icon_type == NON_TOUCH_16_DIP &&
        icon_url == target_favicon_url_) {
      driver_->RemoveObserver(this);

      if (!quit_closure_.is_null())
        quit_closure_.Run();
    }
  }

  raw_ptr<favicon::ContentFaviconDriver> driver_;
  GURL target_favicon_url_;
  base::RepeatingClosure quit_closure_;
};

}  // namespace

// Defines a browser test class for testing the task manager tracking of tab
// contents.
class TabContentsTagTest : public InProcessBrowserTest {
 public:
  TabContentsTagTest() { EXPECT_TRUE(embedded_test_server()->Start()); }
  TabContentsTagTest(const TabContentsTagTest&) = delete;
  TabContentsTagTest& operator=(const TabContentsTagTest&) = delete;
  ~TabContentsTagTest() override = default;

  void AddNewTestTabAt(int index, const char* test_page_file) {
    int tabs_count_before = tabs_count();
    GURL url = GetUrlOfFile(test_page_file);
    ASSERT_TRUE(AddTabAtIndex(index, url, ui::PAGE_TRANSITION_TYPED));
    EXPECT_EQ(++tabs_count_before, tabs_count());
  }

  void NavigateToUrl(const char* test_page_file) {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetUrlOfFile(test_page_file)));
  }

  void CloseTabAt(int index) {
    browser()->tab_strip_model()->CloseWebContentsAt(index,
                                                     TabCloseTypes::CLOSE_NONE);
  }

  std::u16string GetTestPageExpectedTitle(const TestPageData& page_data) const {
    // Pages with no title should fall back to their URL.
    std::u16string title = base::UTF8ToUTF16(page_data.title);
    if (title.empty()) {
      GURL url = GetUrlOfFile(page_data.page_file);
      return GetDefaultTitleForUrl(url);
    }
    return l10n_util::GetStringFUTF16(page_data.expected_prefix_message, title);
  }

  // Returns the expected title for |url| if |url| does not specify a custom
  // title (e.g. via the <title> tag).
  std::u16string GetDefaultTitleForUrl(const GURL& url) const {
    std::u16string title =
        base::UTF8ToUTF16(url.host() + ":" + url.port() + url.path());
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX, title);
  }

  std::u16string GetAboutBlankExpectedTitle() const {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                      u"about:blank");
  }

  int tabs_count() const { return browser()->tab_strip_model()->count(); }

  const std::vector<raw_ptr<WebContentsTag, VectorExperimental>>& tracked_tags()
      const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }

  GURL GetUrlOfFile(const char* test_page_file) const {
    return embedded_test_server()->GetURL(test_page_file);
  }
};

// Tests that TabContentsTags are being recorded correctly by the
// WebContentsTagsManager.
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, BasicTagsTracking) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());

  // Add a bunch of tabs and make sure we're tracking them.
  AddNewTestTabAt(0, kTestPages[0].page_file);
  EXPECT_EQ(2, tabs_count());
  EXPECT_EQ(2U, tracked_tags().size());

  AddNewTestTabAt(1, kTestPages[1].page_file);
  EXPECT_EQ(3, tabs_count());
  EXPECT_EQ(3U, tracked_tags().size());

  // Navigating the selected tab doesn't change the number of tabs nor the
  // number of tags.
  NavigateToUrl(kTestPages[2].page_file);
  EXPECT_EQ(3, tabs_count());
  EXPECT_EQ(3U, tracked_tags().size());

  // Close a bunch of tabs and make sure we can notice that.
  CloseTabAt(0);
  CloseTabAt(0);
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());
}

// Tests that the pre-task-manager-existing tabs are given to the task manager
// once it starts observing.
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, PreExistingTaskProviding) {
  // We start with the "about:blank" tab.
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());

  // Add a bunch of tabs and make sure when the task manager is created and
  // starts observing sees those pre-existing tabs.
  AddNewTestTabAt(0, kTestPages[0].page_file);
  EXPECT_EQ(2, tabs_count());
  EXPECT_EQ(2U, tracked_tags().size());
  AddNewTestTabAt(1, kTestPages[1].page_file);
  EXPECT_EQ(3, tabs_count());
  EXPECT_EQ(3U, tracked_tags().size());

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();
  EXPECT_EQ(3U, task_manager.tasks().size());
}

// Tests that the task manager sees the correct tabs with their correct
// corresponding tasks data.
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, PostExistingTaskProviding) {
  // We start with the "about:blank" tab.
  EXPECT_EQ(1, tabs_count());
  EXPECT_EQ(1U, tracked_tags().size());

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();
  ASSERT_EQ(1U, task_manager.tasks().size());

  const Task* first_tab_task = task_manager.tasks().front();
  EXPECT_EQ(Task::RENDERER, first_tab_task->GetType());
  EXPECT_EQ(GetAboutBlankExpectedTitle(), first_tab_task->title());

  // Add the test pages in order and test the provided tasks.
  for (const auto& test_page_data : kTestPages) {
    AddNewTestTabAt(0, test_page_data.page_file);

    const Task* task = task_manager.tasks().back();
    EXPECT_EQ(test_page_data.task_type, task->GetType());
    EXPECT_EQ(GetTestPageExpectedTitle(test_page_data), task->title());
  }

  EXPECT_EQ(1 + kTestPagesLength, task_manager.tasks().size());

  // Close the last tab that was added. Make sure it doesn't show up in the
  // task manager.
  CloseTabAt(0);
  EXPECT_EQ(kTestPagesLength, task_manager.tasks().size());
  const std::u16string closed_tab_title =
      GetTestPageExpectedTitle(kTestPages[kTestPagesLength - 1]);
  for (const task_manager::Task* task : task_manager.tasks()) {
    EXPECT_NE(closed_tab_title, task->title());
  }
}

// Test that the default favicon is shown in the task manager after navigating
// from a page with a favicon to a page without a favicon. crbug.com/528924
IN_PROC_BROWSER_TEST_F(TabContentsTagTest, NavigateToPageNoFavicon) {
  // We start with the "about:blank" tab.
  MockWebContentsTaskManager task_manager;
  task_manager.StartObserving();
  ASSERT_EQ(1, tabs_count());
  ASSERT_EQ(1U, tracked_tags().size());

  // Navigate to a page with a favicon.
  GURL favicon_page_url = GetUrlOfFile("/favicon/page_with_favicon.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), favicon_page_url));
  ASSERT_GE(1U, task_manager.tasks().size());
  Task* task = task_manager.tasks().back();
  ASSERT_EQ(GetDefaultTitleForUrl(favicon_page_url), task->title());

  // Wait for the browser to download the favicon.
  favicon::ContentFaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  FaviconWaiter waiter(favicon_driver);
  waiter.WaitForFaviconWithURL(GetUrlOfFile("/favicon/icon.png"));
  const auto favicon_url = browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetSiteInstance()
                               ->GetSiteURL();

  // Check that the task manager uses the specified favicon for the page.
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  std::string favicon_string;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(
        test_dir.AppendASCII("favicon").AppendASCII("icon.png"),
        &favicon_string);
  }
  SkBitmap favicon_bitmap;
  gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(favicon_string.data()),
      favicon_string.length(), &favicon_bitmap);
  ASSERT_TRUE(
      gfx::test::AreBitmapsEqual(favicon_bitmap, *task->icon().bitmap()));

  // Navigate to a page without a favicon.
  GURL no_favicon_page_url = GetUrlOfFile("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), no_favicon_page_url));

  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main frame navigations, we'll get a new task because we are
    // changing RenderFrameHosts. Note that the previous page's task might still
    // be around if the previous page is saved in the back/forward cache.
    if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
      ASSERT_EQ(2U, task_manager.tasks().size());
      ASSERT_EQ(
          l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX,
                                     base::UTF8ToUTF16(favicon_url.spec())),
          task_manager.tasks().front()->title());
    } else {
      ASSERT_EQ(1U, task_manager.tasks().size());
    }
  }

  task = task_manager.tasks().back();
  ASSERT_EQ(GetDefaultTitleForUrl(no_favicon_page_url), task->title());

  // Check that the task manager uses the default favicon for the page.
  gfx::Image default_favicon_image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON);
  gfx::Image default_dark_favicon_image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON_DARK);
  EXPECT_TRUE(gfx::test::AreImagesEqual(default_favicon_image,
                                        gfx::Image(task->icon())) ||
              gfx::test::AreImagesEqual(default_dark_favicon_image,
                                        gfx::Image(task->icon())));
}

class TabContentsTagFencedFrameTest : public TabContentsTagTest {
 public:
  TabContentsTagFencedFrameTest() = default;
  ~TabContentsTagFencedFrameTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Tests that a fenced frame doesn't update the title of its web contents' task
// via WebContentsTaskProvider::WebContentsEntry.
IN_PROC_BROWSER_TEST_F(TabContentsTagFencedFrameTest,
                       FencedFrameDoesNotUpdateTitle) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();
  ASSERT_EQ(1U, task_manager.tasks().size());

  const GURL initial_url = embedded_test_server()->GetURL("/title3.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  const Task* primary_mainframe_task = task_manager.tasks().front();
  EXPECT_EQ(Task::RENDERER, primary_mainframe_task->GetType());
  EXPECT_EQ(primary_mainframe_task->title(), u"Tab: Title Of More Awesomeness");

  // Create a fenced frame and load a URL.
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/fenced_frames/title2.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), kFencedFrameUrl);
  EXPECT_NE(nullptr, fenced_frame_host);

  // The navigation in the fenced frame should not change the title of the
  // primary mainframe's task to "Title Of Awesomeness".
  EXPECT_EQ(primary_mainframe_task->title(), u"Tab: Title Of More Awesomeness");
}

}  // namespace task_manager
