// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

// Aliases.
using ::crosapi::mojom::DownloadState;
using ::crosapi::mojom::DownloadStatus;
using ::crosapi::mojom::DownloadStatusPtr;
using ::crosapi::mojom::DownloadStatusUpdater;
using ::crosapi::mojom::DownloadStatusUpdaterClient;
using ::crosapi::mojom::DownloadStatusUpdaterClientAsyncWaiter;
using ::testing::Action;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointer;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

// Actions ---------------------------------------------------------------------

ACTION_P3(InvokeEq, invokee, invokable, expected) {
  return (invokee->*invokable)() == expected;
}

ACTION_P2(ReturnAllOf, a, b) {
  Action<bool()> action_a = a;
  Action<bool()> action_b = b;
  return action_a.Perform(std::forward_as_tuple()) &&
         action_b.Perform(std::forward_as_tuple());
}

// MockDownloadManager ---------------------------------------------------------

// A mock `content::DownloadManager` for testing which supports realistic
// observer behaviors and propagation of download creation events.
class MockDownloadManager : public content::MockDownloadManager {
 public:
  // content::MockDownloadManager:
  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void Shutdown() override {
    for (auto& observer : observer_list_) {
      observer.ManagerGoingDown(this);
    }
  }

  void NotifyDownloadCreated(download::DownloadItem* item) {
    for (auto& observer : observer_list_) {
      observer.OnDownloadCreated(this, item);
    }
  }

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

// MockDownloadStatusUpdater ---------------------------------------------------

// A mock `DownloadStatusUpdater` for testing.
class MockDownloadStatusUpdater : public DownloadStatusUpdater {
 public:
  // DownloadStatusUpdater:
  MOCK_METHOD(void,
              BindClient,
              (mojo::PendingRemote<DownloadStatusUpdaterClient> client),
              (override));
  MOCK_METHOD(void, Update, (DownloadStatusPtr status), (override));
};

// TestDownloadManagerDelegate -------------------------------------------------

// This is required to make a download appear to be a malicious danger_type that
// does not block browser shutdown.
// TODO(chlily): Deduplicate from browser_close_manager_browsertest.cc.
class TestDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    if (!profile->IsOffTheRecord()) {
      GetDownloadIdReceiverCallback().Run(download::DownloadItem::kInvalidId +
                                          1);
    }
  }
  ~TestDownloadManagerDelegate() override = default;

  // ChromeDownloadManagerDelegate:
  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      content::DownloadTargetCallback* callback) override {
    content::DownloadTargetCallback dangerous_callback = base::BindOnce(
        &TestDownloadManagerDelegate::SetDangerous, std::move(*callback));
    bool run = ChromeDownloadManagerDelegate::DetermineDownloadTarget(
        item, &dangerous_callback);
    // ChromeDownloadManagerDelegate::DetermineDownloadTarget() needs to run the
    // `callback`.
    CHECK(run);
    CHECK(!dangerous_callback);
    return true;
  }

  static void SetDangerous(content::DownloadTargetCallback callback,
                           const base::FilePath& target_path,
                           download::DownloadItem::TargetDisposition disp,
                           download::DownloadDangerType danger_type,
                           download::DownloadItem::InsecureDownloadStatus ids,
                           const base::FilePath& intermediate_path,
                           const base::FilePath& display_name,
                           const std::string& mime_type,
                           download::DownloadInterruptReason reason) {
    std::move(callback).Run(target_path, disp,
                            download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL, ids,
                            intermediate_path, display_name, mime_type, reason);
  }
};

// BrowserActivationWaiter -----------------------------------------------------

// Waits for a Browser to be set to last active.
class BrowserActivationWaiter : public BrowserListObserver {
 public:
  explicit BrowserActivationWaiter(const Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
    // When the active browser closes, the next "last active browser" in the
    // BrowserList might not be immediately activated. So we need to wait for
    // the "last active browser" to actually be active.
    if (chrome::FindLastActive() == browser_ &&
        browser_->window()->IsActive()) {
      observed_ = true;
    }
  }

  BrowserActivationWaiter(const BrowserActivationWaiter&) = delete;
  BrowserActivationWaiter& operator=(const BrowserActivationWaiter&) = delete;

  ~BrowserActivationWaiter() override { BrowserList::RemoveObserver(this); }

  // Runs a message loop until the `browser_` supplied to the constructor is
  // activated, or returns immediately if `browser_` has already become active.
  // Should only be called once.
  void WaitForActivation() {
    if (observed_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override {
    if (browser != browser_) {
      return;
    }

    observed_ = true;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  const raw_ptr<const Browser> browser_;
  bool observed_ = false;
  base::RunLoop run_loop_;
};

// Helpers ---------------------------------------------------------------------

void ActivateBrowser(Browser* browser) {
  BrowserActivationWaiter activation_waiter(browser);
  browser->window()->Show();
  activation_waiter.WaitForActivation();
}

// DownloadStatusUpdaterBrowserTest --------------------------------------------

// Base class for tests of `DownloadStatusUpdater`.
class DownloadStatusUpdaterBrowserTest : public DownloadTestBase {
 public:
  // DownloadTestBase:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    DownloadTestBase::CreatedBrowserMainParts(browser_main_parts);

    // Replace the binding for the Ash Chrome download status updater with a
    // mock that can be observed for interactions with Lacros Chrome.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        download_status_updater_receiver_
            .BindNewPipeAndPassRemoteWithVersion());

    // When the Lacros Chrome download status updater is initiated, it will
    // attempt to bind the client for the Ash Chrome download status updater.
    // Bind the client ourselves so we can verify it is working as intended.
    EXPECT_CALL(download_status_updater_, BindClient)
        .WillOnce(Invoke(
            [&](mojo::PendingRemote<DownloadStatusUpdaterClient> client) {
              download_status_updater_client_.Bind(std::move(client));
            }));
  }

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();

    // Associate the mock download manager with the Lacros Chrome browser.
    ON_CALL(download_manager_, GetBrowserContext())
        .WillByDefault(Return(browser()->profile()));

    // Register the mock download manager with the download status updater in
    // Lacros Chrome.
    g_browser_process->download_status_updater()->AddManager(
        &download_manager_);

    // Start the initial `browser()` active.
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
    BrowserActivationWaiter activation_waiter(browser());
    native_window->Show();
    native_window->Focus();
    activation_waiter.WaitForActivation();
  }

  void TearDownOnMainThread() override {
    if (item_ && item_->GetState() == download::DownloadItem::IN_PROGRESS) {
      item_->Cancel(false);
    }
    item_ = nullptr;
  }

  Browser* CreateAndWaitForBrowser(Profile* profile, bool otr = false) {
    Browser* browser =
        otr ? CreateIncognitoBrowser(profile) : CreateBrowser(profile);
    std::string browser_window_id =
        lacros_window_utility::GetRootWindowUniqueId(
            BrowserView::GetBrowserViewForBrowser(browser)
                ->frame()
                ->GetNativeWindow()
                ->GetRootWindow());
    EXPECT_TRUE(browser_test_util::WaitForWindowCreation(browser_window_id));
    return browser;
  }

  void SetUpBrowserForTest(Browser* browser) {
    download_button(browser)->DisableAutoCloseTimerForTesting();
    download_button(browser)->DisableDownloadStartedAnimationForTesting();

    // Do not prompt for download location, otherwise test will time out waiting
    // on the prompt.
    SetPromptForDownload(browser, false);
  }

  void SetUpBrowserForDangerousDownloadTest(Browser* browser) {
    SetUpBrowserForTest(browser);

    // Prime the UI to avoid crbug.com/1469673.
    // TODO(chlily): Fix the bug and remove this line.
    download_button(browser)->Show();

    // Disable SafeBrowsing so that danger will be determined by downloads
    // system.
    browser->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

    // Set up the fake delegate that forces the download to be malicious.
    auto test_delegate =
        std::make_unique<TestDownloadManagerDelegate>(browser->profile());
    DownloadCoreServiceFactory::GetForBrowserContext(browser->profile())
        ->SetDownloadManagerDelegateForTesting(std::move(test_delegate));
  }

  // Runs the current message loop until a no-op message on the download status
  // updater interface's message pipe is received. This effectively ensures that
  // any messages in transit are received before returning.
  void FlushInterfaceForTesting() {
    chromeos::LacrosService::Get()
        ->GetRemote<DownloadStatusUpdater>()
        .FlushForTesting();
  }

  download::DownloadItem* DownloadNormalTestFile(Browser* browser) {
    SetUpBrowserForTest(browser);
    download::DownloadItem* item = CreateSlowTestDownload(browser);
    EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);
    CHECK(!item_);
    item_ = item;
    return item;
  }

  download::DownloadItem* DownloadDangerousTestFile(Browser* browser) {
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    EXPECT_TRUE(embedded_test_server()->Start());

    SetUpBrowserForDangerousDownloadTest(browser);

    std::unique_ptr<content::DownloadTestObserver> waiter{
        DangerousDownloadWaiter(
            browser, /*num_downloads=*/1,
            content::DownloadTestObserver::DangerousDownloadAction::
                ON_DANGEROUS_DOWNLOAD_QUIT)};
    // Run a dangerous download, but the user doesn't make a decision. It's a
    // .swf file so that it's dangerous on all platforms (including CrOS).
    // This .swf normally would be categorized as DANGEROUS_FILE, but
    // TestDownloadManagerDelegate turns it into DANGEROUS_URL.
    GURL url =
        embedded_test_server()->GetURL("/downloads/dangerous/dangerous.swf");
    ui_test_utils::NavigateToURL(browser, url);
    waiter->WaitForFinished();

    std::vector<download::DownloadItem*> items;
    DownloadManagerForBrowser(browser)->GetAllDownloads(&items);
    EXPECT_FALSE(items.empty());

    download::DownloadItem* item = items.back();
    EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);
    EXPECT_EQ(item->GetDangerType(),
              download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
    CHECK(!item_);
    item_ = item;
    return item;
  }

  // Returns the mock download manager registered with the download status
  // updater in Lacros Chrome.
  NiceMock<MockDownloadManager>& download_manager() {
    return download_manager_;
  }

  // Returns the mock download status updater in Ash Chrome that can be observed
  // for interactions with Lacros Chrome.
  NiceMock<MockDownloadStatusUpdater>& download_status_updater() {
    return download_status_updater_;
  }

  // Returns the client for the download status updater in Ash Chrome,
  // implemented by the delegate of the download status updater in Lacros
  // Chrome.
  DownloadStatusUpdaterClient* download_status_updater_client() {
    return download_status_updater_client_.get();
  }

  DownloadToolbarButtonView* download_button(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar_button_provider()
        ->GetDownloadButton();
  }

 private:
  // The mock download manager registered with the download status updater in
  // Lacros Chrome.
  NiceMock<MockDownloadManager> download_manager_;

  // Pointer to the item that may be created for the test. This is owned by the
  // DownloadManager and should be cancelled before the test exits.
  raw_ptr<download::DownloadItem> item_ = nullptr;

  // The mock download status updater in Ash Chrome that can be observed for
  // interactions with Lacros Chrome.
  NiceMock<MockDownloadStatusUpdater> download_status_updater_;
  mojo::Receiver<DownloadStatusUpdater> download_status_updater_receiver_{
      &download_status_updater_};

  // The client for the download status updater in Ash Chrome, implemented by
  // the delegate of the download status updater in Lacros Chrome.
  mojo::Remote<DownloadStatusUpdaterClient> download_status_updater_client_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `DownloadStatusUpdaterClient::Cancel()` works as intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, Cancel) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  EXPECT_TRUE(client.Cancel(item->GetGuid()));
  EXPECT_EQ(item->GetState(), download::DownloadItem::CANCELLED);
}

// Verifies that `DownloadStatusUpdaterClient::Pause()` and `Resume()` work as
// intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, PauseAndResume) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  ASSERT_FALSE(item->IsPaused());

  EXPECT_TRUE(client.Pause(item->GetGuid()));
  EXPECT_TRUE(item->IsPaused());

  EXPECT_TRUE(client.Resume(item->GetGuid()));
  EXPECT_FALSE(item->IsPaused());
  EXPECT_NE(item->GetState(), download::DownloadItem::CANCELLED);

  // Clean up: cancel the item to allow the test to exit.
  item->Cancel(false);
}

// Tests the case where `Pause()` is called on an already-paused item.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, PauseNoOp) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_FALSE(item->IsPaused());
  item->Pause();
  ASSERT_TRUE(item->IsPaused());

  // Handled because item was found (despite being a no-op).
  EXPECT_TRUE(client.Pause(item->GetGuid()));
  EXPECT_TRUE(item->IsPaused());

  // Clean up: cancel the item to allow the test to exit.
  item->Cancel(false);
}

// Tests the case where `Resume()` is called on a not-paused item.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, ResumeNoOp) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  ASSERT_FALSE(item->CanResume());

  // Handled because item was found (despite being a no-op).
  EXPECT_TRUE(client.Resume(item->GetGuid()));
  EXPECT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  EXPECT_FALSE(item->CanResume());

  // Clean up: cancel the item to allow the test to exit.
  item->Cancel(false);
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, NoItem) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  // Create an item and then remove it so it no longer can be found.
  download::DownloadItem* item = CreateSlowTestDownload();
  std::string guid = item->GetGuid();
  item->Cancel(false);
  item->Remove();

  EXPECT_FALSE(client.Pause(guid));
  EXPECT_FALSE(client.Resume(guid));
  EXPECT_FALSE(client.Cancel(guid));
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_DoesNotHandleNonexistentGuid) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  EXPECT_FALSE(client.ShowInBrowser(/*guid=*/base::EmptyString()));
  EXPECT_FALSE(download_button(browser())->IsShowingDetails());
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_NormalDownload) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadNormalTestFile(browser());

  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_DangerousDownload) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadDangerousTestFile(browser());

  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kSecurity);
  EXPECT_EQ(download_button(browser())
                ->bubble_contents_for_testing()
                ->security_view_for_testing()
                ->content_id(),
            OfflineItemUtils::GetContentIdForDownload(item));
}

IN_PROC_BROWSER_TEST_F(
    DownloadStatusUpdaterBrowserTest,
    ShowInBrowser_NormalDownload_PickMostRecentActiveBrowser) {
  // Open a different browser window and activate it.
  Browser* browser2 = CreateAndWaitForBrowser(browser()->profile());
  ActivateBrowser(browser2);

  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  // Even though the download is started in browser2, the last active browser
  // will be chosen for the ShowInBrowser action, regardless of the initiating
  // browser.
  download::DownloadItem* item = DownloadNormalTestFile(browser2);

  // Make `browser()` the last active browser.
  ActivateBrowser(browser());

  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_FALSE(download_button(browser2)->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(
    DownloadStatusUpdaterBrowserTest,
    ShowInBrowser_DangerousDownload_PickMostRecentActiveBrowser) {
  // Open a different browser window and activate it.
  Browser* browser2 = CreateAndWaitForBrowser(browser()->profile());
  ActivateBrowser(browser2);

  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  // Even though the download is started in browser2, the last active browser
  // will be chosen for the ShowInBrowser action, regardless of the initiating
  // browser.
  download::DownloadItem* item = DownloadDangerousTestFile(browser2);

  // Make `browser()` the last active browser.
  ActivateBrowser(browser());

  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_FALSE(download_button(browser2)->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kSecurity);
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_NormalDownload_RestoreMinimizedBrowser) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  // Minimize the browser.
  browser()->window()->Minimize();
  EXPECT_TRUE(browser()->window()->IsMinimized());

  download::DownloadItem* item = DownloadNormalTestFile(browser());

  // We need to wait for the widget to become visible because it happens
  // asynchronously upon the window being restored.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      DownloadToolbarButtonView::kBubbleName);
  BrowserActivationWaiter activation_waiter(browser());
  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));
  activation_waiter.WaitForActivation();
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kPrimary);

  EXPECT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(BrowserWindow::IsRestored(*browser()->window()));
}

// TODO(crbug.com/1482885): Deflake this test. The flake rate seems to be < 5%.
IN_PROC_BROWSER_TEST_F(
    DownloadStatusUpdaterBrowserTest,
    DISABLED_ShowInBrowser_DangerousDownload_RestoreMinimizedBrowser) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  // Minimize the browser.
  browser()->window()->Minimize();
  EXPECT_TRUE(browser()->window()->IsMinimized());

  download::DownloadItem* item = DownloadDangerousTestFile(browser());

  // We need to wait for the widget to become visible because it happens
  // asynchronously upon the window being restored.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      DownloadToolbarButtonView::kBubbleName);
  BrowserActivationWaiter activation_waiter(browser());
  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));
  activation_waiter.WaitForActivation();
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kSecurity);

  EXPECT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(BrowserWindow::IsRestored(*browser()->window()));
}

// Dangerous downloads don't block browser window shutdown, so there is the
// possibility that a dangerous download is in-progress and the notification is
// clicked while there are no open browser windows for the profile. This tests
// that a new browser window is opened in such a situation.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_DangerousDownload_OpenNewBrowser) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadDangerousTestFile(browser());

  // Keep the browser process alive despite closing the last window. This
  // mimics the behavior in prod, where the lacros browser process is kept alive
  // despite the last window closing. Tests are run with
  // --disable-lacros-keep-alive which is why we need this here explicitly.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER_PROCESS_LACROS, KeepAliveRestartOption::ENABLED);

  // Close the browser window.
  int num_downloads_blocking = 0;
  ASSERT_EQ(
      browser()->OkToCloseWithInProgressDownloads(&num_downloads_blocking),
      Browser::DownloadCloseType::kOk);
  ASSERT_EQ(num_downloads_blocking, 0);
  browser()->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser());
  ASSERT_TRUE(BrowserList::GetInstance()->empty());

  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));

  // ShowInBrowser() should have caused a new browser window to be opened.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* new_browser = BrowserList::GetInstance()->get(0);

  EXPECT_TRUE(download_button(new_browser)->IsShowingDetails());
  EXPECT_EQ(download_button(new_browser)
                ->bubble_contents_for_testing()
                ->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);
}

IN_PROC_BROWSER_TEST_F(
    DownloadStatusUpdaterBrowserTest,
    ShowInBrowser_NormalDownload_MatchBrowserForExactProfile) {
  // Open an incognito browser window.
  Browser* otr_browser =
      CreateAndWaitForBrowser(browser()->profile(), /*otr=*/true);

  // Make the incognito window the last active browser. It should not be picked
  // even though it is the most recent active browser, because the profile is
  // different.
  ActivateBrowser(otr_browser);

  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadNormalTestFile(browser());

  // We need to wait for the widget to become visible because it happens
  // asynchronously upon the window being activated.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      DownloadToolbarButtonView::kBubbleName);
  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_FALSE(download_button(otr_browser)->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(
    DownloadStatusUpdaterBrowserTest,
    ShowInBrowser_DangerousDownload_MatchBrowserForExactProfile) {
  // Open an incognito browser window.
  Browser* otr_browser =
      CreateAndWaitForBrowser(browser()->profile(), /*otr=*/true);

  // Make the incognito window the last active browser. It should not be picked
  // even though it is the most recent active browser, because the profile is
  // different.
  ActivateBrowser(otr_browser);

  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadDangerousTestFile(browser());

  // We need to wait for the widget to become visible because it happens
  // asynchronously upon the window being activated.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      DownloadToolbarButtonView::kBubbleName);
  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(download_button(browser())->IsShowingDetails());
  EXPECT_FALSE(download_button(otr_browser)->IsShowingDetails());
  EXPECT_EQ(
      download_button(browser())->bubble_contents_for_testing()->VisiblePage(),
      DownloadBubbleContentsView::Page::kSecurity);
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_NormalDownload_IncognitoBrowser) {
  // Open an incognito browser window.
  Browser* otr_browser =
      CreateAndWaitForBrowser(browser()->profile(), /*otr=*/true);

  // Make `browser()` the last active browser. It should not be picked even
  // though it is the most recent active browser, because the profile is
  // different.
  ActivateBrowser(browser());

  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadNormalTestFile(otr_browser);

  // We need to wait for the widget to become visible because it happens
  // asynchronously upon the window being activated.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      DownloadToolbarButtonView::kBubbleName);
  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(download_button(otr_browser)->IsShowingDetails());
  EXPECT_FALSE(download_button(browser())->IsShowingDetails());
  EXPECT_EQ(download_button(otr_browser)
                ->bubble_contents_for_testing()
                ->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest,
                       ShowInBrowser_DangerousDownload_IncognitoBrowser) {
  // Open an incognito browser window.
  Browser* otr_browser =
      CreateAndWaitForBrowser(browser()->profile(), /*otr=*/true);

  // Make `browser()` the last active browser. It should not be picked even
  // though it is the most recent active browser, because the profile is
  // different.
  ActivateBrowser(browser());

  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  download::DownloadItem* item = DownloadDangerousTestFile(otr_browser);

  // We need to wait for the widget to become visible because it happens
  // asynchronously upon the window being activated.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{},
      DownloadToolbarButtonView::kBubbleName);
  EXPECT_TRUE(client.ShowInBrowser(item->GetGuid()));
  widget_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(download_button(otr_browser)->IsShowingDetails());
  EXPECT_FALSE(download_button(browser())->IsShowingDetails());
  EXPECT_EQ(download_button(otr_browser)
                ->bubble_contents_for_testing()
                ->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);
}

// Verifies that `DownloadStatusUpdater::Update()` events work as intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, Update) {
  // Create a mock in-progress download `item`.
  NiceMock<download::MockDownloadItem> item;
  ON_CALL(item, GetGuid())
      .WillByDefault(
          ReturnRefOfCopy(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  ON_CALL(item, GetReceivedBytes()).WillByDefault(Return(10));
  ON_CALL(item, GetTotalBytes()).WillByDefault(Return(100));
  ON_CALL(item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath("target_file_path")));

  // Fulfill `CanResume()` dynamically based on `item` state and paused status.
  ON_CALL(item, CanResume())
      .WillByDefault(
          ReturnAllOf(Invoke(&item, &download::DownloadItem::IsPaused),
                      InvokeEq(&item, &download::DownloadItem::GetState,
                               download::DownloadItem::IN_PROGRESS)));

  // Fulfill `IsDone()` dynamically based on `item` state.
  ON_CALL(item, IsDone())
      .WillByDefault(InvokeEq(&item, &download::DownloadItem::GetState,
                              download::DownloadItem::COMPLETE));

  // Associate the download `item` with the browser `profile()`.
  content::DownloadItemUtils::AttachInfoForTesting(&item, browser()->profile(),
                                                   /*web_contents=*/nullptr);

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` creation.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress)),
          Field(&DownloadStatus::received_bytes, Eq(item.GetReceivedBytes())),
          Field(&DownloadStatus::total_bytes, Eq(item.GetTotalBytes())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath())),
          Field(&DownloadStatus::cancellable, Eq(true)),
          Field(&DownloadStatus::pausable, Eq(true)),
          Field(&DownloadStatus::resumable, Eq(false))))));

  // Notify the download status updater in Lacros Chrome of `item` creation and
  // verify Ash Chrome expectations.
  download_manager().NotifyDownloadCreated(&item);
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Pause `item`.
  ON_CALL(item, IsPaused()).WillByDefault(Return(true));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress)),
          Field(&DownloadStatus::received_bytes, Eq(item.GetReceivedBytes())),
          Field(&DownloadStatus::total_bytes, Eq(item.GetTotalBytes())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath())),
          Field(&DownloadStatus::cancellable, Eq(true)),
          Field(&DownloadStatus::pausable, Eq(false)),
          Field(&DownloadStatus::resumable, Eq(true))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Complete `item`.
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(item, GetReceivedBytes())
      .WillByDefault(Invoke(&item, &download::DownloadItem::GetTotalBytes));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kComplete)),
          Field(&DownloadStatus::received_bytes, Eq(item.GetReceivedBytes())),
          Field(&DownloadStatus::total_bytes, Eq(item.GetTotalBytes())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath())),
          Field(&DownloadStatus::cancellable, Eq(false)),
          Field(&DownloadStatus::pausable, Eq(false)),
          Field(&DownloadStatus::resumable, Eq(false))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());
}

}  // namespace
