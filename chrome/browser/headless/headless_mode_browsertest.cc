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
#include "chrome/common/chrome_switches.h"
#include "components/headless/clipboard/headless_clipboard.h"  // nogncheck
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
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"
#include "url/gurl.h"

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

}  // namespace

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
