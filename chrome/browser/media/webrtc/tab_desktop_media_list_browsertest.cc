// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/common/extension_builder.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using content::WebContents;

namespace {

constexpr int kDefaultSourceCount = 2;
constexpr int kThumbnailSize = 50;

// Create a greyscale image with certain size and grayscale value.
gfx::Image CreateGrayscaleImage(gfx::Size size, uint8_t greyscale_value) {
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height(), true);

  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());

  // Set greyscale value for all pixels.
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      UNSAFE_TODO(
          pixels_data[result.rowBytes() * y + x * result.bytesPerPixel()]) =
          greyscale_value;
      UNSAFE_TODO(
          pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 1]) =
          greyscale_value;
      UNSAFE_TODO(
          pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 2]) =
          greyscale_value;
      UNSAFE_TODO(
          pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3]) =
          0xff;
    }
  }

  return gfx::Image::CreateFrom1xBitmap(result);
}

ACTION_P2(CheckListSize, list, expected_list_size) {
  EXPECT_EQ(expected_list_size, list->GetSourceCount());
}

// This is a helper class to abstract away some of the details of creating and
// managing the life-cycle of an AppWindow
class TestAppWindow : public content::WebContentsObserver {
 public:
  TestAppWindow(const TestAppWindow&) = delete;
  TestAppWindow& operator=(const TestAppWindow&) = delete;

  TestAppWindow(Profile* profile,
                const extensions::Extension* extension,
                std::unique_ptr<content::WebContents> contents) {
    window_ = new extensions::AppWindow(
        profile,
        std::make_unique<ChromeAppDelegate>(profile, /*keep_alive=*/false),
        extension);

    extensions::AppWindow::CreateParams params;
    window_->Init(GURL(),
                  std::make_unique<extensions::TestAppWindowContents>(
                      std::move(contents)),
                  nullptr, params);

    Observe(window_->web_contents());
  }

  ~TestAppWindow() override { Close(); }

  void Close() {
    if (!window_) {
      return;
    }

    Observe(nullptr);  // Stop observing since we handle destruction here
    extensions::AppWindow* local_window = window_;
    window_ = nullptr;

    content::WebContentsDestroyedWatcher destroyed_watcher(
        local_window->web_contents());

    local_window->GetBaseWindow()->Close();

    destroyed_watcher.Wait();
  }

  void WebContentsDestroyed() override { window_ = nullptr; }

 private:
  raw_ptr<extensions::AppWindow> window_;
};

}  // namespace

class TabDesktopMediaListTest : public InProcessBrowserTest,
                                public testing::WithParamInterface<bool> {
 public:
  TabDesktopMediaListTest(const TabDesktopMediaListTest&) = delete;
  TabDesktopMediaListTest& operator=(const TabDesktopMediaListTest&) = delete;

 protected:
  TabDesktopMediaListTest() : picker_called_from_web_contents_(GetParam()) {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    for (int i = 0; i < kDefaultSourceCount; i++) {
      AddWebcontents(i + 1);
    }
    // Close the default about:blank tab
    browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                     TabCloseTypes::CLOSE_NONE);
  }

  void TearDownOnMainThread() override {
    list_.reset();
    manually_added_app_windows_.clear();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void AddWebcontents(int favicon_greyscale) {
    GURL url = embedded_test_server()->GetURL("/title1.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(contents);

    std::u16string title =
        u"Test tab " + base::NumberToString16(favicon_greyscale);
    contents->UpdateTitleForEntry(
        contents->GetController().GetLastCommittedEntry(), title);
  }

  void AddAppWindow() {
    auto web_contents = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));

    // Navigate to a valid test page to ensure the WebContents is fully
    // initialized and has a valid RenderFrameHost and Process ID.
    EXPECT_TRUE(content::NavigateToURL(
        web_contents.get(), embedded_test_server()->GetURL("/title1.html")));

    auto app_window = std::make_unique<TestAppWindow>(
        browser()->profile(), BuildOrGetExtension(), std::move(web_contents));
    manually_added_app_windows_.push_back(std::move(app_window));
  }

  const extensions::Extension* BuildOrGetExtension() {
    if (!extension_) {
      extension_ =
          extensions::ExtensionBuilder()
              .SetManifest(base::DictValue()
                               .Set("name", "TabListUnitTest Extension")
                               .Set("version", "1.0")
                               .Set("manifest_version", 2))
              .Build();
    }
    return extension_.get();
  }

  void CreateDefaultList() {
    DCHECK(!list_);

    content::WebContents* const web_contents =
        picker_called_from_web_contents_
            ? browser()->tab_strip_model()->GetWebContentsAt(0)
            : nullptr;

    // The actual "default" for |include_chrome_app_windows| is false; but for
    // the purposes of the tests we make the default true, so that all paths are
    // exercised.
    list_ = std::make_unique<TabDesktopMediaList>(
        web_contents, base::BindRepeating([](content::WebContents* contents) {
          return true;
        }),
        /*include_chrome_app_windows=*/true);
    list_->SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    // Set update period to reduce the time it takes to run tests.
    // >0 to avoid unit test failure.
    list_->SetUpdatePeriod(base::Milliseconds(1));
  }

  void InitializeAndVerify() {
    CreateDefaultList();
    base::RunLoop loop;
    // The tabs in media source list are sorted in decreasing time order. The
    // latest one is listed first. However, tabs are added to TabStripModel in
    // increasing time order, the oldest one is added first.
    {
      testing::InSequence dummy;

      for (int i = 0; i < kDefaultSourceCount; i++) {
        EXPECT_CALL(observer_, OnSourceAdded(i))
            .WillOnce(CheckListSize(list_.get(), i + 1));
      }

      for (int i = 0; i < kDefaultSourceCount - 1; i++) {
        EXPECT_CALL(observer_,
                    OnSourceThumbnailChanged(kDefaultSourceCount - 1 - i));
      }
      EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
          .WillOnce(base::test::RunClosure(loop.QuitClosure()));
    }

    // TabDesktopMediaList needs an observer that implements
    // DesktopMediaListObserver to update
    list_->StartUpdating(&observer_);
    loop.Run();

    for (int i = 0; i < kDefaultSourceCount; ++i) {
      EXPECT_EQ(list_->GetSource(i).id.type,
                content::DesktopMediaID::TYPE_WEB_CONTENTS);
    }

    observer_.VerifyAndClearExpectations();
  }

  const bool picker_called_from_web_contents_;

  // Must be listed before |list_|, so it's destroyed last.
  DesktopMediaListMockObserver observer_;
  std::unique_ptr<TabDesktopMediaList> list_;
  std::vector<raw_ptr<content::WebContents, VectorExperimental>>
      manually_added_web_contents_;
  std::vector<std::unique_ptr<TestAppWindow>> manually_added_app_windows_;

  scoped_refptr<const extensions::Extension> extension_;
};

INSTANTIATE_TEST_SUITE_P(, TabDesktopMediaListTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest, AddTab) {
  InitializeAndVerify();
  base::RunLoop loop;
  EXPECT_CALL(observer_, OnSourceAdded(0))
      .WillOnce(CheckListSize(list_.get(), kDefaultSourceCount + 1));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  AddWebcontents(10);

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest, AddAppWindow) {
  InitializeAndVerify();

  base::RunLoop loop;
  // Note that unlike adding a tab, our AppWindow that we add is only
  // initialized enough to show up in the list; it doesn't have a favicon driver
  // which would be needed to extract the favicon from it.
  EXPECT_CALL(observer_, OnSourceAdded(0))
      .WillOnce(
          testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount + 1),
                         base::test::RunClosure(loop.QuitClosure())));

  AddAppWindow();

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest, RemoveTab) {
  InitializeAndVerify();
  base::RunLoop loop;
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  EXPECT_CALL(observer_, OnSourceRemoved(0))
      .WillOnce(
          testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount - 1),
                         base::test::RunClosure(loop.QuitClosure())));

  tab_strip_model->CloseWebContentsAt(kDefaultSourceCount - 1,
                                      TabCloseTypes::CLOSE_NONE);

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest, MoveTab) {
  InitializeAndVerify();
  base::RunLoop loop;
  // Swap the two media sources by swap their time stamps.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  tab_strip_model->ActivateTabAt(0);

  EXPECT_CALL(observer_, OnSourceMoved(1, 0))
      .WillOnce(testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount),
                               base::test::RunClosure(loop.QuitClosure())));

  loop.Run();
}

IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest, UpdateTitle) {
  InitializeAndVerify();
  base::RunLoop loop;
  // Change tab's title.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);
  content::NavigationController& controller = contents->GetController();
  contents->UpdateTitleForEntry(controller.GetLastCommittedEntry(),
                                u"New test tab");

  EXPECT_CALL(observer_, OnSourceNameChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();

  EXPECT_EQ(list_->GetSource(0).name, u"New test tab");
}

IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest, UpdateThumbnail) {
  InitializeAndVerify();

  base::RunLoop loop;
  // Change tab's favicon.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);

  content::FaviconStatus favicon_info;
  favicon_info.image = CreateGrayscaleImage(gfx::Size(10, 10), 100);
  contents->GetController().GetLastCommittedEntry()->GetFavicon() =
      favicon_info;

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();
}

// Test that a source which is set as the one being previewed is marked as being
// visibly captured, so that it is still painted even when hidden.
IN_PROC_BROWSER_TEST_P(TabDesktopMediaListTest,
                       SetPreviewMarksTabAsVisiblyCaptured) {
  InitializeAndVerify();
  base::RunLoop loop;
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);

  list_->SetPreviewedSource(list_->GetSource(0).id);

  EXPECT_TRUE(contents->IsBeingVisiblyCaptured());

  EXPECT_CALL(observer_, OnSourcePreviewChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();
}

class TabDesktopMediaListIwaTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  TabDesktopMediaListIwaTest() = default;

  TabDesktopMediaListIwaTest(const TabDesktopMediaListIwaTest&) = delete;
  TabDesktopMediaListIwaTest& operator=(const TabDesktopMediaListIwaTest&) =
      delete;

  void CreateDefaultList() {
    CHECK(!list_);

    list_ = std::make_unique<TabDesktopMediaList>(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        base::BindRepeating(
            [](content::WebContents* contents) { return true; }),
        /*include_chrome_app_windows=*/false);

    // Set update period to reduce the time it takes to run tests.
    // >0 to avoid unit test failure.
    list_->SetUpdatePeriod(base::Milliseconds(1));

    // TabDesktopMediaList needs an observer that implements
    // DesktopMediaListObserver to update
    list_->StartUpdating(&observer_);
  }

  void InstallAndOpenIsolatedWebApp() {
    auto app = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
                   .BuildBundle();
    app->TrustSigningKey();
    web_app::IsolatedWebAppUrlInfo url_info = app->Install(profile()).value();

    OpenApp(url_info.app_id());
  }

  const TabDesktopMediaList& list() const { return *list_; }

 private:
  DesktopMediaListMockObserver observer_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
  std::unique_ptr<TabDesktopMediaList> list_;
};

IN_PROC_BROWSER_TEST_F(TabDesktopMediaListIwaTest,
                       IwaNotVisibleInTabMediaList) {
  CreateDefaultList();
  int initial_list_size = list().GetSourceCount();

  InstallAndOpenIsolatedWebApp();

  EXPECT_EQ(initial_list_size, list().GetSourceCount());
}

IN_PROC_BROWSER_TEST_F(TabDesktopMediaListIwaTest,
                       NewTabVisibleInTabMediaList) {
  CreateDefaultList();
  int initial_list_size = list().GetSourceCount();

  chrome::NewTab(browser());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(initial_list_size + 1, list().GetSourceCount());
}

class TabDesktopMediaListWithIwaIncludedTest
    : public TabDesktopMediaListIwaTest {
  void SetUp() override {
    base::test::ScopedFeatureList scoped_feature_list_;
    scoped_feature_list_.InitWithFeatureState(
        features::kRemovalOfIWAsFromTabCapture, false);

    TabDesktopMediaListIwaTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(TabDesktopMediaListWithIwaIncludedTest,
                       IwaVisibleInTabMediaListWhenFeatureIsDisabled) {
  CreateDefaultList();
  int initial_list_size = list().GetSourceCount();

  InstallAndOpenIsolatedWebApp();

  EXPECT_EQ(initial_list_size + 1, list().GetSourceCount());
}
