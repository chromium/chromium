// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state/login_state.h"
#endif  // defined(OS_CHROMEOS)

using extensions::Extension;
using extensions::ExtensionActionRunner;
using extensions::PageCaptureSaveAsMHTMLFunction;
using extensions::ResultCatcher;
using extensions::ScopedTestDialogAutoConfirm;

class ExtensionPageCaptureApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

class PageCaptureSaveAsMHTMLDelegate
    : public PageCaptureSaveAsMHTMLFunction::TestDelegate {
 public:
  PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(this);
  }

  virtual ~PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(NULL);
  }

  void OnTemporaryFileCreated(const base::FilePath& temp_file) override {
    temp_file_ = temp_file;
  }

  base::FilePath temp_file_;
};

// TODO(crbug.com/961017): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_SaveAsMHTMLWithActiveTabWithFileAccess \
  DISABLED_SaveAsMHTMLWithActiveTabWithFileAccess
#else
#define MAYBE_SaveAsMHTMLWithActiveTabWithFileAccess \
  SaveAsMHTMLWithActiveTabWithFileAccess
#endif

// TODO(crbug.com/961017): Fix memory leaks in tests and re-enable on LSAN.
// Also flaky-failing on slow (debug) bots: https://crbug.com/1017305
#if defined(LEAK_SANITIZER) || !defined(NDEBUG) || \
    defined(ADDRESS_SANITIZER) || defined(OS_WIN)
#define MAYBE_SaveAsMHTML DISABLED_SaveAsMHTML
#else
#define MAYBE_SaveAsMHTML SaveAsMHTML
#endif

IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest, MAYBE_SaveAsMHTML) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunExtensionTestWithFlagsAndArg(
      "page_capture", "ONLY_PAGE_CAPTURE_PERMISSION", kFlagNone))
      << message_;
  // Make sure the MHTML data gets written to the temporary file.
  ASSERT_FALSE(delegate.temp_file_.empty());
  // Flush the message loops to make sure the delete happens.
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  // Make sure the temporary file is destroyed once the javascript side reads
  // the contents.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(delegate.temp_file_));
}

IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest,
                       MAYBE_SaveAsMHTMLWithActiveTabWithFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunExtensionTest("page_capture")) << message_;
  // Make sure the MHTML data gets written to the temporary file.
  ASSERT_FALSE(delegate.temp_file_.empty());
  // Flush the message loops to make sure the delete happens.
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  // Make sure the temporary file is destroyed once the javascript side reads
  // the contents.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(delegate.temp_file_));
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest,
                       PublicSessionRequestAllowed) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  chromeos::ScopedTestPublicSessionLoginState login_state;
  // Resolve Permission dialog with Allow.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(RunExtensionTest("page_capture")) << message_;
  ASSERT_FALSE(delegate.temp_file_.empty());
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(delegate.temp_file_));
}

IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest,
                       PublicSessionRequestDenied) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  chromeos::ScopedTestPublicSessionLoginState login_state;
  // Resolve Permission dialog with Deny.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::CANCEL);
  ASSERT_TRUE(RunExtensionTestWithArg("page_capture", "REQUEST_DENIED"))
      << message_;
}
#endif  // defined(OS_CHROMEOS)
