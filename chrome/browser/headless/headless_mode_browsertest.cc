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
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chrome_process_singleton.h"
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
#include "testing/multiprocess_func_list.h"
#include "ui/base/clipboard/clipboard.h"
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

namespace {
const int kErrorResultCode = -1;
}  // namespace

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
    headless::SetUpCommandLine(command_line);
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

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithUserDataDir,
                       ChromeProcessSingletonExists) {
  // Pass the user data dir to the child process which will try
  // to create a mock ChromeProcessSingleton in it that is
  // expected to fail.
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  command_line.AppendSwitchPath(::switches::kUserDataDir, user_data_dir());

  base::Process child_process =
      base::SpawnMultiProcessTestChild("ChromeProcessSingletonChildProcessMain",
                                       command_line, base::LaunchOptions());

  int result = kErrorResultCode;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &result));

  EXPECT_EQ(static_cast<ProcessSingleton::NotifyResult>(result),
            ProcessSingleton::PROFILE_IN_USE);
}

MULTIPROCESS_TEST_MAIN(ChromeProcessSingletonChildProcessMain) {
  content::BrowserTaskEnvironment task_environment;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(::switches::kUserDataDir);
  if (user_data_dir.empty())
    return kErrorResultCode;

  ChromeProcessSingleton chrome_process_singleton(user_data_dir);
  ProcessSingleton::NotifyResult notify_result =
      chrome_process_singleton.NotifyOtherProcessOrCreate();

  return static_cast<int>(notify_result);
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

class HeadlessModeBrowserTestWithNoUserDataDir
    : public HeadlessModeBrowserTest {
 public:
  HeadlessModeBrowserTestWithNoUserDataDir() = default;
  ~HeadlessModeBrowserTestWithNoUserDataDir() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Postpone headless switch handling until after --user-data-dir switch is
    // removed in SetUpUserDataDirectory() so that headless switch processing
    // logic will not see it.
  }

  bool SetUpUserDataDirectory() override {
    // Chrome test suite adds --user-data-dir in (at least) two places: in
    // InProcessBrowserTest::SetUp() and in content::LaunchTests(), so there is
    // no good way to prevent its addition.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->RemoveSwitch(::switches::kUserDataDir);

    // Setup headless mode switches after we removed user data directory switch
    // so that incognito switches logic will be able to detect it.
    AppendHeadlessCommandLineSwitches(command_line);

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithNoUserDataDir,
                       StartWithNoUserDataDir) {
  // By default expect to start in incognito mode.
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());
}

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
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
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

  ui::ClipboardBuffer buffer = ui::ClipboardBuffer::kCopyPaste;
  ASSERT_TRUE(ui::Clipboard::IsSupportedClipboardBuffer(buffer));

  const std::u16string text = u"Clippy!";
  ui::ScopedClipboardWriter(buffer).WriteText(text);

  std::u16string copy_pasted_text;
  clipboard->ReadText(buffer, /*data_dst=*/nullptr, &copy_pasted_text);

  EXPECT_EQ(text, copy_pasted_text);
}

}  // namespace

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
