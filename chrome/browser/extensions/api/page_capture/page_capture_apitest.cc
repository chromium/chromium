// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

class PageCaptureSaveAsMHTMLDelegate
    : public PageCaptureSaveAsMHTMLFunction::TestDelegate {
 public:
  PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(this);
  }

  virtual ~PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(nullptr);
  }

  void OnTemporaryFileCreated(
      scoped_refptr<storage::ShareableFileReference> file) override {
    file->AddFinalReleaseCallback(
        base::BindOnce(&PageCaptureSaveAsMHTMLDelegate::OnReleaseCallback,
                       weak_factory_.GetWeakPtr()));
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
  base::WeakPtrFactory<PageCaptureSaveAsMHTMLDelegate> weak_factory_{this};
};

class ExtensionPageCaptureApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool RunTest(const char* extension_name,
               const char* custom_arg = nullptr,
               bool allow_file_access = false) {
    return RunExtensionTest(extension_name, {.custom_arg = custom_arg},
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

// TODO(crbug.com/326868086, crbug.com/342254075): Test is flaky on MSan and on
// Windows.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_WIN)
#define MAYBE_SaveAsMHTMLWithoutFileAccess DISABLED_SaveAsMHTMLWithoutFileAccess
#else
#define MAYBE_SaveAsMHTMLWithoutFileAccess SaveAsMHTMLWithoutFileAccess
#endif
IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest,
                       MAYBE_SaveAsMHTMLWithoutFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture", "ONLY_PAGE_CAPTURE_PERMISSION"))
      << message_;
  WaitForFileCleanup(&delegate);
}

// TODO(crbug.com/326868086): Test is flaky.
IN_PROC_BROWSER_TEST_P(ExtensionPageCaptureApiTest,
                       DISABLED_SaveAsMHTMLWithFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture", /*custom_arg=*/nullptr,
                      /*allow_file_access=*/true))
      << message_;
  WaitForFileCleanup(&delegate);
}

}  // namespace extensions
