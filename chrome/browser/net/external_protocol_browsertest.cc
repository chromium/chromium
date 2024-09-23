// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// DefaultSchemeClientWorker checks whether the browser is set as the default
// handler for some scheme, and optionally sets the browser as the default
// handler for some scheme. Our fake implementation pretends that the browser is
// not the default handler.
class FakeDefaultSchemeClientWorker
    : public shell_integration::DefaultSchemeClientWorker {
 public:
  explicit FakeDefaultSchemeClientWorker(const GURL& url)
      : DefaultSchemeClientWorker(url) {}
  FakeDefaultSchemeClientWorker(const FakeDefaultSchemeClientWorker&) = delete;
  FakeDefaultSchemeClientWorker& operator=(
      const FakeDefaultSchemeClientWorker&) = delete;

 private:
  ~FakeDefaultSchemeClientWorker() override = default;
  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return shell_integration::DefaultWebClientState::NOT_DEFAULT;
  }

  std::u16string GetDefaultClientNameImpl() override { return u"TestApp"; }

  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_finished_callback));
  }
};

// Used during testing to intercept invocations of external protocol handlers.
class FakeProtocolHandlerDelegate : public ExternalProtocolHandler::Delegate {
 public:
  FakeProtocolHandlerDelegate() = default;
  FakeProtocolHandlerDelegate(const FakeProtocolHandlerDelegate&) = delete;
  FakeProtocolHandlerDelegate& operator=(const FakeProtocolHandlerDelegate&) =
      delete;

  const GURL& WaitForUrl() {
    run_loop_.Run();
    return url_invoked_;
  }

 private:
  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    return base::MakeRefCounted<FakeDefaultSchemeClientWorker>(url);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::BlockState::DONT_BLOCK;
  }

  void BlockRequest() override { NOTREACHED_IN_MIGRATION(); }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {
    NOTREACHED_IN_MIGRATION();
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
};

// Navigates to the |target_url| and waits until that same URL is observed at
// the ExternalProtocolHandler.
void InvokeUrlAndWaitForExternalHandler(Browser* browser, GURL target_url) {
  FakeProtocolHandlerDelegate external_handler_delegate;
  ExternalProtocolHandler::SetDelegateForTesting(&external_handler_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, target_url));
  auto actual_url = external_handler_delegate.WaitForUrl();
  EXPECT_EQ(target_url, actual_url);

  ExternalProtocolHandler::SetDelegateForTesting(nullptr);
}

}  // namespace

// TODO(mmenke): Should these be merged into
// chrome/browser/external_porotocol_handler_browsertest.cc?
using ExternalProtocolBrowserTest = InProcessBrowserTest;

// Ensure that ftp:// URLs are passed through to the external protocol handler.
IN_PROC_BROWSER_TEST_F(ExternalProtocolBrowserTest, ExternalProtocolHandler) {
  // If this test fails, then the issue is with the external protocol handler
  // mechanism as configured by //chrome. This test must pass for the test below
  // it to be valid.
  InvokeUrlAndWaitForExternalHandler(browser(), GURL("example-not-real:foo"));
  InvokeUrlAndWaitForExternalHandler(browser(), GURL("gopher://foo.example"));

  // And now with an ftp:// URL.
  InvokeUrlAndWaitForExternalHandler(browser(),
                                     GURL("ftp://example.com/foo/bar"));
}
