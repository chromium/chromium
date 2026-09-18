// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/switches.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

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
    if (temp_file_count_ > 0) {
      run_loop_.Run();
    }
  }

  int temp_file_count() const { return temp_file_count_; }

 private:
  void OnReleaseCallback(const base::FilePath& path) {
    if (--temp_file_count_ == 0) {
      release_closure_.Run();
    }
  }

  base::RunLoop run_loop_;
  base::RepeatingClosure release_closure_ = run_loop_.QuitClosure();
  std::atomic<int> temp_file_count_{0};
  base::WeakPtrFactory<PageCaptureSaveAsMHTMLDelegate> weak_factory_{this};
};

class ExtensionPageCaptureApiTest : public ExtensionApiTest {
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
};

// https://crbug.com/492228013: Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest,
                       DISABLED_SaveAsMHTMLWithoutFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture", "ONLY_PAGE_CAPTURE_PERMISSION"))
      << message_;
}

// https://crbug.com/492228013: Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest,
                       DISABLED_SaveAsMHTMLWithFileAccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunTest("page_capture", /*custom_arg=*/nullptr,
                      /*allow_file_access=*/true))
      << message_;
}

// Tests that chrome.pageCapture.saveAsMHTML excludes subframes belonging to
// other extensions unless activeTab permission has been granted, while
// preserving the capturing extension's own frames and the main document.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SaveAsMHTMLWithExtensionSubframe \
  DISABLED_SaveAsMHTMLWithExtensionSubframe
#else
#define MAYBE_SaveAsMHTMLWithExtensionSubframe SaveAsMHTMLWithExtensionSubframe
#endif
IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest,
                       MAYBE_SaveAsMHTMLWithExtensionSubframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load a first extension that exposes a web accessible resource widget.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(R"({
    "name": "First Extension",
    "version": "1.0",
    "manifest_version": 3,
    "web_accessible_resources": [{
      "resources": ["widget.html", "sandbox_widget.html"],
      "matches": ["<all_urls>"]
    }]
  })");
  ext_dir1.WriteFile(FILE_PATH_LITERAL("widget.html"),
                     "<p id='ext1'>ext1_sample_data</p>");
  ext_dir1.WriteFile(FILE_PATH_LITERAL("sandbox_widget.html"),
                     "<p id='ext1_sandbox'>ext1_sandbox_data</p>");
  const Extension* extension1 = LoadExtension(ext_dir1.UnpackedPath());
  ASSERT_TRUE(extension1);

  // Load a second extension with pageCapture and activeTab permissions.
  TestExtensionDir ext_dir2;
  ext_dir2.WriteManifest(R"({
    "name": "Second Extension",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["pageCapture", "tabs", "activeTab"],
    "background": {
      "service_worker": "bg.js"
    },
    "web_accessible_resources": [{
      "resources": ["own_widget.html"],
      "matches": ["<all_urls>"]
    }]
  })");
  ext_dir2.WriteFile(FILE_PATH_LITERAL("bg.js"), "// background");
  ext_dir2.WriteFile(FILE_PATH_LITERAL("own_widget.html"),
                     "<p id='ext2'>ext2_sample_data</p>");
  const Extension* extension2 = LoadExtension(ext_dir2.UnpackedPath());
  ASSERT_TRUE(extension2);

  // Navigate to a test page in the main tab.
  GURL page_url = embedded_test_server()->GetURL("example.com", "/title1.html");
  content::WebContents* main_tab = GetActiveWebContents();
  ASSERT_TRUE(main_tab);
  ASSERT_TRUE(content::NavigateToURL(main_tab, page_url));
  const int tab_id = ExtensionTabUtil::GetTabId(main_tab);

  // Embed extension iframes (including a sandboxed iframe with an opaque
  // origin whose precursor tuple is extension1) and wait for all to load.
  GURL ext1_widget_url = extension1->GetResourceURL("widget.html");
  GURL ext1_sandbox_url = extension1->GetResourceURL("sandbox_widget.html");
  GURL ext2_widget_url = extension2->GetResourceURL("own_widget.html");

  ASSERT_TRUE(content::ExecJs(
      main_tab, content::JsReplace(
                    R"(
            new Promise(resolve => {
              let loaded = 0;
              const onLoad = () => { if (++loaded === 3) resolve(); };

              const iframe1 = document.createElement('iframe');
              iframe1.onload = onLoad;
              iframe1.src = $1;
              document.body.appendChild(iframe1);

              const iframe2 = document.createElement('iframe');
              iframe2.sandbox = '';
              iframe2.onload = onLoad;
              iframe2.src = $2;
              document.body.appendChild(iframe2);

              const iframe3 = document.createElement('iframe');
              iframe3.onload = onLoad;
              iframe3.src = $3;
              document.body.appendChild(iframe3);
            })
          )",
                    ext1_widget_url, ext1_sandbox_url, ext2_widget_url)));

  auto capture_tab_fn = [this, extension2](int target_tab_id) -> std::string {
    static constexpr char kScript[] = R"(
      chrome.pageCapture.saveAsMHTML({tabId: %d}, async (blob) => {
        if (chrome.runtime.lastError) {
          chrome.test.sendScriptResult(
              'ERROR: ' + chrome.runtime.lastError.message);
          return;
        }
        if (!blob) {
          chrome.test.sendScriptResult('ERROR: No blob returned');
          return;
        }
        const text = await blob.text();
        chrome.test.sendScriptResult(text);
      });
    )";
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension2->id(), base::StringPrintf(kScript, target_tab_id),
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_TRUE(result.is_string());
    return result.is_string() ? result.GetString() : std::string();
  };

  // 1. Request pageCapture from the second extension without activeTab granted.
  std::string mhtml = capture_tab_fn(tab_id);

  EXPECT_THAT(mhtml, testing::Not(testing::StartsWith("ERROR:")));
  // The main page content should be present.
  EXPECT_THAT(mhtml, testing::HasSubstr("This page has no title."));
  // The capturing extension's own subframe should be included.
  EXPECT_THAT(mhtml, testing::HasSubstr("ext2_sample_data"));
  EXPECT_THAT(mhtml, testing::HasSubstr("own_widget.html"));
  // The other extension's subframes (both normal and sandboxed) should NOT be
  // included.
  EXPECT_THAT(mhtml, testing::Not(testing::HasSubstr("ext1_sample_data")));
  EXPECT_THAT(mhtml, testing::Not(testing::HasSubstr(ext1_widget_url.spec())));
  EXPECT_THAT(mhtml, testing::Not(testing::HasSubstr("ext1_sandbox_data")));
  EXPECT_THAT(mhtml, testing::Not(testing::HasSubstr(ext1_sandbox_url.spec())));

  // 2. Grant activeTab permission on the main tab and capture again.
  ActiveTabPermissionGranter::FromWebContents(main_tab)->GrantIfRequested(
      extension2);
  std::string mhtml_with_active_tab = capture_tab_fn(tab_id);
  EXPECT_THAT(mhtml_with_active_tab,
              testing::Not(testing::StartsWith("ERROR:")));
  EXPECT_THAT(mhtml_with_active_tab, testing::HasSubstr("ext1_sample_data"));
  EXPECT_THAT(mhtml_with_active_tab, testing::HasSubstr("ext1_sandbox_data"));
  EXPECT_THAT(mhtml_with_active_tab, testing::HasSubstr("ext2_sample_data"));

  // 3. Capturing a tab where the main frame itself belongs to the other
  // extension should be rejected when activeTab is not granted.
  content::WebContents* new_main_tab = GetActiveWebContents();
  ASSERT_TRUE(new_main_tab);
  ASSERT_TRUE(content::NavigateToURL(new_main_tab, ext1_widget_url));
  const int new_tab_id = ExtensionTabUtil::GetTabId(new_main_tab);
  std::string ext_main_frame_capture = capture_tab_fn(new_tab_id);
  EXPECT_THAT(ext_main_frame_capture,
              testing::HasSubstr(
                  "Don't have permissions required to capture this page."));
}

}  // namespace extensions
