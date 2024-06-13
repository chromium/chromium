// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/containers/flat_map.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/image/image_unittest_util.h"

void CopyTextItem() {
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"test");
  }
  base::RunLoop().RunUntilIdle();
}

void CopyBitmapItem() {
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    SkBitmap input_bitmap = gfx::test::CreateBitmap(3, 2);
    scw.WriteImage(input_bitmap);
  }
  base::RunLoop().RunUntilIdle();
}

void CopyFileItem() {
  const base::flat_map<std::u16string, std::u16string> input_data = {
      {u"fs/sources", u"/path/to/My%20File.txt"}};
  base::Pickle input_data_pickle;
  ui::WriteCustomDataToPickle(input_data, &input_data_pickle);
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WritePickledData(input_data_pickle,
                         ui::ClipboardFormatType::DataTransferCustomType());
  }
  base::RunLoop().RunUntilIdle();
}

class VirtualKeyboardPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  VirtualKeyboardPrivateApiTest() = default;
  ~VirtualKeyboardPrivateApiTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/ash/clipboard_history"));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void CopyHtmlItem() {
    // Load the web page which contains images and text.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/image-and-text.html")));

    // Select one part of the web page. Wait until the selection region updates.
    // Then copy the selected part to clipboard.
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    content::BoundingBoxUpdateWaiter select_waiter(web_contents);
    ASSERT_TRUE(ExecJs(web_contents, "selectPart1();"));
    select_waiter.Wait();
    ASSERT_TRUE(ExecJs(web_contents, "copyToClipboard();"));
    base::RunLoop().RunUntilIdle();
  }
};

// TODO(crbug.com/1352320): Flaky on release bots.
#if defined(NDEBUG)
#define MAYBE_Multipaste DISABLED_Multipaste
#else
#define MAYBE_Multipaste Multipaste
#endif
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPrivateApiTest, MAYBE_Multipaste) {
  // Copy to the clipboard an item of each display format type.
  CopyHtmlItem();
  CopyTextItem();
  CopyBitmapItem();
  CopyFileItem();

  ASSERT_TRUE(RunExtensionTest("virtual_keyboard_private",
                               {.custom_arg = "multipaste"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardPrivateApiTest, MultipasteLockedScreen) {
  // Copy to the clipboard an item of each display format type.
  CopyHtmlItem();
  CopyTextItem();
  CopyBitmapItem();
  CopyFileItem();

  // Verify that no clipboard items are returned when the screen is locked.
  ash::ScreenLockerTester tester;
  tester.Lock();

  ASSERT_TRUE(RunExtensionTest("virtual_keyboard_private",
                               {.custom_arg = "multipasteUnderLockScreen"},
                               {.load_as_component = true}))
      << message_;
}
