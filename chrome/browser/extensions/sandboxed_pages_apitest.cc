// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

enum class ManifestVersion { TWO, THREE };

class SandboxedPagesTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<ManifestVersion> {
 public:
  SandboxedPagesTest() = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  [[nodiscard]] bool RunTest(const char* extension_name,
                             const char* manifest,
                             const RunOptions& run_options,
                             const LoadOptions& load_options) {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;

    //  Load the extension with the given `manifest`.
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "Could not create temporary dir for test";
      return false;
    }

    base::FilePath source_extension_path =
        test_data_dir_.AppendASCII(extension_name);
    base::FilePath destination_extension_path =
        temp_dir_.GetPath().AppendASCII(extension_name);
    if (!base::CopyDirectory(source_extension_path, destination_extension_path,
                             true /* recursive */)) {
      ADD_FAILURE() << source_extension_path.value()
                    << " could not be copied to "
                    << destination_extension_path.value();
      return false;
    }

    test_data_dir_ = temp_dir_.GetPath();
    base::FilePath manifest_path =
        destination_extension_path.Append(kManifestFilename);
    if (!base::WriteFile(manifest_path, manifest)) {
      ADD_FAILURE() << "Could not write manifest file to "
                    << manifest_path.value();
      return false;
    }

    return RunExtensionTest(extension_name, run_options, load_options);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

// A test class to verify operation of metrics to record use of extension API
// functions in extensions pages that are sandboxed, but not listed as sandboxed
// in the extension's manifest. This class is parameterized on
// kIsolateSandboxedIframes so that it tests both in-process and
// process-isolated sandboxed frames.
class SandboxAPIMetricsTest : public ExtensionApiTest,
                              public ::testing::WithParamInterface<bool> {
 public:
  SandboxAPIMetricsTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          blink::features::kIsolateSandboxedIframes);
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kIsolateSandboxedIframes);
    }
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SandboxedPagesTest,
#if BUILDFLAG(IS_ANDROID)
                         // Android only supports manifest V3.
                         ::testing::Values(ManifestVersion::THREE));
#else
                         ::testing::Values(ManifestVersion::TWO,
                                           ManifestVersion::THREE));
#endif

IN_PROC_BROWSER_TEST_P(SandboxedPagesTest, SandboxedPages) {
  const char* kManifestV2 = R"(
    {
      "name": "Extension with sandboxed pages",
      "manifest_version": 2,
      "version": "0.1",
      "sandbox": {
        "pages": ["sandboxed.html"]
      }
    }
  )";
  const char* kManifestV3 = R"(
    {
      "name": "Extension with sandboxed pages",
      "manifest_version": 3,
      "version": "0.1",
      "sandbox": {
        "pages": ["sandboxed.html"]
      }
    }
  )";
  const char* kManifest =
      GetParam() == ManifestVersion::TWO ? kManifestV2 : kManifestV3;
  EXPECT_TRUE(
      RunTest("sandboxed_pages", kManifest, {.extension_url = "main.html"}, {}))
      << message_;
}

#if !BUILDFLAG(IS_ANDROID)
// Verifies the behavior of sandboxed pages in Manifest V2. Remote frames
// should be disallowed. Android only supports Manifest V3, so this test is
// skipped on Android.
IN_PROC_BROWSER_TEST_F(SandboxedPagesTest, ManifestV2DisallowsWebContent) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const char* kManifest = R"(
    {
      "name": "Tests that loading web content fails inside sandboxed pages",
      "manifest_version": 2,
      "version": "0.1",
      "web_accessible_resources": ["local_frame.html", "remote_frame.html"],
      "sandbox": {
        "pages": ["sandboxed.html"],
        "content_security_policy": "sandbox allow-scripts; child-src *;"
      }
    }
  )";

  // This extension attempts to load remote web content inside a sandboxed page.
  // Loading web content will fail because of CSP. In addition to that we will
  // show manifest warnings, hence ignore_manifest_warnings is set to true.
  ASSERT_TRUE(RunTest("sandboxed_pages_csp", kManifest,
                      {.extension_url = "main.html"},
                      {.ignore_manifest_warnings = true}))
      << message_;
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verifies the behavior of sandboxed pages in Manifest V3. Remote frames
// should be allowed.
IN_PROC_BROWSER_TEST_F(SandboxedPagesTest, ManifestV3AllowsWebContent) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "test extension",
           "version": "0.1",
           "manifest_version": 3,
           "content_security_policy": {
             "sandbox": "sandbox allow-scripts; child-src *;"
           },
           "sandbox": { "pages": ["sandboxed.html"] }
         })";
  static constexpr char kSandboxedHtml[] =
      R"(<html>
           <body>Sandboxed Page</body>
           <script>
             var iframe = document.createElement('iframe');
             iframe.src = 'http://example.com:%d/extensions/echo_message.html';
             // Check that we can post-message the frame.
             addEventListener('message', (e) => {
               // Note: We use domAutomationController here (and
               // DOMMessageQueue below) because since this is a sandboxed page,
               // it doesn't have access to any chrome.* APIs, including
               // chrome.test.
               domAutomationController.send(e.data);
             });
             iframe.onload = () => {
               iframe.contentWindow.postMessage('hello', '*');
             };
             document.body.appendChild(iframe);
           </script>
         </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("sandboxed.html"),
      base::StringPrintf(kSandboxedHtml, embedded_test_server()->port()));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::DOMMessageQueue message_queue;
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, extension->GetResourceURL("sandboxed.html")));
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame_host);

  // The frame should be sandboxed, so the origin should be "null" (as opposed
  // to `extension->origin()`).
  EXPECT_EQ("null", frame_host->GetLastCommittedOrigin().Serialize());

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ(R"("echo hello")", message);
}

// This test has an API function access inside a frame sandboxed via HTML
// attributes (rather than the manifest specification); it should trigger a
// histogram count.
IN_PROC_BROWSER_TEST_P(SandboxAPIMetricsTest,
                       SandboxedApiAccessTriggersHistogramCounts) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "test extension",
           "version": "0.1",
           "manifest_version": 3
         })";
  static constexpr char kPageWithSandboxedFrame[] =
      R"(<html>
          <body>
            <h1>Page with Sandboxed Frame</h1>
            <iframe sandbox="allow-scripts" src="sandboxed_page.html"></iframe>
          </body>
        </html>)";
  static constexpr char kSandboxedScriptSrc[] =
      R"((async function hasAccessToExtensionAPIs() {
            try {
              // Use chrome.extension because it is available on Android.
              let allowed = await chrome.extension.isAllowedIncognitoAccess();
              // Intentionally check the type and the false value.
              return allowed === false;
            } catch(err) {
              return false;
            }
          })().then(result => domAutomationController.send(result));
        )";
  static constexpr char kSandboxedPage[] =
      R"(<html>
          <body>
            <h1>Sandboxed Page</h1>
            <script src="sandboxed.js"></script>
          </body>
        </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("main.html"), kPageWithSandboxedFrame);
  test_dir.WriteFile(FILE_PATH_LITERAL("sandboxed.js"), kSandboxedScriptSrc);
  test_dir.WriteFile(FILE_PATH_LITERAL("sandboxed_page.html"), kSandboxedPage);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Prepare histogram.
  base::HistogramTester histograms;
  const char* kHistogramName =
      "Extensions.Functions.DidSandboxedExtensionAPICall";

  // Use message queue to verify that loading of the sandboxed child completed
  // successfully.
  content::DOMMessageQueue message_queue;
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     extension->GetResourceURL("main.html")));
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame_host);

  // Verify the sandboxed frame loaded and has api access.
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("true", message);

  // Verify histogram count captured exactly one API call from the sandboxed
  // frame.
  histograms.ExpectBucketCount(kHistogramName, true, 1u);
}

// This test is nearly identical to ApiAccessTriggersHistogramCounts, except
// that the API access is in the (non-sandboxed) main frame, and shouldn't
// trigger a count.
IN_PROC_BROWSER_TEST_P(SandboxAPIMetricsTest,
                       NonSandboxedApiAccessDoesntTriggerHistogramCounts) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "test extension",
           "version": "0.1",
           "manifest_version": 3
         })";
  static constexpr char kMainScriptSrc[] =
      R"(window.onload = async () => {
           let hasApiAccess = true;
           try {
             // Use chrome.extension because it is available on Android.
             let allowed = await chrome.extension.isAllowedIncognitoAccess();
             // Intentionally check the type and the false value.
             hasApiAccess = allowed === false;
           } catch(err) {
             hasApiAccess = false;
           }
           domAutomationController.send(hasApiAccess);
         };)";
  static constexpr char kPageWithSandboxedFrame[] =
      R"(<html>
          <head>
            <script src="main.js"></script>
          </head>
          <body>
            <h1>Page with Sandboxed Frame</h1>
            <iframe sandbox="allow-scripts" src="sandboxed_page.html"></iframe>
          </body>
        </html>)";
  static constexpr char kSandboxedPage[] =
      R"(<html>
          <body>
            <h1>Sandboxed Page</h1>
          </body>
        </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("main.js"), kMainScriptSrc);
  test_dir.WriteFile(FILE_PATH_LITERAL("main.html"), kPageWithSandboxedFrame);
  test_dir.WriteFile(FILE_PATH_LITERAL("sandboxed_page.html"), kSandboxedPage);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Prepare histogram.
  base::HistogramTester histograms;
  const char* kHistogramName =
      "Extensions.Functions.DidSandboxedExtensionAPICall";

  // Use message queue to verify that loading of the sandboxed child completed
  // successfully.
  content::DOMMessageQueue message_queue;
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     extension->GetResourceURL("main.html")));
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(frame_host);

  // Verify the sandboxed frame loaded.
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("true", message);

  // Verify histogram count captured no API calls from the non-sandboxed frame.
  histograms.ExpectBucketCount(kHistogramName, true, 0u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SandboxAPIMetricsTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "kIsolateSandboxedIframesEnabled"
                                      : "kIsolateSandboxedIframesDisabled";
                         });

// Verify sandbox behavior.
IN_PROC_BROWSER_TEST_P(SandboxedPagesTest, WebAccessibleResourcesTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install extension.
  TestExtensionDir extension_dir;
  static constexpr char kManifestV2[] = R"({
    "name": "Extension sandbox text",
    "version": "1.0",
    "manifest_version": 2,
    "sandbox": {
      "pages": ["sandboxed_page.html"]
    },
    "web_accessible_resources": [
      "web_accessible_resource.html"
    ]
  })";

  static constexpr char kManifestV3[] =
      R"({
           "name": "Extension sandbox text",
           "version": "1.0",
           "manifest_version": 3,
           "sandbox": {
             "pages": ["sandboxed_page.html"]
           },
           "web_accessible_resources": [{
             "resources": ["web_accessible_resource.html"],
             "matches": ["<all_urls>"]
           }]
         })";

  const char* manifest =
      GetParam() == ManifestVersion::TWO ? kManifestV2 : kManifestV3;

  extension_dir.WriteManifest(manifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("sandboxed_page.html"), "");
  extension_dir.WriteFile(FILE_PATH_LITERAL("page.html"), "");
  extension_dir.WriteFile(FILE_PATH_LITERAL("resource.html"), "resource.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("web_accessible_resource.html"),
                          "web_accessible_resource.html");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Fetch url from frame to verify histograms match expectations.
  auto test_frame_with_fetch = [&](const char* frame_url, const char* fetch_url,
                                   bool is_web_accessible_resource, int count,
                                   std::string expected_frame_origin) {
    // Fetch and test resource.
    content::WebContents* web_contents = GetActiveWebContents();
    ASSERT_TRUE(content::NavigateToURL(web_contents,
                                       extension->GetResourceURL(frame_url)));
    constexpr char kFetchScriptTemplate[] =
        R"(
        fetch($1).then(result => {
          return result.text();
        }).catch(err => {
          return String(err);
        });)";
    EXPECT_EQ(content::EvalJs(
                  web_contents,
                  content::JsReplace(kFetchScriptTemplate,
                                     extension->GetResourceURL(fetch_url))),
              fetch_url);
    EXPECT_EQ(expected_frame_origin, web_contents->GetPrimaryMainFrame()
                                         ->GetLastCommittedOrigin()
                                         .Serialize());
  };

  // Extension page fetching an extension file.
  test_frame_with_fetch("page.html", "resource.html", false, 0,
                        extension->origin().Serialize());

  // Extension page fetching a web accessible resource.
  test_frame_with_fetch("page.html", "web_accessible_resource.html", true, 0,
                        extension->origin().Serialize());

  // Sandboxed extension page fetching an extension file.
  test_frame_with_fetch("sandboxed_page.html", "resource.html", false, 1,
                        "null");

  // Sandboxed extension page fetching a web accessible resource.
  test_frame_with_fetch("sandboxed_page.html", "web_accessible_resource.html",
                        true, 1, "null");
}

}  // namespace extensions
