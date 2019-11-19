// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Whether FTP is enabled or not.
enum class FtpState { ENABLED, DISABLED };

class FtpBrowserTest : public InProcessBrowserTest {
 public:
  explicit FtpBrowserTest(FtpState ftp_state = FtpState::ENABLED)
      : ftp_server_(net::SpawnedTestServer::TYPE_FTP,
                    base::FilePath(FILE_PATH_LITERAL("chrome/test/data/ftp"))) {
    scoped_feature_list_.InitWithFeatureState(features::kFtpProtocol,
                                              ftp_state == FtpState::ENABLED);
  }

 protected:
  net::SpawnedTestServer ftp_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void WaitForTitle(content::WebContents* contents, const char* expected_title) {
  content::TitleWatcher title_watcher(contents,
      base::ASCIIToUTF16(expected_title));

  EXPECT_EQ(base::ASCIIToUTF16(expected_title),
            title_watcher.WaitAndGetTitle());
}

// DefaultProtocolClientWorker checks whether the browser is set as the default
// handler for some scheme, and optionally sets the browser as the default
// handler for some scheme. Our fake implementation pretends that the browser is
// not the default handler.
class FakeDefaultProtocolClientWorker
    : public shell_integration::DefaultProtocolClientWorker {
 public:
  FakeDefaultProtocolClientWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol)
      : DefaultProtocolClientWorker(callback, protocol) {}

 private:
  ~FakeDefaultProtocolClientWorker() override = default;
  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return shell_integration::DefaultWebClientState::NOT_DEFAULT;
  }

  void SetAsDefaultImpl(const base::Closure& on_finished_callback) override {
    base::GetContinuationTaskRunner()->PostTask(FROM_HERE,
                                                on_finished_callback);
  }

  DISALLOW_COPY_AND_ASSIGN(FakeDefaultProtocolClientWorker);
};

// Used during testing to intercept invocations of external protocol handlers.
class FakeProtocolHandlerDelegate : public ExternalProtocolHandler::Delegate {
 public:
  FakeProtocolHandlerDelegate() = default;

  const GURL& WaitForUrl() {
    run_loop_.Run();
    return url_invoked_;
  }

 private:
  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    return base::MakeRefCounted<FakeDefaultProtocolClientWorker>(callback,
                                                                 protocol);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::BlockState::DONT_BLOCK;
  }

  void BlockRequest() override { NOTREACHED(); }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin) override {
    NOTREACHED();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    EXPECT_TRUE(url_invoked_.is_empty());
    url_invoked_ = url;
    run_loop_.Quit();
  }

  void FinishedProcessingCheck() override {}

  GURL url_invoked_;
  base::RunLoop run_loop_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FakeProtocolHandlerDelegate);
};

// Navigates to the |target_url| and waits until that same URL is observed at
// the ExternalProtocolHandler.
void InvokeUrlAndWaitForExternalHandler(Browser* browser, GURL target_url) {
  FakeProtocolHandlerDelegate external_handler_delegate;
  ExternalProtocolHandler::SetDelegateForTesting(&external_handler_delegate);

  ui_test_utils::NavigateToURL(browser, target_url);
  auto actual_url = external_handler_delegate.WaitForUrl();
  EXPECT_EQ(target_url, actual_url);

  ExternalProtocolHandler::SetDelegateForTesting(nullptr);
}

// Test fixture where FTP is disabled.
class FtpDisabledFeatureBrowserTest : public FtpBrowserTest {
 public:
  FtpDisabledFeatureBrowserTest() : FtpBrowserTest(FtpState::DISABLED) {}
};

class FtpEnabledBySwitchBrowserTest : public FtpBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableFtp);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FtpBrowserTest, BasicFtpUrlAuthentication) {
  ASSERT_TRUE(ftp_server_.Start());
  ui_test_utils::NavigateToURL(
      browser(),
      ftp_server_.GetURLWithUserAndPassword("", "chrome", "chrome"));

  WaitForTitle(browser()->tab_strip_model()->GetActiveWebContents(),
               "Index of /");
}

IN_PROC_BROWSER_TEST_F(FtpBrowserTest, DirectoryListingNavigation) {
  ftp_server_.set_no_anonymous_ftp_user(true);
  ASSERT_TRUE(ftp_server_.Start());

  ui_test_utils::NavigateToURL(
      browser(),
      ftp_server_.GetURLWithUserAndPassword("", "chrome", "chrome"));

  // Navigate to directory dir1/ without needing to re-authenticate
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "(function() {"
      "  function navigate() {"
      "    for (const element of document.getElementsByTagName('a')) {"
      "      if (element.innerHTML == 'dir1/') {"
      "        element.click();"
      "      }"
      "    }"
      "  }"
      "  if (document.readyState === 'loading') {"
      "    document.addEventListener('DOMContentLoaded', navigate);"
      "  } else {"
      "    navigate();"
      "  }"
      "})()"));

  WaitForTitle(browser()->tab_strip_model()->GetActiveWebContents(),
               "Index of /dir1/");

  // Navigate to file `test.html`, verify that it's downloaded.
  content::DownloadTestObserverTerminal download_test_observer_terminal(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "(function() {"
      "  function navigate() {"
      "    for (const element of document.getElementsByTagName('a')) {"
      "      if (element.innerHTML == 'test.html') {"
      "        element.click();"
      "      }"
      "    }"
      "  }"
      "  if (document.readyState === 'loading') {"
      "    document.addEventListener('DOMContentLoaded', navigate);"
      "  } else {"
      "    navigate();"
      "  }"
      "})()"));

  download_test_observer_terminal.WaitForFinished();
  EXPECT_EQ(download_test_observer_terminal.NumDownloadsSeenInState(
                download::DownloadItem::COMPLETE),
            1u);
}

// Ensure that ftp:// URLs are passed through to the external protocol handler
// when the FTP feature is disabled.
IN_PROC_BROWSER_TEST_F(FtpDisabledFeatureBrowserTest, ExternalProtocolHandler) {
  // If this test fails, then the issue is with the external protocol handler
  // mechanism as configured by //chrome. This test must pass for the test below
  // it to be valid.
  InvokeUrlAndWaitForExternalHandler(browser(), GURL("example-not-real:foo"));
  InvokeUrlAndWaitForExternalHandler(browser(), GURL("gopher://foo.example"));

  // And now with an ftp:// URL.
  InvokeUrlAndWaitForExternalHandler(browser(),
                                     GURL("ftp://example.com/foo/bar"));
}

// Did I wire the switch correctly?
IN_PROC_BROWSER_TEST_F(FtpEnabledBySwitchBrowserTest, SwitchWorks) {
  ASSERT_TRUE(
      base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kFtpProtocol.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE));
}
