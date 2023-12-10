// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include "build/build_config.h"

// Native headless is currently available on Linux, Windows and Mac platforms.
// More platforms will be added later, so avoid function level clutter by
// providing a compile time condition over the entire file.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/common/chrome_switches.h"
#include "components/headless/clipboard/headless_clipboard.h"     // nogncheck
#include "components/infobars/content/content_infobar_manager.h"  // nogncheck
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/dialog_model.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using testing::Ge;
using testing::SizeIs;
using views::Widget;

namespace headless {

namespace switches {
// This switch runs tests in headful mode, intended for experiments only because
// not all tests are expected to pass in headful mode.
static const char kHeadfulMode[] = "headful-mode";
}  // namespace switches

HeadlessModeBrowserTest::HeadlessModeBrowserTest() {
  base::FilePath test_data(
      FILE_PATH_LITERAL("chrome/browser/headless/test/data"));
  embedded_test_server()->AddDefaultHandlers(test_data);
}

void HeadlessModeBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  AppendHeadlessCommandLineSwitches(command_line);
}

void HeadlessModeBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(headless::IsHeadlessMode() || headful_mode());
}

void HeadlessModeBrowserTest::AppendHeadlessCommandLineSwitches(
    base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kHeadfulMode)) {
    headful_mode_ = true;
  } else {
    command_line->AppendSwitchASCII(::switches::kHeadless,
                                    kHeadlessSwitchValue);
    headless::InitHeadlessMode();
  }
}

content::WebContents* HeadlessModeBrowserTest::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void HeadlessModeBrowserTestWithUserDataDir::SetUpCommandLine(
    base::CommandLine* command_line) {
  ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(base::IsDirectoryEmpty(user_data_dir()));
  command_line->AppendSwitchPath(::switches::kUserDataDir, user_data_dir());

  AppendHeadlessCommandLineSwitches(command_line);
}

void HeadlessModeBrowserTestWithStartWindowMode::SetUpCommandLine(
    base::CommandLine* command_line) {
  HeadlessModeBrowserTest::SetUpCommandLine(command_line);

  switch (start_window_mode()) {
    case kStartWindowNormal:
      break;
    case kStartWindowMaximized:
      command_line->AppendSwitch(::switches::kStartMaximized);
      break;
    case kStartWindowFullscreen:
      command_line->AppendSwitch(::switches::kStartFullscreen);
      break;
  }
}

void ToggleFullscreenModeSync(Browser* browser) {
  FullscreenNotificationObserver observer(browser);
  chrome::ToggleFullscreenMode(browser);
  observer.Wait();
}

void HeadlessModeBrowserTestWithWindowSize::SetUpCommandLine(
    base::CommandLine* command_line) {
  HeadlessModeBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(
      ::switches::kWindowSize,
      base::StringPrintf("%u,%u", kWindowSize.width(), kWindowSize.height()));
}

void HeadlessModeBrowserTestWithWindowSizeAndScale::SetUpCommandLine(
    base::CommandLine* command_line) {
  HeadlessModeBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(
      ::switches::kWindowSize,
      base::StringPrintf("%u,%u", kWindowSize.width(), kWindowSize.height()));
  command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor, "1.5");
}

namespace {

// Miscellaneous tests -------------------------------------------------------

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, BrowserWindowIsActive) {
  EXPECT_TRUE(browser()->window()->IsActive());
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, NoInfoBarInHeadless) {
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_manager);

  PageInfoInfoBarDelegate::Create(infobar_manager);

  EXPECT_THAT(infobar_manager->infobars(), testing::IsEmpty());
}

// UserAgent tests -----------------------------------------------------------

class HeadlessModeUserAgentBrowserTest : public HeadlessModeBrowserTest {
 public:
  HeadlessModeUserAgentBrowserTest() = default;
  ~HeadlessModeUserAgentBrowserTest() override = default;

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HeadlessModeUserAgentBrowserTest::RequestHandler,
                            base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server()->Start());

    HeadlessModeBrowserTest::SetUp();
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/page.html") {
      headers_ = request.headers;

      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&HeadlessModeUserAgentBrowserTest::FinishTest,
                         base::Unretained(this)));

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(R"(<div>Hi, I'm headless!</div>)");

      return response;
    }

    return nullptr;
  }

  void RunLoop() {
    if (!test_complete_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  void FinishTest() {
    test_complete_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  bool test_complete_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  net::test_server::HttpRequest::HeaderMap headers_;
};

IN_PROC_BROWSER_TEST_F(HeadlessModeUserAgentBrowserTest, UserAgentHasHeadless) {
  content::BrowserContext* browser_context = browser()->profile();
  DCHECK(browser_context);

  content::WebContents::CreateParams create_params(browser_context);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  DCHECK(web_contents);

  GURL url = embedded_test_server()->GetURL("/page.html");
  content::NavigationController::LoadURLParams params(url);
  web_contents->GetController().LoadURLWithParams(params);

  RunLoop();

  web_contents->Close();
  web_contents.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(headers_.at("User-Agent"), testing::HasSubstr("HeadlessChrome/"));
}

// Incognito mode tests ------------------------------------------------------

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithUserDataDir,
                       StartWithUserDataDir) {
  // With user data dir expect to start in non incognito mode.
  EXPECT_FALSE(browser()->profile()->IsOffTheRecord());
}

class HeadlessModeBrowserTestWithUserDataDirAndIncognito
    : public HeadlessModeBrowserTestWithUserDataDir {
 public:
  HeadlessModeBrowserTestWithUserDataDirAndIncognito() = default;
  ~HeadlessModeBrowserTestWithUserDataDirAndIncognito() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeBrowserTestWithUserDataDir::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kIncognito);
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithUserDataDirAndIncognito,
                       StartWithUserDataDirAndIncognito) {
  // With user data dir and incognito expect to start in incognito mode.
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());
}

// Clipboard tests -----------------------------------------------------------

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, HeadlessClipboardInstalled) {
  ui::Clipboard* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);

  ui::ClipboardBuffer buffer = ui::ClipboardBuffer::kCopyPaste;
  ASSERT_TRUE(ui::Clipboard::IsSupportedClipboardBuffer(buffer));

  // Expect sequence number to be incremented. This confirms that the headless
  // clipboard implementation is being used.
  int request_counter = GetSequenceNumberRequestCounterForTesting();
  clipboard->GetSequenceNumber(buffer);
  EXPECT_GT(GetSequenceNumberRequestCounterForTesting(), request_counter);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, HeadlessClipboardCopyPaste) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ASSERT_TRUE(clipboard);

  static const struct ClipboardBufferInfo {
    ui::ClipboardBuffer buffer;
    std::u16string paste_text;
  } clipboard_buffers[] = {
      {ui::ClipboardBuffer::kCopyPaste, u"kCopyPaste"},
      {ui::ClipboardBuffer::kSelection, u"kSelection"},
      {ui::ClipboardBuffer::kDrag, u"kDrag"},
  };

  // Check basic write/read ops into each buffer type.
  for (const auto& [buffer, paste_text] : clipboard_buffers) {
    if (!ui::Clipboard::IsSupportedClipboardBuffer(buffer)) {
      continue;
    }
    {
      ui::ScopedClipboardWriter writer(buffer);
      writer.WriteText(paste_text);
    }
    std::u16string copy_text;
    clipboard->ReadText(buffer, /* data_dst = */ nullptr, &copy_text);
    EXPECT_EQ(paste_text, copy_text);
  }

  // Verify that different clipboard buffer data is independent.
  for (const auto& [buffer, paste_text] : clipboard_buffers) {
    if (!ui::Clipboard::IsSupportedClipboardBuffer(buffer)) {
      continue;
    }
    std::u16string copy_text;
    clipboard->ReadText(buffer, /* data_dst = */ nullptr, &copy_text);
    EXPECT_EQ(paste_text, copy_text);
  }
}

// Bubble tests --------------------------------------------------------------

class TestBubbleDelegate : public ui::DialogModelDelegate {
 public:
  TestBubbleDelegate() = default;
  ~TestBubbleDelegate() override = default;

  void OnOkButton() { dialog_model()->host()->Close(); }
};

Widget* ShowTestBubble(Browser* browser) {
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAppMenuButton();

  auto bubble_delegate = std::make_unique<TestBubbleDelegate>();
  TestBubbleDelegate* bubble_delegate_ptr = bubble_delegate.get();

  ui::DialogModel::Builder dialog_builder(std::move(bubble_delegate));
  dialog_builder.SetTitle(std::u16string(u"Test bubble"))
      .AddParagraph(ui::DialogModelLabel(std::u16string(u"Test bubble text")))
      .AddOkButton(
          base::BindOnce(&TestBubbleDelegate::OnOkButton,
                         base::Unretained(bubble_delegate_ptr)),
          ui::DialogModelButton::Params().SetLabel(std::u16string(u"OK")));

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);

  Widget* widget = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  widget->Show();

  return widget;
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, HeadlessBubbleVisibility) {
  // Desktop window widget should be headless.
  gfx::NativeWindow desktop_window = browser()->window()->GetNativeWindow();
  Widget* desktop_widget = Widget::GetWidgetForNativeWindow(desktop_window);
  ASSERT_TRUE(desktop_widget);
  ASSERT_TRUE(desktop_widget->is_headless());

  // Bubble widget is expected to be headless.
  Widget* bubble_widget = ShowTestBubble(browser());
  EXPECT_TRUE(bubble_widget->is_headless());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // On Windows and Mac in headless mode we still have actual platform
  // windows which are always hidden, so verify that they are not visible.
  EXPECT_FALSE(IsPlatformWindowVisible(bubble_widget));
#elif BUILDFLAG(IS_LINUX)
  // On Linux headless mode uses Ozone/Headless where platform windows are not
  // backed up by any real windows, so verify that their visibility state
  // matches the widget's visibility state.
  EXPECT_EQ(IsPlatformWindowVisible(bubble_widget), bubble_widget->IsVisible());
#else
#error Unsupported platform
#endif
}

}  // namespace

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
