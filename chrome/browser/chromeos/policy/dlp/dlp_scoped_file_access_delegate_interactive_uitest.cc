// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace policy {
namespace {
const char kInputDragNDropPagePath[] = "/dlp/file_drop.html";
const char kInputCopyPastePagePath[] = "/dlp/file_paste.html";

const char kFileContent[] = "File content.";
const char kErrorMessage[] = "Could not read the file.";

// Coordinates where to drop the file, see /dlp/file_drop.html.
const gfx::Point kMiddleOfDropArea = gfx::Point(250, 250);
}  // namespace

class DlpScopedFileAccessDelegateInteractiveUITest
    : public InProcessBrowserTest {
 public:
  DlpScopedFileAccessDelegateInteractiveUITest() = default;
  DlpScopedFileAccessDelegateInteractiveUITest(
      const DlpScopedFileAccessDelegateInteractiveUITest&) = delete;
  DlpScopedFileAccessDelegateInteractiveUITest& operator=(
      const DlpScopedFileAccessDelegateInteractiveUITest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // DlpClient is initialized by DBUS code, let's shut it down and manually
    // start a fake client. The client will then be shutdown by DBUS code at the
    // end of the test.
    chromeos::DlpClient::Shutdown();
    chromeos::DlpClient::InitializeFake();

    delegate_ = std::unique_ptr<DlpScopedFileAccessDelegate>(
        new DlpScopedFileAccessDelegate(
            base::BindRepeating(chromeos::DlpClient::Get)));

    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        browser()->window()->GetNativeWindow()));
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(temp_dir_.Delete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool NavigateToTestPage(const std::string& origin_of_main_frame,
                          const std::string& page_path) {
    GURL url = embedded_test_server()->GetURL(origin_of_main_frame, page_path);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return web_contents()->GetLastCommittedURL() == url;
  }

  base::FilePath CreateTestFileInDirectory(const base::FilePath& directory_path,
                                           const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(directory_path, &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  bool SimulateFileDrop(content::WebContents* web_contents,
                        const base::FilePath& file,
                        const gfx::Point& location) {
    std::unique_ptr<ui::OSExchangeData> data =
        std::make_unique<ui::OSExchangeData>();
    data->SetFilename(file);
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(web_contents->GetContentNativeView());
    if (!delegate) {
      return false;
    }
    static constexpr int kDefaultSourceOperations =
        ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY |
        ui::DragDropTypes::DRAG_LINK;
    gfx::PointF event_location;
    gfx::PointF event_root_location;
    CalculateEventLocations(location, &event_location, &event_root_location,
                            web_contents);
    std::unique_ptr<ui::DropTargetEvent> active_drag_event_ = base::WrapUnique(
        new ui::DropTargetEvent(*data, event_location, event_root_location,
                                kDefaultSourceOperations));
    // Simulate drag.
    delegate->OnDragEntered(*active_drag_event_);
    delegate->OnDragUpdated(*active_drag_event_);
    // Simulate drop.
    active_drag_event_->set_location_f(event_location);
    active_drag_event_->set_root_location_f(event_root_location);
    delegate->OnDragUpdated(*active_drag_event_);

    auto drop_cb = delegate->GetDropCallback(*active_drag_event_);
    if (!drop_cb) {
      return false;
    }
    ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(data), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);
    return true;
  }

  void ClickOnView(views::View* view) {
    // Allow the page to be fully rendered before trying to click on it. Without
    // sleeping the test is flaky.
    // TODO(b/293274162): Find a better way that does not require sleeping.
    base::PlatformThread::Sleep(base::Seconds(1));

    // Click a few times to make sure the page has focus.
    // See also https://crbug.com/59011 and https://crbug.com/35581.
    ui_test_utils::ClickOnView(view);
    ui_test_utils::ClickOnView(view);
  }

  void SimulateFilePaste(content::WebContents* web_contents,
                         const base::FilePath& file) {
    // Put files on clipboard.
    {
      ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
      writer.WriteFilenames(
          ui::FileInfosToURIList({ui::FileInfo(file, base::FilePath())}));
    }

    web_contents->Paste();
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  void CalculateEventLocations(const gfx::Point& web_contents_relative_location,
                               gfx::PointF* out_event_location,
                               gfx::PointF* out_event_root_location,
                               content::WebContents* contents) {
    gfx::NativeView view = contents->GetNativeView();
    *out_event_location = gfx::PointF(web_contents_relative_location);
    gfx::Point root_location = web_contents_relative_location;
    aura::Window::ConvertPointToTarget(view, view->GetRootWindow(),
                                       &root_location);
    *out_event_root_location = gfx::PointF(root_location);
  }

  std::unique_ptr<DlpScopedFileAccessDelegate> delegate_;
};

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateInteractiveUITest,
                       FileDropped_UploadAllowed) {
  chromeos::FakeDlpClient* fake_dlp_client =
      static_cast<chromeos::FakeDlpClient*>(chromeos::DlpClient::Get());
  fake_dlp_client->SetFileAccessAllowed(true);

  ASSERT_TRUE(NavigateToTestPage("localhost", kInputDragNDropPagePath));

  content::WebContentsConsoleObserver console_observer(web_contents());
  // The console log should contain the actual file content.
  console_observer.SetPattern(kFileContent);

  base::FilePath file =
      CreateTestFileInDirectory(temp_dir_.GetPath(), kFileContent);

  ASSERT_TRUE(SimulateFileDrop(web_contents(), file, kMiddleOfDropArea));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateInteractiveUITest,
                       FileDropped_UploadDenied) {
  chromeos::FakeDlpClient* fake_dlp_client =
      static_cast<chromeos::FakeDlpClient*>(chromeos::DlpClient::Get());
  fake_dlp_client->SetFileAccessAllowed(false);

  ASSERT_TRUE(NavigateToTestPage("localhost", kInputDragNDropPagePath));

  content::WebContentsConsoleObserver console_observer(web_contents());
  // The console log should contain an error message because the file is not
  // accessible.
  console_observer.SetPattern(kErrorMessage);

  base::FilePath file =
      CreateTestFileInDirectory(temp_dir_.GetPath(), kFileContent);

  ASSERT_TRUE(SimulateFileDrop(web_contents(), file, kMiddleOfDropArea));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateInteractiveUITest,
                       FilePasted_UploadAllowed) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  // The console log should contain the actual file content.
  console_observer.SetPattern(kFileContent);

  chromeos::FakeDlpClient* fake_dlp_client =
      static_cast<chromeos::FakeDlpClient*>(chromeos::DlpClient::Get());
  fake_dlp_client->SetFileAccessAllowed(true);

  ASSERT_TRUE(NavigateToTestPage("localhost", kInputCopyPastePagePath));

  base::FilePath file =
      CreateTestFileInDirectory(temp_dir_.GetPath(), kFileContent);

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ClickOnView(browser_view->contents_container());

  SimulateFilePaste(web_contents(), file);

  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(DlpScopedFileAccessDelegateInteractiveUITest,
                       FilePasted_UploadDenied) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  // The console log should contain an error message because the file is not
  // accessible.
  console_observer.SetPattern(kErrorMessage);

  chromeos::FakeDlpClient* fake_dlp_client =
      static_cast<chromeos::FakeDlpClient*>(chromeos::DlpClient::Get());
  fake_dlp_client->SetFileAccessAllowed(false);

  ASSERT_TRUE(NavigateToTestPage("localhost", kInputCopyPastePagePath));

  base::FilePath file =
      CreateTestFileInDirectory(temp_dir_.GetPath(), kFileContent);

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ClickOnView(browser_view->contents_container());

  SimulateFilePaste(web_contents(), file);

  EXPECT_TRUE(console_observer.Wait());
}

}  // namespace policy
