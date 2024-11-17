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

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/headless/headless_mode_browsertest_utils.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/headless/clipboard/headless_clipboard.h"     // nogncheck
#include "components/infobars/content/content_infobar_manager.h"  // nogncheck
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobars_switches.h"
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

bool HeadlessModeBrowserTest::IsIncognito() {
  return false;
}

void HeadlessModeBrowserTest::AppendHeadlessCommandLineSwitches(
    base::CommandLine* command_line) {
  if (IsIncognito()) {
    command_line->AppendSwitch(::switches::kIncognito);
  }
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

base::FilePath HeadlessModeBrowserTestWithUserDataDir::GetUserDataDir() const {
  // InProcessBrowserTest class HeadlessModeBrowserTest is derived from
  // guarantees that user data dir exists.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir;
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

// Infobar tests -------------------------------------------------------------

class TestInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  TestInfoBarDelegate(const TestInfoBarDelegate&) = delete;
  TestInfoBarDelegate& operator=(const TestInfoBarDelegate&) = delete;

  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     int buttons) {
    infobar_manager->AddInfoBar(
        CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
            new TestInfoBarDelegate(buttons))));
  }

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return TEST_INFOBAR;
  }
  std::u16string GetMessageText() const override {
    return buttons_ ? u"BUTTON" : u"";
  }
  int GetButtons() const override { return buttons_; }

 private:
  explicit TestInfoBarDelegate(int buttons) : buttons_(buttons) {}
  ~TestInfoBarDelegate() override = default;

  int buttons_;
};

class HeadlessModeInfobarBrowserTest
    : public HeadlessModeBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  HeadlessModeInfobarBrowserTest() = default;
  ~HeadlessModeInfobarBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeBrowserTest::SetUpCommandLine(command_line);
    if (disable_infobars()) {
      command_line->AppendSwitch(::switches::kDisableInfoBars);
    }
  }

  bool disable_infobars() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HeadlessModeInfobarBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(HeadlessModeInfobarBrowserTest, InfoBarsCanBeDisabled) {
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_manager);

#if BUILDFLAG(CHROME_FOR_TESTING)
  // Chrome for Testing shows its disclaimer infobar upon start.
  infobar_manager->RemoveAllInfoBars(/*animate=*/false);
#endif

  ASSERT_THAT(infobar_manager->infobars(), testing::IsEmpty());

  TestInfoBarDelegate::Create(infobar_manager,
                              ConfirmInfoBarDelegate::BUTTON_NONE);
  TestInfoBarDelegate::Create(infobar_manager,
                              ConfirmInfoBarDelegate::BUTTON_OK);

  // The infobar with a button should appear even if infobars are disabled.
  EXPECT_THAT(infobar_manager->infobars(),
              testing::SizeIs(disable_infobars() ? 1 : 2));
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
  // InProcessBrowserTest always provdies temporary user data dir.
  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  ASSERT_EQ(command_line.GetSwitchValuePath(::switches::kUserDataDir),
            GetUserDataDir());

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
  dialog_builder.SetTitle(u"Test bubble")
      .AddParagraph(ui::DialogModelLabel(u"Test bubble text"))
      .AddOkButton(base::BindOnce(&TestBubbleDelegate::OnOkButton,
                                  base::Unretained(bubble_delegate_ptr)),
                   ui::DialogModel::Button::Params().SetLabel(u"OK"));

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
  EXPECT_FALSE(test::IsPlatformWindowVisible(bubble_widget));
#elif BUILDFLAG(IS_LINUX)
  // On Linux headless mode uses Ozone/Headless where platform windows are not
  // backed up by any real windows, so verify that their visibility state
  // matches the widget's visibility state.
  EXPECT_EQ(test::IsPlatformWindowVisible(bubble_widget),
            bubble_widget->IsVisible());
#else
#error Unsupported platform
#endif
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, HeadlessBubbleSize) {
  Widget* bubble_widget = ShowTestBubble(browser());
  ASSERT_TRUE(bubble_widget->is_headless());

  gfx::Rect bounds = test::GetPlatformWindowExpectedBounds(bubble_widget);
  EXPECT_FALSE(bounds.IsEmpty());
}

}  // namespace

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
