// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

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
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state/login_state.h"
#endif  // defined(OS_CHROMEOS)

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
 public:
  ExtensionPageCaptureApiTest() {
    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker)
      current_channel_ = std::make_unique<ScopedWorkerBasedExtensionsChannel>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool RunTest(const std::string& extension_name) {
    return RunTestWithArg(extension_name, nullptr);
  }

  bool RunTestWithArg(const std::string& extension_name,
                      const char* custom_arg) {
    return RunTestWithFlagsAndArg(extension_name, custom_arg,
                                  kFlagEnableFileAccess);
  }

  bool RunTestWithFlagsAndArg(const std::string& extension_name,
                              const char* custom_arg,
                              int browser_test_flags) {
    if (GetParam() == ContextType::kServiceWorker)
      browser_test_flags |= kFlagRunAsServiceWorkerBasedExtension;

    return RunExtensionTestWithFlagsAndArg(extension_name, custom_arg,
                                           browser_test_flags, kFlagNone);
  }

  void WaitForFileCleanup(PageCaptureSaveAsMHTMLDelegate* delegate) {
    // Garbage collection in SW-based extensions doesn't clean up the temp
    // file.
    if (GetParam() != ContextType::kServiceWorker)
      delegate->WaitForFinalRelease();
  }

 private:
  std::unique_ptr<ScopedWorkerBasedExtensionsChannel> current_channel_;
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
  ASSERT_TRUE(RunTestWithFlagsAndArg("page_capture",
                                     "ONLY_PAGE_CAPTURE_PERMISSION", kFlagNone))
      << message_;
  WaitForFileCleanup(&delegate);
}

IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest, SaveAsMHTMLWithFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture")) << message_;
  WaitForFileCleanup(&delegate);
}

#if defined(OS_CHROMEOS)
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
  ASSERT_TRUE(RunTestWithArg("page_capture", "REQUEST_DENIED")) << message_;
  EXPECT_EQ(0, delegate.temp_file_count());
}
#endif  // defined(OS_CHROMEOS)

}  // namespace extensions
