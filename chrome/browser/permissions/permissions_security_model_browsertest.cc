// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/features.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"

namespace {

// Helper for CreateBlobURL() and CreateFilesystemURL().
// ASSERT_* macros can only be used in functions returning void.
void AssertResultIsString(const content::EvalJsResult& result) {
  // Verify no error.
  ASSERT_EQ("", result.error);
  // We could use result.value.is_string(), but this logs the actual type in
  // case of mismatch.
  ASSERT_EQ(base::Value::Type::STRING, result.value.type()) << result.value;
}

// Creates a blob containing dummy HTML, then returns its URL.
// Executes javascript to do so in `rfh`.
GURL CreateBlobURL(content::RenderFrameHost* rfh) {
  content::EvalJsResult result = content::EvalJs(rfh, R"(
    const blob = new Blob(["foo"], {type: "text/html"});
    URL.createObjectURL(blob)
  )");

  AssertResultIsString(result);
  GURL blob_url = GURL(result.ExtractString());
  EXPECT_TRUE(blob_url.SchemeIsBlob());

  return blob_url;
}

// Writes some dummy HTML to a file, then returns its `filesystem:` URL.
// Executes javascript to do so in `rfh`, which must not be nullptr.
GURL CreateFilesystemURL(content::RenderFrameHost* rfh) {
  content::EvalJsResult result = content::EvalJs(rfh, R"(
    // It seems anonymous async functions are not available yet, so we cannot
    // use an immediately-invoked function expression.
    async function run() {
      const fs = await new Promise((resolve, reject) => {
        window.webkitRequestFileSystem(window.TEMPORARY, 1024, resolve,
        reject);
      });
      const file = await new Promise((resolve, reject) => {
        fs.root.getFile('hello.html', {create: true}, resolve, reject);
      });
      const writer = await new Promise((resolve, reject) => {
        file.createWriter(resolve, reject);
      });
      await new Promise((resolve) => {
        writer.onwriteend = resolve;
        writer.write(new Blob(["foo"], {type: "text/html"}));
      });
      return file.toURL();
    }
    run()
  )");

  AssertResultIsString(result);
  GURL fs_url = GURL(result.ExtractString());
  EXPECT_TRUE(fs_url.SchemeIsFileSystem());

  return fs_url;
}

GURL CreateFileURL(const base::FilePath::CharType file_name[] =
                       FILE_PATH_LITERAL("title1.html")) {
  GURL file_url =
      ui_test_utils::GetTestUrl(base::FilePath(), base::FilePath(file_name));
  EXPECT_EQ(url::kFileScheme, file_url.scheme());

  return file_url;
}

// Adds a child iframe sourced from `url` to the given `parent_rfh` document.
// `parent_rfh` must not be nullptr.
content::RenderFrameHost* EmbedIframeFromURL(
    content::RenderFrameHost* parent_rfh,
    const GURL& url) {
  std::string script_template = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.name = "my_iframe";
      iframe.src = $1;
      iframe.onload = _ => { resolve(true); };
      document.body.appendChild(iframe);
    })
  )";

  content::EvalJsResult result =
      content::EvalJs(parent_rfh, content::JsReplace(script_template, url));
  EXPECT_EQ(true, result);  // For the error message.

  content::RenderFrameHost* iframe_rfh = content::FrameMatchingPredicate(
      parent_rfh->GetPage(),
      base::BindRepeating(&content::FrameMatchesName, "my_iframe"));
  return iframe_rfh;
}

constexpr char kCheckNotifications[] = R"((async () => {
       const PermissionStatus = await navigator.permissions.query({name:
       'notifications'}); return PermissionStatus.state === 'granted';
      })();)";

constexpr char kRequestNotifications[] = "Notification.requestPermission()";

constexpr char kCheckGeolocation[] = R"((async () => {
       const PermissionStatus = await navigator.permissions.query({name:
       'geolocation'}); return PermissionStatus.state === 'granted';
      })();)";

constexpr char kRequestGeolocation[] = R"(
          navigator.geolocation.getCurrentPosition(
            _ => domAutomationController.send('granted'),
            _ => domAutomationController.send('denied'));
        )";

constexpr char kCheckCamera[] = R"((async () => {
     const PermissionStatus =
        await navigator.permissions.query({name: 'camera'});
     return PermissionStatus.state === 'granted';
    })();)";

constexpr char kRequestCamera[] = R"(
    var constraints = { video: true };
    navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
        domAutomationController.send('granted');
    })
    .catch(function(err) {
        domAutomationController.send('denied');
    });
    )";

// Tests of permissions behavior for an inheritance and embedding of an origin.
// Test fixtures are run with and without the `PermissionsRevisedOriginHandling`
// flag.
class PermissionsSecurityModelBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionsSecurityModelBrowserTest() {
    feature_list_.InitWithFeatureState(
        permissions::features::kRevisedOriginHandling, GetParam());
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
  }
  PermissionsSecurityModelBrowserTest(
      const PermissionsSecurityModelBrowserTest&) = delete;
  PermissionsSecurityModelBrowserTest& operator=(
      const PermissionsSecurityModelBrowserTest&) = delete;
  ~PermissionsSecurityModelBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        embedder_support::kDisablePopupBlocking);
  }

  content::WebContents* OpenPopup(Browser* browser, const GURL& url) const {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::ExecuteScriptAsync(
        contents, content::JsReplace("window.open($1, '', '[]');", url));
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_NE(popup, browser);
    content::WebContents* popup_contents =
        popup->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetMainFrame()));
    WaitForLoadStop(popup_contents);
    return popup_contents;
  }

  bool IsRevisedOriginHandlingEnabled() { return GetParam(); }

  void VeriftyPermissions(content::WebContents* opener_or_embedder_contents,
                          content::RenderFrameHost* test_rfh) {
    const struct {
      std::string check_permission;
      std::string request_permission;
    } kTests[] = {
        {kCheckNotifications, kRequestNotifications},
        {kCheckCamera, kRequestCamera},
        {kCheckGeolocation, kRequestGeolocation},
    };

    for (const auto& test : kTests) {
      bool is_notification = test.request_permission == kRequestNotifications;
      ASSERT_FALSE(
          content::EvalJs(opener_or_embedder_contents, test.check_permission)
              .value.GetBool());
      ASSERT_FALSE(
          content::EvalJs(test_rfh, test.check_permission).value.GetBool());

      const bool is_embedder =
          test_rfh->IsDescendantOf(opener_or_embedder_contents->GetMainFrame());

      permissions::PermissionRequestManager* manager =
          permissions::PermissionRequestManager::FromWebContents(
              opener_or_embedder_contents);
      std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
          std::make_unique<permissions::MockPermissionPromptFactory>(manager);

      // Enable auto-accept of a permission request.
      bubble_factory->set_response_type(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

      // Request permission on the opener or embedder contents.
      EXPECT_EQ("granted",
                content::EvalJs(
                    opener_or_embedder_contents, test.request_permission,
                    is_notification ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                    : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractString());
      EXPECT_EQ(1, bubble_factory->TotalRequestCount());

      // Disable auto-accept of a permission request.
      bubble_factory->set_response_type(
          permissions::PermissionRequestManager::AutoResponseType::NONE);

      EXPECT_TRUE(
          content::EvalJs(opener_or_embedder_contents, test.check_permission)
              .value.GetBool());

      // Verify permissions on the test RFH.
      {
        // If `test_rfh` is not a descendant of `opener_or_embedder_contents`,
        // in other words if `test_rfh` was created via `Window.open()`,
        // permissions are propagated from an opener WebContents only if
        // `RevisedOriginHandlingEnabled` is enabled.
        const bool expect_granted =
            IsRevisedOriginHandlingEnabled() || is_embedder;

        EXPECT_EQ(
            expect_granted,
            content::EvalJs(test_rfh, test.check_permission).value.GetBool());
      }

      const bool expect_granted =
          IsRevisedOriginHandlingEnabled() || is_embedder;

      // Request permission on the test RFH.
      EXPECT_EQ(expect_granted ? "granted" : "denied",
                content::EvalJs(test_rfh, test.request_permission,
                                is_notification
                                    ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                    : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractString());

      // There should not be the 2nd prompt.
      EXPECT_EQ(1, bubble_factory->TotalRequestCount());
    }
  }

  void VerifyPermissionsForFile(content::RenderFrameHost* rfh,
                                bool expect_granted) {
    const struct {
      std::string check_permission;
      std::string request_permission;
    } kTests[] = {
        {kCheckNotifications, kRequestNotifications},
        {kCheckCamera, kRequestCamera},
        {kCheckGeolocation, kRequestGeolocation},
    };

    for (const auto& test : kTests) {
      ASSERT_FALSE(content::EvalJs(rfh, test.check_permission).value.GetBool());
      EXPECT_EQ(expect_granted ? "granted" : "denied",
                content::EvalJs(rfh, test.request_permission,
                                test.request_permission == kRequestNotifications
                                    ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                    : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractString());

      ASSERT_FALSE(content::EvalJs(rfh, test.check_permission).value.GetBool());
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionsSecurityModelBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       EmbedIframeAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/iframe_about_blank.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* about_blank_iframe =
      content::FrameMatchingPredicate(
          main_rfh->GetPage(), base::BindRepeating(&content::FrameMatchesName,
                                                   "about_blank_iframe"));
  ASSERT_TRUE(about_blank_iframe);

  VeriftyPermissions(embedder_contents, about_blank_iframe);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       WindowOpenAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  content::WebContents* popup_contents =
      OpenPopup(browser(), GURL("about:blank"));
  ASSERT_TRUE(popup_contents);

  VeriftyPermissions(opener_contents, popup_contents->GetMainFrame());
}

// `about:srcdoc` supports only embedder WebContents, hence no test for opener.
IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, EmbedIframeSrcDoc) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/iframe_srcdoc.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* srcdoc_iframe = content::FrameMatchingPredicate(
      main_rfh->GetPage(),
      base::BindRepeating(&content::FrameMatchesName, "srcdoc_iframe"));
  ASSERT_TRUE(srcdoc_iframe);

  VeriftyPermissions(embedder_contents, srcdoc_iframe);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, EmbedIframeBlob) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/iframe_blob.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* blob_iframe_rfh = content::FrameMatchingPredicate(
      main_rfh->GetPage(),
      base::BindRepeating(&content::FrameMatchesName, "blob_iframe"));
  ASSERT_TRUE(blob_iframe_rfh);
  EXPECT_TRUE(blob_iframe_rfh->GetLastCommittedURL().SchemeIsBlob());

  VeriftyPermissions(embedder_contents, blob_iframe_rfh);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, WindowOpenBlob) {
  if (GetParam()) {
    // Blob iframe on an opener contents does not work if
    // `kRevisedOriginHandling` feature enabled.
    // TODO(crbug.com/698985): Remove when the bug is fixed.
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  content::WebContents* blob_popup_contents =
      OpenPopup(browser(), CreateBlobURL(main_rfh));
  ASSERT_TRUE(blob_popup_contents);
  EXPECT_TRUE(blob_popup_contents->GetLastCommittedURL().SchemeIsBlob());

  VeriftyPermissions(opener_contents, blob_popup_contents->GetMainFrame());
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       EmbedIframeFileSystem) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  content::RenderFrameHost* embedded_iframe_rfh =
      EmbedIframeFromURL(main_rfh, CreateFilesystemURL(main_rfh));
  ASSERT_TRUE(embedded_iframe_rfh);

  VeriftyPermissions(embedder_contents, embedded_iframe_rfh);
}

// Renderer navigation for "filesystem:" is not allowed.
IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       WindowOpenFileSystemRendererNavigationNotAllowed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  content::WebContents* popup_iframe =
      OpenPopup(browser(), CreateFilesystemURL(main_rfh));
  ASSERT_TRUE(popup_iframe);

  // Not allowed to navigate top frame to filesystem URL.
  EXPECT_EQ("", popup_iframe->GetLastCommittedURL().scheme());
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       WindowOpenFileSystemBrowserNavigation) {
  if (!GetParam()) {
    // Filesystem iframe on an opener contents does not work if
    // `kRevisedOriginHandling` feature disabled.
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  GURL fs_url = CreateFilesystemURL(main_rfh);

  content::WebContents* popup_iframe_web_contents =
      OpenPopup(browser(), fs_url);
  ASSERT_TRUE(popup_iframe_web_contents);

  EXPECT_EQ("", popup_iframe_web_contents->GetLastCommittedURL().scheme());

  content::RenderFrameHost* popup_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          chrome::FindBrowserWithWebContents(popup_iframe_web_contents), fs_url,
          1);

  EXPECT_TRUE(popup_rfh->GetLastCommittedURL().SchemeIsFileSystem());

  VeriftyPermissions(opener_contents, popup_rfh);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest, TopIframeFile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);
  EXPECT_FALSE(embedder_contents->GetLastCommittedURL().SchemeIsFile());

  main_rfh = ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), CreateFileURL(), 1);
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().SchemeIsFile());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ true);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ false);
}

// Permissions granted for a file should not leak to another file.
IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       PermissionDoesNotLeakToAnotherFile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);
  EXPECT_FALSE(embedder_contents->GetLastCommittedURL().SchemeIsFile());

  GURL file1_url = CreateFileURL();

  main_rfh = ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), file1_url, 1);
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().SchemeIsFile());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ true);

  GURL file2_url = CreateFileURL(FILE_PATH_LITERAL("title2.html"));

  main_rfh = ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), file2_url, 1);
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().SchemeIsFile());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());
  EXPECT_EQ(file2_url.spec(), main_rfh->GetLastCommittedURL().spec());
  EXPECT_NE(file1_url.spec(), file2_url.spec());

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Permission is failed because it is another file.
  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ false);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       UniversalAccessFromFileUrls) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  // Activate the preference to allow universal access from file URLs.
  blink::web_pref::WebPreferences prefs =
      embedder_contents->GetOrCreateWebPreferences();
  prefs.allow_universal_access_from_file_urls = true;
  embedder_contents->SetWebPreferences(prefs);

  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);
  EXPECT_FALSE(main_rfh->GetLastCommittedURL().SchemeIsFile());

  main_rfh = ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), CreateFileURL(), 1);
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().SchemeIsFile());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ true);

  content::EvalJsResult result = content::EvalJs(
      embedder_contents, "history.pushState({}, {}, 'https://chromium.org');");
  EXPECT_EQ(std::string(), result.error);
  EXPECT_EQ("https://chromium.org/", main_rfh->GetLastCommittedURL().spec());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());

  const struct {
    std::string check_permission;
    std::string request_permission;
  } kTests[] = {
      {kCheckCamera, kRequestCamera},
      {kCheckGeolocation, kRequestGeolocation},
  };

  for (const auto& test : kTests) {
    ASSERT_FALSE(
        content::EvalJs(main_rfh, test.check_permission).value.GetBool());
    EXPECT_EQ("granted",
              content::EvalJs(main_rfh, test.request_permission,
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());
    ASSERT_TRUE(
        content::EvalJs(main_rfh, test.check_permission).value.GetBool());
  }

  // Notifications is not supported for file:/// with changed URL.
  ASSERT_FALSE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());
  EXPECT_EQ("denied", content::EvalJs(main_rfh, kRequestNotifications,
                                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS)
                          .ExtractString());
}

// Verifies that permissions are not supported for file:/// with changed URL to
// `about:blank`.
IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       UniversalAccessFromFileUrlsAboutBlank) {
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  // Activate the preference to allow universal access from file URLs.
  blink::web_pref::WebPreferences prefs =
      embedder_contents->GetOrCreateWebPreferences();
  prefs.allow_universal_access_from_file_urls = true;
  embedder_contents->SetWebPreferences(prefs);

  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          browser(), CreateFileURL(), 1);
  ASSERT_TRUE(main_rfh);
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().SchemeIsFile());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ true);

  content::EvalJsResult result = content::EvalJs(
      embedder_contents, "history.pushState({}, {}, 'about:blank');");
  EXPECT_EQ(std::string(), result.error);
  EXPECT_EQ("about:blank", main_rfh->GetLastCommittedURL().spec());
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().IsAboutBlank());

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ false);
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       PermissionRequestOnNtpUseDseOrigin) {
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          browser(), GURL(chrome::kChromeUINewTabURL), 1);

  ASSERT_TRUE(main_rfh);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            embedder_contents->GetLastCommittedURL().GetOrigin());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
            main_rfh->GetLastCommittedOrigin().GetURL());

  constexpr char kCheckMic[] = R"((async () => {
         const PermissionStatus =
            await navigator.permissions.query({name: 'microphone'});
         return PermissionStatus.state === 'granted';
    })();)";

  constexpr char kRequestMic[] = R"(
    var constraints = { audio: true };
    navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
        domAutomationController.send('granted');
    })
    .catch(function(err) {
        domAutomationController.send('denied');
    });
    )";

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMic,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_EQ("granted",
            content::EvalJs(main_rfh, kRequestMic,
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckMic,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());
}

IN_PROC_BROWSER_TEST_P(PermissionsSecurityModelBrowserTest,
                       MicActivityIndicatorOnNtpUseDseOrigin) {
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          browser(), GURL(chrome::kChromeUINewTabURL), 1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            embedder_contents->GetLastCommittedURL().GetOrigin());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
            main_rfh->GetLastCommittedOrigin().GetURL());

  constexpr char kCheckMic[] = R"((async () => {
         const PermissionStatus =
            await navigator.permissions.query({name: 'microphone'});
         return PermissionStatus.state === 'granted';
    })();)";

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMic,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);

  constexpr char kRequestMic[] =
      "navigator.mediaDevices.getUserMedia({ audio: true });";

  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(
          permission_request_manager);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  content::EvalJsResult result = content::EvalJs(
      main_rfh, kRequestMic, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1);
  EXPECT_EQ("", result.error);

  // Mic request from an NTP will be changed to DSE origin.
  EXPECT_TRUE(
      bubble_factory->RequestOriginSeen(GURL("https://www.google.com")));
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckMic,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  content_settings::PageSpecificContentSettings* page_content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(main_rfh);

  // Media stream origin on NTP should equal to DSE.
  EXPECT_EQ(page_content_settings->media_stream_access_origin(),
            GURL("https://www.google.com"));

}  // namespace
}  // anonymous namespace
