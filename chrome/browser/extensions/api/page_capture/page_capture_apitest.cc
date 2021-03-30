// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/login/login_state/login_state.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

class PageCaptureSaveAsMHTMLDelegate
    : public PageCaptureSaveAsMHTMLFunction::TestDelegate {
 public:
  PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(this);
  }

  virtual ~PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(NULL);
  }

  void OnTemporaryFileCreated(
      scoped_refptr<storage::ShareableFileReference> file) override {
    file->AddFinalReleaseCallback(
        base::BindOnce(&PageCaptureSaveAsMHTMLDelegate::OnReleaseCallback,
                       base::Unretained(this)));
    ++temp_file_count_;
  }

  void WaitForFinalRelease() {
    if (temp_file_count_ > 0)
      run_loop_.Run();
  }

  int temp_file_count() const { return temp_file_count_; }

 private:
  void OnReleaseCallback(const base::FilePath& path) {
    if (--temp_file_count_ == 0)
      release_closure_.Run();
  }

  base::RunLoop run_loop_;
  base::RepeatingClosure release_closure_ = run_loop_.QuitClosure();
  std::atomic<int> temp_file_count_{0};
};

class ExtensionPageCaptureApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool RunTest(const char* extension_name,
               const char* custom_arg = nullptr,
               bool allow_file_access = false) {
    return RunExtensionTest({.name = extension_name, .custom_arg = custom_arg},
                            {.allow_file_access = allow_file_access});
  }
  void WaitForFileCleanup(PageCaptureSaveAsMHTMLDelegate* delegate) {
    // Garbage collection in SW-based extensions doesn't clean up the temp
    // file.
    if (GetParam() != ContextType::kServiceWorker)
      delegate->WaitForFinalRelease();
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionPageCaptureApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionPageCaptureApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest,
                       SaveAsMHTMLWithoutFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture", "ONLY_PAGE_CAPTURE_PERMISSION"))
      << message_;
  WaitForFileCleanup(&delegate);
}

IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest, SaveAsMHTMLWithFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture", /*custom_arg=*/nullptr,
                      /*allow_file_access=*/true))
      << message_;
  WaitForFileCleanup(&delegate);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest,
                       PublicSessionRequestAllowed) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  chromeos::ScopedTestPublicSessionLoginState login_state;
  // Resolve Permission dialog with Allow.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(RunTest("page_capture")) << message_;
  WaitForFileCleanup(&delegate);
}

IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest,
                       PublicSessionRequestDenied) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  chromeos::ScopedTestPublicSessionLoginState login_state;
  // Resolve Permission dialog with Deny.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::CANCEL);
  ASSERT_TRUE(RunTest("page_capture", "REQUEST_DENIED")) << message_;
  EXPECT_EQ(0, delegate.temp_file_count());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
