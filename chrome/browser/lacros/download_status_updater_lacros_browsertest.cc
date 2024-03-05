// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/download/download_bubble_info_utils.h"
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
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

// Aliases.
using ::crosapi::mojom::DownloadProgress;
using ::crosapi::mojom::DownloadState;
using ::crosapi::mojom::DownloadStatus;
using ::crosapi::mojom::DownloadStatusIcons;
using ::crosapi::mojom::DownloadStatusPtr;
using ::crosapi::mojom::DownloadStatusUpdater;
using ::crosapi::mojom::DownloadStatusUpdaterClient;
using ::crosapi::mojom::DownloadStatusUpdaterClientAsyncWaiter;
using ::testing::Action;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Pointer;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
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

// Helpers ---------------------------------------------------------------------

// Creates a JPEG file from `bitmap`. Returns whether creation succeeds.
bool CreateJPEGFile(const base::FilePath& file_path, const SkBitmap& bitmap) {
  if (file_path.Extension() != ".jpg") {
    return false;
  }

  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);

  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::WriteFile(file_path, data);
}

// Matchers --------------------------------------------------------------------

MATCHER_P(BitmapEq, bitmap, "") {
  return gfx::test::AreBitmapsEqual(arg, bitmap);
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
      download::DownloadTargetCallback* callback) override {
    auto set_dangerous = [](download::DownloadTargetCallback callback,
                            download::DownloadTargetInfo target_info) {
      target_info.danger_type = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
      std::move(callback).Run(std::move(target_info));
    };

    download::DownloadTargetCallback dangerous_callback =
        base::BindOnce(set_dangerous, std::move(*callback));
    bool run = ChromeDownloadManagerDelegate::DetermineDownloadTarget(
        item, &dangerous_callback);
    // ChromeDownloadManagerDelegate::DetermineDownloadTarget() needs to run the
    // `callback`.
    CHECK(run);
    CHECK(!dangerous_callback);
    return true;
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
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    waiter->WaitForFinished();

    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
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
  EXPECT_FALSE(client.ShowInBrowser(/*guid=*/std::string()));
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

  std::vector<DownloadItemWarningData::WarningActionEvent> events =
      DownloadItemWarningData::GetWarningActionEvents(item);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(events[0].surface,
            DownloadItemWarningData::WarningSurface::DOWNLOAD_NOTIFICATION);
  EXPECT_EQ(events[0].action,
            DownloadItemWarningData::WarningAction::OPEN_SUBPAGE);
  EXPECT_FALSE(events[0].is_terminal_action);
}

IN_PROC_BROWSER_TEST_F(
    DownloadStatusUpdaterBrowserTest,
    ShowInBrowser_NormalDownload_PickMostRecentActiveBrowser) {
  // Open a different browser window and activate it.
  Browser* browser2 = CreateBrowser(browser()->profile());
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
  Browser* browser2 = CreateBrowser(browser()->profile());
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
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));

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
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

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
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

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
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

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
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

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

// The test suite verifying that `DownloadStatusUpdater::Update()` events work
// as intended.
class DownloadStatusUpdaterUpdateBrowserTest
    : public DownloadStatusUpdaterBrowserTest {
 public:
  download::MockDownloadItem& mock_download_item() { return item_; }

 private:
  // DownloadStatusUpdaterBrowserTest:
  void SetUpOnMainThread() override {
    DownloadStatusUpdaterBrowserTest::SetUpOnMainThread();

    ON_CALL(item_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(
            base::Uuid::GenerateRandomV4().AsLowercaseString()));

    // Associate the download `item_` with the browser `profile()`.
    content::DownloadItemUtils::AttachInfoForTesting(&item_,
                                                     browser()->profile(),
                                                     /*web_contents=*/nullptr);
  }

  // A mock download item with a valid guid.
  NiceMock<download::MockDownloadItem> item_;
};

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterUpdateBrowserTest, Basics) {
  // Configure `item` to indicate an in-progress download.
  download::MockDownloadItem& item = mock_download_item();
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  ON_CALL(item, GetReceivedBytes()).WillByDefault(Return(10));
  ON_CALL(item, GetTotalBytes()).WillByDefault(Return(100));
  ON_CALL(item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath("target_file_path")));
  ON_CALL(item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath("full_path")));

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

  DownloadItemModel download_item_model(
      &item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` creation.
  // TODO(http://b/306459683): Remove the testing on the deprecated received
  // bytes and total bytes fields after the 2-version skew period passes.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::cancellable, Eq(true)),
          Field(&DownloadStatus::full_path, Eq(item.GetFullPath())),
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::pausable, Eq(true)),
          Field(&DownloadStatus::progress,
                Pointer(AllOf(Field(&DownloadProgress::loop, Eq(false)),
                              Field(&DownloadProgress::received_bytes,
                                    Eq(item.GetReceivedBytes())),
                              Field(&DownloadProgress::total_bytes,
                                    Eq(item.GetTotalBytes())),
                              Field(&DownloadProgress::visible, Eq(true))))),
          Field(&DownloadStatus::resumable, Eq(false)),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress)),
          Field(&DownloadStatus::status_text,
                Eq(download_item_model.GetStatusText())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath()))))));

  // Notify the download status updater in Lacros Chrome of `item` creation and
  // verify Ash Chrome expectations.
  download_manager().NotifyDownloadCreated(&item);
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Pause `item`.
  ON_CALL(item, IsPaused()).WillByDefault(Return(true));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  // TODO(http://b/306459683): Remove the testing on the deprecated received
  // bytes and total bytes fields after the 2-version skew period passes.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::cancellable, Eq(true)),
          Field(&DownloadStatus::full_path, Eq(item.GetFullPath())),
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::pausable, Eq(false)),
          Field(&DownloadStatus::progress,
                Pointer(AllOf(Field(&DownloadProgress::loop, Eq(false)),
                              Field(&DownloadProgress::received_bytes,
                                    Eq(item.GetReceivedBytes())),
                              Field(&DownloadProgress::total_bytes,
                                    Eq(item.GetTotalBytes())),
                              Field(&DownloadProgress::visible, Eq(true))))),
          Field(&DownloadStatus::resumable, Eq(true)),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress)),
          Field(&DownloadStatus::status_text,
                Eq(download_item_model.GetStatusText())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath()))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Configure `item` to have dangerous contents.
  ON_CALL(item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT));

  const auto* const native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  ui::ColorProviderKey dark_mode_key = native_theme->GetColorProviderKey(
      /*custom_theme=*/nullptr);
  dark_mode_key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
  const ui::ColorProvider* const color_provider_dark =
      ui::ColorProviderManager::Get().GetColorProviderFor(dark_mode_key);

  ui::ColorProviderKey light_mode_key =
      native_theme->GetColorProviderKey(/*custom_theme=*/nullptr);
  light_mode_key.color_mode = ui::ColorProviderKey::ColorMode::kLight;
  const ui::ColorProvider* const color_provider_light =
      ui::ColorProviderManager::Get().GetColorProviderFor(light_mode_key);

  const IconAndColor icon_and_color =
      IconAndColorForDownload(download_item_model);
  ASSERT_TRUE(icon_and_color.icon);
  const SkColor expected_color_dark =
      color_provider_dark->GetColor(icon_and_color.color);
  const SkColor expected_color_light =
      color_provider_light->GetColor(icon_and_color.color);

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  // Check the download status's icons.
  EXPECT_CALL(download_status_updater(),
              Update(Pointer(Field(
                  &DownloadStatus::icons,
                  Pointer(AllOf(
                      Field(&DownloadStatusIcons::dark_mode,
                            Property(&gfx::ImageSkia::bitmap,
                                     Pointee(BitmapEq(*gfx::CreateVectorIcon(
                                                           *icon_and_color.icon,
                                                           /*dip_size=*/50,
                                                           expected_color_dark)
                                                           .bitmap())))),
                      Field(&DownloadStatusIcons::light_mode,
                            Property(&gfx::ImageSkia::bitmap,
                                     Pointee(BitmapEq(*gfx::CreateVectorIcon(
                                                           *icon_and_color.icon,
                                                           /*dip_size=*/50,
                                                           expected_color_light)
                                                           .bitmap()))))))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Reset the danger type.
  ON_CALL(item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));

  // Complete `item`.
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(item, GetReceivedBytes())
      .WillByDefault(Invoke(&item, &download::DownloadItem::GetTotalBytes));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  // Since the download completes, the progress bar is invisible.
  // TODO(http://b/306459683): Remove the testing on the deprecated received
  // bytes and total bytes fields after the 2-version skew period passes.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::cancellable, Eq(false)),
          Field(&DownloadStatus::full_path, Eq(item.GetFullPath())),
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::icons, Pointer(IsNull())),
          Field(&DownloadStatus::pausable, Eq(false)),
          Field(&DownloadStatus::progress,
                Pointer(AllOf(Field(&DownloadProgress::loop, Eq(false)),
                              Field(&DownloadProgress::received_bytes,
                                    Eq(item.GetReceivedBytes())),
                              Field(&DownloadProgress::total_bytes,
                                    Eq(item.GetTotalBytes())),
                              Field(&DownloadProgress::visible, Eq(false))))),
          Field(&DownloadStatus::resumable, Eq(false)),
          Field(&DownloadStatus::state, Eq(DownloadState::kComplete)),
          Field(&DownloadStatus::status_text,
                Eq(download_item_model.GetStatusText())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath()))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterUpdateBrowserTest, DownloadImage) {
  base::ScopedTempDir temp_dir;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  }

  const base::FilePath image_path = temp_dir.GetPath().AppendASCII("image.jpg");
  const SkBitmap bitmap = gfx::test::CreateBitmap(/*size=*/100, SK_ColorCYAN);
  ASSERT_TRUE(CreateJPEGFile(image_path, bitmap));

  // Configure `item` to indicate an in-progress image download.
  download::MockDownloadItem& item = mock_download_item();
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  ON_CALL(item, GetReceivedBytes()).WillByDefault(Return(100));
  ON_CALL(item, GetTotalBytes()).WillByDefault(Return(100));
  ON_CALL(item, GetTargetFilePath()).WillByDefault(ReturnRef(image_path));
  ON_CALL(item, GetFullPath())
      .WillByDefault(ReturnRef(item.GetTargetFilePath()));

  // The download file should be of an image MIME type.
  DownloadItemModel download_item_model(
      &item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  EXPECT_TRUE(download_item_model.HasSupportedImageMimeType());

  // Create a download associated with `item`.
  download_manager().NotifyDownloadCreated(&item);
  FlushInterfaceForTesting();

  // The image is created asynchronously. Therefore, right after download
  // completion, the image cached by the download status should be null.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(Field(&DownloadStatus::image,
                           Property(&gfx::ImageSkia::isNull, IsTrue)))));

  // Complete the download.
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Wait until the image is created. Check the image's contents.
  base::RunLoop run_loop;
  EXPECT_CALL(download_status_updater(),
              Update(Pointer(Field(&DownloadStatus::image,
                                   Property(&gfx::ImageSkia::bitmap,
                                            Pointee(BitmapEq(bitmap)))))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  run_loop.Run();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.Delete());
  }
}

}  // namespace
