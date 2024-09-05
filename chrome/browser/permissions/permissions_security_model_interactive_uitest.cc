// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
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

// Return the active RenderFrameHost loaded in the last iframe in |parent_rfh|.
content::RenderFrameHost* LastChild(content::RenderFrameHost* parent_rfh) {
  int child_end = 0;
  while (ChildFrameAt(parent_rfh, child_end))
    child_end++;
  if (child_end == 0)
    return nullptr;
  return ChildFrameAt(parent_rfh, child_end - 1);
}

// Create an <iframe> inside |parent_rfh|, and navigate it toward |url|.
// |permission_policy| can be used to set permission policy to the iframe.
// For instance:
// ```
// child = CreateIframe(parent, url, "geolocation *; camera *");
// ```
// This returns the new RenderFrameHost associated with new document created in
// the iframe.
content::RenderFrameHost* CreateIframe(
    content::RenderFrameHost* parent_rfh,
    const GURL& url,
    const std::string& permission_policy = "") {
  EXPECT_EQ(
      "iframe loaded",
      content::EvalJs(parent_rfh, content::JsReplace(R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.allow = $2;
      iframe.onload = _ => { resolve("iframe loaded"); };
      document.body.appendChild(iframe);
    }))",
                                                     url, permission_policy)));
  return LastChild(parent_rfh);
}

content::WebContents* OpenPopup(Browser* browser, const GURL& url) {
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::ExecuteScriptAsync(
      contents, content::JsReplace("window.open($1, '', '[]');", url));
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_NE(popup, browser);
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetPrimaryMainFrame()));
  WaitForLoadStop(popup_contents);
  return popup_contents;
}

constexpr char kCheckNotifications[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'notifications'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

constexpr char kRequestNotifications[] = R"(
    new Promise(resolve => {
      Notification.requestPermission().then(function (permission) {
        resolve(permission)
      });
    })
    )";

constexpr char kCheckGeolocation[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'geolocation'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

constexpr char kRequestGeolocation[] = R"(
    new Promise(resolve => {
      navigator.geolocation.getCurrentPosition(
        () => resolve('granted'),
        () => resolve('denied')
      );
    })
    )";

constexpr char kCheckCamera[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'camera'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

constexpr char kRequestCamera[] = R"(
    new Promise(async resolve => {
      var constraints = { video: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

constexpr char kRequestMicrophone[] = R"(
    new Promise(async resolve => {
      var constraints = { audio: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

constexpr char kCheckMicrophone[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'microphone'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

constexpr char kIframePolicy[] = "geolocation *; camera *";

void VerifyPermissionsAllowed(content::RenderFrameHost* main_rfh,
                              const std::string& request_permission_script,
                              const std::string& check_permission_script) {
  ASSERT_FALSE(
      content::EvalJs(main_rfh, check_permission_script).value.GetBool());
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(
          content::WebContents::FromRenderFrameHost(main_rfh));
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Move the web contents to the foreground.
  main_rfh->GetView()->Focus();
  ASSERT_TRUE(main_rfh->GetView()->HasFocus());

  EXPECT_EQ("granted", content::EvalJs(main_rfh, request_permission_script));
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Disable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(
      content::EvalJs(main_rfh, check_permission_script).value.GetBool());
}

// `test_rfh` is either an embedded iframe or an external popup window.
void VerifyPermission(content::WebContents* opener_or_embedder_contents,
                      content::RenderFrameHost* test_rfh,
                      const std::string& request_permission_script,
                      const std::string& check_permission_script) {
  content::RenderFrameHost* opener_rfh =
      opener_or_embedder_contents->GetPrimaryMainFrame();
  ASSERT_FALSE(
      content::EvalJs(opener_rfh, check_permission_script).value.GetBool());
  ASSERT_FALSE(
      content::EvalJs(test_rfh, check_permission_script).value.GetBool());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(
          opener_or_embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Move the web contents to the foreground.
  opener_rfh->GetView()->Focus();
  ASSERT_TRUE(opener_rfh->GetView()->HasFocus());
  // Request permission on the opener or embedder contents.
  EXPECT_EQ("granted", content::EvalJs(opener_rfh, request_permission_script));
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Disable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(
      content::EvalJs(opener_rfh, check_permission_script).value.GetBool());

  // Verify permissions on the test RFH.
  {
    EXPECT_TRUE(
        content::EvalJs(test_rfh, check_permission_script).value.GetBool());
  }

  // Request permission on the test RFH.
  test_rfh->GetView()->Focus();
  ASSERT_TRUE(test_rfh->GetView()->HasFocus());
  EXPECT_EQ("granted", content::EvalJs(test_rfh, request_permission_script));

  // There should not be the 2nd prompt.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());
}

void VerifyPermissionsDeniedForFencedFrame(
    content::WebContents* embedder_contents,
    content::RenderFrameHost* fenced_rfh,
    const std::string& request_permission_script,
    const std::string& check_permission_script) {
  content::RenderFrameHost* embedder_main_rfh =
      embedder_contents->GetPrimaryMainFrame();
  ASSERT_FALSE(content::EvalJs(embedder_main_rfh, check_permission_script)
                   .value.GetBool());
  ASSERT_FALSE(
      content::EvalJs(fenced_rfh, check_permission_script).value.GetBool());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Move the web contents to the foreground.
  embedder_main_rfh->GetView()->Focus();
  ASSERT_TRUE(embedder_main_rfh->GetView()->HasFocus());

  // Request permission on the embedder contents.
  EXPECT_EQ("granted",
            content::EvalJs(embedder_main_rfh, request_permission_script));
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Disable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(content::EvalJs(embedder_main_rfh, check_permission_script)
                  .value.GetBool());

  // MPArch RFH is not allowed to verify permissions.
  EXPECT_FALSE(
      content::EvalJs(fenced_rfh, check_permission_script).value.GetBool());

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Request permission on the test RFH.
  fenced_rfh->GetView()->Focus();
  ASSERT_TRUE(fenced_rfh->GetView()->HasFocus());
  EXPECT_EQ("denied", content::EvalJs(fenced_rfh, request_permission_script));

  // There should not be the 2nd prompt.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());
}

// getUserMedia requires focus. It should be verified only on a popup window.
void VerifyPopupWindowGetUserMedia(content::WebContents* opener_contents,
                                   content::WebContents* popup_contents) {
  content::RenderFrameHost* opener_rfh = opener_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* popup_rfh = popup_contents->GetPrimaryMainFrame();

  ASSERT_FALSE(content::EvalJs(opener_rfh, kCheckCamera).value.GetBool());
  ASSERT_FALSE(content::EvalJs(popup_rfh, kCheckCamera).value.GetBool());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(popup_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Move the web contents to the foreground.
  popup_rfh->GetView()->Focus();
  ASSERT_TRUE(popup_rfh->GetView()->HasFocus());
  // Request permission on the popup RenderFrameHost.
  EXPECT_EQ("granted", content::EvalJs(popup_rfh, kRequestCamera));
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Disable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(content::EvalJs(popup_rfh, kCheckCamera).value.GetBool());
  EXPECT_TRUE(content::EvalJs(opener_rfh, kCheckCamera).value.GetBool());
}

void VerifyPermissionsAllowed(content::RenderFrameHost* rfh) {
  const struct {
    std::string check_permission;
    std::string request_permission;
  } kTests[] = {
      {kCheckNotifications, kRequestNotifications},
      {kCheckGeolocation, kRequestGeolocation},
      {kCheckCamera, kRequestCamera},
  };

  for (const auto& test : kTests) {
    VerifyPermissionsAllowed(rfh, test.request_permission,
                             test.check_permission);
  }
}

void VerifyPermissionsDeniedForFencedFrame(
    content::WebContents* embedder_contents,
    content::RenderFrameHost* fenced_rfh) {
  const struct {
    std::string check_permission;
    std::string request_permission;
  } kTests[] = {
      {kCheckNotifications, kRequestNotifications},
      {kCheckGeolocation, kRequestGeolocation},
      {kCheckCamera, kRequestCamera},
  };

  for (const auto& test : kTests) {
    VerifyPermissionsDeniedForFencedFrame(embedder_contents, fenced_rfh,
                                          test.request_permission,
                                          test.check_permission);
  }
}

void VerifyPermissionsExceptGetUserMedia(
    content::WebContents* opener_or_embedder_contents,
    content::RenderFrameHost* test_rfh) {
  const struct {
    std::string check_permission;
    std::string request_permission;
  } kTests[] = {
      {kCheckNotifications, kRequestNotifications},
      {kCheckGeolocation, kRequestGeolocation},
  };

  for (const auto& test : kTests) {
    VerifyPermission(opener_or_embedder_contents, test_rfh,
                     test.request_permission, test.check_permission);
  }
}

void VerifyPermissionsAllPermissions(
    content::WebContents* opener_or_embedder_contents,
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
    VerifyPermission(opener_or_embedder_contents, test_rfh,
                     test.request_permission, test.check_permission);
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
              content::EvalJs(rfh, test.request_permission));

    ASSERT_FALSE(content::EvalJs(rfh, test.check_permission).value.GetBool());
  }
}

// Tests of permissions behavior for an inheritance and embedding of an
// origin.
class PermissionsSecurityModelInteractiveUITest : public InProcessBrowserTest {
 public:
  PermissionsSecurityModelInteractiveUITest() {
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
  }
  PermissionsSecurityModelInteractiveUITest(
      const PermissionsSecurityModelInteractiveUITest&) = delete;
  PermissionsSecurityModelInteractiveUITest& operator=(
      const PermissionsSecurityModelInteractiveUITest&) = delete;
  ~PermissionsSecurityModelInteractiveUITest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        embedder_support::kDisablePopupBlocking);
  }

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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

  VerifyPermissionsAllPermissions(embedder_contents, about_blank_iframe);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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

  VerifyPermissionsExceptGetUserMedia(opener_contents,
                                      popup_contents->GetPrimaryMainFrame());
  VerifyPopupWindowGetUserMedia(opener_contents, popup_contents);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       WindowOpenAboutBlankToUseQuiet) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kEnableQuietNotificationPermissionUi, true);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(opener_contents);

  content::WebContents* popup_contents =
      OpenPopup(browser(), GURL("about:blank"));
  ASSERT_TRUE(popup_contents);

  VerifyPermissionsExceptGetUserMedia(opener_contents,
                                      popup_contents->GetPrimaryMainFrame());
  VerifyPopupWindowGetUserMedia(opener_contents, popup_contents);
}

// `about:srcdoc` supports only embedder WebContents, hence no test for opener.
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       EmbedIframeSrcDoc) {
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

  VerifyPermissionsAllPermissions(embedder_contents, srcdoc_iframe);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       EmbedIframeBlob) {
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

  VerifyPermissionsAllPermissions(embedder_contents, blob_iframe_rfh);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       WindowOpenBlob) {
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

  VerifyPermissionsExceptGetUserMedia(
      opener_contents, blob_popup_contents->GetPrimaryMainFrame());
  VerifyPopupWindowGetUserMedia(opener_contents, blob_popup_contents);
}

// Renderer navigation for "filesystem:" is not allowed.
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       WindowOpenFileSystemBrowserNavigation) {
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
          chrome::FindBrowserWithTab(popup_iframe_web_contents), fs_url, 1);

  EXPECT_TRUE(popup_rfh->GetLastCommittedURL().SchemeIsFileSystem());

  VerifyPermissionsExceptGetUserMedia(opener_contents, popup_rfh);
  VerifyPopupWindowGetUserMedia(
      opener_contents, content::WebContents::FromRenderFrameHost(popup_rfh));
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       TopIframeFile) {
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

  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/true);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/false);
}

// Permissions granted for a file should not leak to another file.
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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

  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/true);

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
  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/false);
}

// Flaky - https://crbug.com/1289985
#if BUILDFLAG(IS_WIN)
#define MAYBE_UniversalAccessFromFileUrls UniversalAccessFromFileUrls
#else
#define MAYBE_UniversalAccessFromFileUrls DISABLED_UniversalAccessFromFileUrls
#endif
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       MAYBE_UniversalAccessFromFileUrls) {
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

  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/true);

  content::EvalJsResult result = content::EvalJs(
      embedder_contents, "history.pushState({}, {}, 'https://chromium.org');");
  EXPECT_EQ(std::string(), result.error);
  EXPECT_EQ("https://chromium.org/", main_rfh->GetLastCommittedURL().spec());
  EXPECT_TRUE(main_rfh->GetLastCommittedOrigin().GetURL().SchemeIsFile());

  // `https://chromium.org` is used for permissions verification.
#if BUILDFLAG(IS_ANDROID)
  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/false);
#else
  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/true);
#endif
}

// Verifies that permissions are not supported for file:/// with changed URL to
// `about:blank`.
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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

  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/true);

  content::EvalJsResult result = content::EvalJs(
      embedder_contents, "history.pushState({}, {}, 'about:blank');");
  EXPECT_EQ(std::string(), result.error);
  EXPECT_EQ("about:blank", main_rfh->GetLastCommittedURL().spec());
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().IsAboutBlank());

#if BUILDFLAG(IS_ANDROID)
  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/false);
#else
  VerifyPermissionsForFile(main_rfh, /*expect_granted=*/true);
#endif
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       PermissionRequestOnNtpUseDseOrigin) {
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          browser(), GURL(chrome::kChromeUINewTabURL), 1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            embedder_contents->GetLastCommittedURL());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
            main_rfh->GetLastCommittedOrigin().GetURL());

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_EQ("granted",
            content::EvalJs(main_rfh, kRequestMicrophone,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckMicrophone,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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
            embedder_contents->GetLastCommittedURL());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
            main_rfh->GetLastCommittedOrigin().GetURL());

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);

  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(
          permission_request_manager);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_EQ("granted",
            content::EvalJs(main_rfh, kRequestMicrophone,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));

  // Mic request from an NTP will be changed to DSE origin.
  EXPECT_TRUE(
      bubble_factory->RequestOriginSeen(GURL("https://www.google.com")));
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckMicrophone,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  content_settings::PageSpecificContentSettings* page_content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(main_rfh);

  // Media stream origin on NTP should equal to DSE.
  EXPECT_EQ(page_content_settings->media_stream_access_origin(),
            GURL("https://www.google.com"));
}

// Test that a permission prompt bubble will be shown on NTP despite the empty
// address bar.
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
                       PermissionRequestOnNtpIsNotAutoIgnored) {
  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          browser(), GURL(chrome::kChromeUINewTabURL), 1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            embedder_contents->GetLastCommittedURL());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
            main_rfh->GetLastCommittedOrigin().GetURL());

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  permissions::PermissionRequestObserver observer(embedder_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  EXPECT_TRUE(content::ExecJs(
      main_rfh, kRequestMicrophone,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until a permission request is shown.
  observer.Wait();

  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_TRUE(observer.request_shown());

  manager->Accept();

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckMicrophone,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());
}

class PermissionsSecurityModelHTTPS
    : public PermissionsSecurityModelInteractiveUITest {
 public:
  PermissionsSecurityModelHTTPS()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PermissionsSecurityModelHTTPS() override = default;

  PermissionsSecurityModelHTTPS(const PermissionsSecurityModelHTTPS&) = delete;
  PermissionsSecurityModelHTTPS& operator=(
      const PermissionsSecurityModelHTTPS&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionsSecurityModelInteractiveUITest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PermissionsSecurityModelInteractiveUITest::
        SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    PermissionsSecurityModelInteractiveUITest::
        TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));

    PermissionsSecurityModelInteractiveUITest::SetUpOnMainThread();

    ASSERT_TRUE(GetHttpsServer()->Start());
  }

 protected:
  GURL GetURL(const std::string& hostname, const std::string& relative_url) {
    return https_test_server_.GetURL(hostname, relative_url);
  }

  net::EmbeddedTestServer* GetHttpsServer() { return &https_test_server_; }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Navigate the main frame toward |url|, returns the new RenderFrameHost.
  content::RenderFrameHost* NavigateAndCheckPermissionState(GURL url) {
    url::Origin origin = url::Origin::Create(url);

    content::WebContents* embedder_contents = GetWebContents();

    EXPECT_TRUE(content::NavigateToURL(embedder_contents, url));
    content::RenderFrameHost* main_rfh =
        embedder_contents->GetPrimaryMainFrame();
    EXPECT_EQ(origin, main_rfh->GetLastCommittedOrigin());

    // By default permissions are not allowed.
    CheckPermissionState(main_rfh, /*notifications_allowed=*/false,
                         /*geolocation_allowed=*/false,
                         /*camera_allowed=*/false);

    return main_rfh;
  }

  void CheckPermissionState(content::RenderFrameHost* rfh,
                            bool notifications_allowed,
                            bool geolocation_allowed,
                            bool camera_allowed) {
    EXPECT_EQ(geolocation_allowed, content::EvalJs(rfh, kCheckGeolocation));
    EXPECT_EQ(notifications_allowed, content::EvalJs(rfh, kCheckNotifications));
    EXPECT_EQ(camera_allowed, content::EvalJs(rfh, kCheckCamera));
  }

  GURL GetMainFrameURL() { return GetURL("a.test", "/title1.html"); }
  GURL GetChildFrameURL() { return GetURL("b.test", "/title1.html"); }

  void RequestPermissions(content::RenderFrameHost* rfh, bool expected) {
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebContents());
    std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Enable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    std::string expected_result = expected ? "granted" : "denied";
    EXPECT_EQ(expected_result, content::EvalJs(rfh, kRequestGeolocation));
    EXPECT_EQ(expected_result, content::EvalJs(rfh, kRequestCamera));

    // Notifications permission cannot be granted for an embedded cross-origin
    // iframe.
    bool expected_notifications =
        expected && (rfh->GetLastCommittedOrigin() !=
                     url::Origin::Create(GetChildFrameURL()));
    expected_result = expected_notifications ? "granted" : "denied";

    EXPECT_EQ(expected_result, content::EvalJs(rfh, kRequestNotifications));

    CheckPermissionState(rfh, expected_notifications, expected, expected);
  }

  void RequestPermissionAndGrant(content::RenderFrameHost* rfh,
                                 std::string request_script) {
    auto* manager = permissions::PermissionRequestManager::FromWebContents(
        GetWebContents());
    permissions::PermissionRequestObserver observer(GetWebContents());

    EXPECT_FALSE(manager->IsRequestInProgress());

    EXPECT_TRUE(content::ExecJs(
        rfh, request_script,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    // Wait until a permission request is shown.
    observer.Wait();

    EXPECT_TRUE(manager->IsRequestInProgress());
    EXPECT_TRUE(observer.request_shown());

    manager->Accept();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_test_server_;
};

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS, MainFrameTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());

  VerifyPermissionsAllowed(main_rfh);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       OverridesForDevToolsTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());

  content::PermissionController* permission_controller =
      main_rfh->GetBrowserContext()->GetPermissionController();
  url::Origin origin = url::Origin::Create(GetMainFrameURL());

  SetPermissionControllerOverrideForDevTools(
      permission_controller, origin, blink::PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::GRANTED);

  CheckPermissionState(main_rfh, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/false);

  SetPermissionControllerOverrideForDevTools(
      permission_controller, origin, blink::PermissionType::VIDEO_CAPTURE,
      blink::mojom::PermissionStatus::GRANTED);

  CheckPermissionState(main_rfh, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/true);

  SetPermissionControllerOverrideForDevTools(
      permission_controller, origin, blink::PermissionType::NOTIFICATIONS,
      blink::mojom::PermissionStatus::GRANTED);

  CheckPermissionState(main_rfh, /*notifications_allowed=*/true,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/true);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       TopFramePermissionRequest) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetWebContents();

  EXPECT_TRUE(content::NavigateToURL(web_contents, GetMainFrameURL()));

  RequestPermissionAndGrant(web_contents->GetPrimaryMainFrame(),
                            kRequestGeolocation);

  histograms.ExpectUniqueSample("Permissions.Request.SameOrigin.MainFrame",
                                blink::PermissionType::GEOLOCATION, 1);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       SubFrameSameOriginPermissionRequest) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetWebContents();

  EXPECT_TRUE(content::NavigateToURL(web_contents, GetMainFrameURL()));

  content::RenderFrameHost* sameorigin_subframe = CreateIframe(
      web_contents->GetPrimaryMainFrame(), GetMainFrameURL(), kIframePolicy);
  ASSERT_TRUE(sameorigin_subframe);

  RequestPermissionAndGrant(sameorigin_subframe, kRequestGeolocation);

  histograms.ExpectUniqueSample("Permissions.Request.SameOrigin.SubFrame",
                                blink::PermissionType::GEOLOCATION, 1);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       SubFrameCrossOriginPermissionRequest) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetWebContents();

  EXPECT_TRUE(content::NavigateToURL(web_contents, GetMainFrameURL()));

  content::RenderFrameHost* crossorigin_subframe = CreateIframe(
      web_contents->GetPrimaryMainFrame(), GetChildFrameURL(), kIframePolicy);
  ASSERT_TRUE(crossorigin_subframe);

  RequestPermissionAndGrant(crossorigin_subframe, kRequestGeolocation);

  histograms.ExpectUniqueSample("Permissions.Request.CrossOrigin",
                                blink::PermissionType::GEOLOCATION, 1);
}

// Tests multiple layers of embedded iframes a.com(b.com(a.com)).
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       DeepSubFrameCrossOriginPermissionRequest) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetWebContents();

  EXPECT_TRUE(content::NavigateToURL(web_contents, GetMainFrameURL()));

  content::RenderFrameHost* crossorigin_subframe = CreateIframe(
      web_contents->GetPrimaryMainFrame(), GetChildFrameURL(), kIframePolicy);
  ASSERT_TRUE(crossorigin_subframe);

  content::RenderFrameHost* crossorigin_sub_subframe =
      CreateIframe(crossorigin_subframe, GetMainFrameURL(), kIframePolicy);
  ASSERT_TRUE(crossorigin_sub_subframe);

  EXPECT_TRUE(
      crossorigin_sub_subframe->GetLastCommittedOrigin().IsSameOriginWith(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin()));

  RequestPermissionAndGrant(crossorigin_sub_subframe, kRequestGeolocation);

  histograms.ExpectUniqueSample("Permissions.Request.CrossOrigin",
                                blink::PermissionType::GEOLOCATION, 1);
}

IN_PROC_BROWSER_TEST_F(
    PermissionsSecurityModelHTTPS,
    MainFrameAndCrossOriginIframeWithoutPermissionsPolicyTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());
  content::RenderFrameHost* subframe =
      CreateIframe(main_rfh, GetChildFrameURL());
  ASSERT_TRUE(subframe);
  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  HostContentSettingsMap* HCSM = HostContentSettingsMapFactory::GetForProfile(
      main_rfh->GetBrowserContext());

  HCSM->SetContentSettingDefaultScope(GetChildFrameURL(), GetChildFrameURL(),
                                      ContentSettingsType::GEOLOCATION,
                                      CONTENT_SETTING_ALLOW);

  HCSM->SetContentSettingDefaultScope(GetMainFrameURL(), GetMainFrameURL(),
                                      ContentSettingsType::GEOLOCATION,
                                      CONTENT_SETTING_ALLOW);

  HCSM->SetContentSettingDefaultScope(GetChildFrameURL(), GetChildFrameURL(),
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_ALLOW);

  HCSM->SetContentSettingDefaultScope(GetMainFrameURL(), GetMainFrameURL(),
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_ALLOW);

  CheckPermissionState(main_rfh, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/true);
  // Geolocation and Camera are not allowed for `subframe` because it is a
  // cross-origin child iframe in `main_rfh`.
  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  HCSM->SetContentSettingDefaultScope(GetChildFrameURL(), GetChildFrameURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_ALLOW);

  CheckPermissionState(main_rfh, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/true);
  // Notifications permission is allowed for `subframe` even despite it being a
  // cross-origin child iframe in `main_rfh`.
  CheckPermissionState(subframe, /*notifications_allowed=*/true,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  HCSM->SetContentSettingDefaultScope(GetMainFrameURL(), GetMainFrameURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_ALLOW);

  CheckPermissionState(main_rfh, /*notifications_allowed=*/true,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/true);
  CheckPermissionState(subframe, /*notifications_allowed=*/true,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);
}

// Preconditions: The embedded cross-origin iframe has no permissions policy.
// Permissions requested from both the main frame and the embedded cross-origin
// iframe.
IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       RequestPermissionsOnlyFromMainFrameTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());
  content::RenderFrameHost* subframe =
      CreateIframe(main_rfh, GetChildFrameURL());
  ASSERT_TRUE(subframe);

  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  RequestPermissions(main_rfh, true);

  RequestPermissions(subframe, false);
}

// Preconditions: The embedded cross-origin iframe has no permissions policy.
// Permissions requested only from the embedded cross-origin iframe.
IN_PROC_BROWSER_TEST_F(
    PermissionsSecurityModelHTTPS,
    RequestPermissionsOnlyFromCrossOriginIframeWithoutPermissionsPolicyTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());
  content::RenderFrameHost* subframe =
      CreateIframe(main_rfh, GetChildFrameURL());
  ASSERT_TRUE(subframe);

  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  RequestPermissions(subframe, false);
  CheckPermissionState(main_rfh, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);
}

// Preconditions: The embedded cross-origin iframe has permissions policy.
// Permissions requested only from the embedded cross-origin iframe.
IN_PROC_BROWSER_TEST_F(
    PermissionsSecurityModelHTTPS,
    RequestPermissionsOnlyFromCrossOriginIframeWithPermissionsPolicyTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());
  content::RenderFrameHost* subframe =
      CreateIframe(main_rfh, GetChildFrameURL(), kIframePolicy);
  ASSERT_TRUE(subframe);

  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  RequestPermissions(subframe, true);
}

// Preconditions: The embedded cross-origin iframe has permissions policy.
// Permissions requested from the main frame only.
IN_PROC_BROWSER_TEST_F(
    PermissionsSecurityModelHTTPS,
    RequestPermissionsOnlyFromMainFrameCrossOriginIframeWithPermissionsPolicyTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());
  content::RenderFrameHost* subframe =
      CreateIframe(main_rfh, GetChildFrameURL(), kIframePolicy);
  ASSERT_TRUE(subframe);
  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  RequestPermissions(main_rfh, true);

  // Notifications permission is not allowed for an embedded cross-origin
  // iframe.
  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/true, /*camera_allowed=*/true);
}

// Preconditions: The embedded cross-origin iframe has permissions policy.
// Permissions requested from both the main frame and the embedded cross-origin
// iframe.
IN_PROC_BROWSER_TEST_F(
    PermissionsSecurityModelHTTPS,
    RequestPermissionsFromBothMainframeAndCrossOriginIframeWithPermissionsPolicyTest) {
  content::RenderFrameHost* main_rfh =
      NavigateAndCheckPermissionState(GetMainFrameURL());
  content::RenderFrameHost* subframe =
      CreateIframe(main_rfh, GetChildFrameURL(), kIframePolicy);
  ASSERT_TRUE(subframe);

  CheckPermissionState(subframe, /*notifications_allowed=*/false,
                       /*geolocation_allowed=*/false, /*camera_allowed=*/false);

  RequestPermissions(main_rfh, true);

  RequestPermissions(subframe, true);
}

class PermissionsRequestedFromFencedFrameTest
    : public PermissionsSecurityModelInteractiveUITest {
 public:
  PermissionsRequestedFromFencedFrameTest() = default;
  ~PermissionsRequestedFromFencedFrameTest() override = default;

  PermissionsRequestedFromFencedFrameTest(
      const PermissionsRequestedFromFencedFrameTest&) = delete;
  PermissionsRequestedFromFencedFrameTest& operator=(
      const PermissionsRequestedFromFencedFrameTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PermissionsRequestedFromFencedFrameTest,
                       PermissionsRequestedFromFencedFrameTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  VerifyPermissionsDeniedForFencedFrame(web_contents, fenced_frame_host);
}

class PermissionRequestWithPrerendererTest
    : public PermissionsSecurityModelInteractiveUITest {
 public:
  PermissionRequestWithPrerendererTest()
      : prerender_helper_(base::BindRepeating(
            &PermissionRequestWithPrerendererTest::GetActiveWebContents,
            base::Unretained(this))) {}

  ~PermissionRequestWithPrerendererTest() override = default;

  PermissionRequestWithPrerendererTest(
      const PermissionRequestWithPrerendererTest&) = delete;
  PermissionRequestWithPrerendererTest& operator=(
      const PermissionRequestWithPrerendererTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PermissionsSecurityModelInteractiveUITest::SetUp();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestWithPrerendererTest,
                       PermissionsRequestedFromPrerendererTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  EXPECT_FALSE(
      GetActiveWebContents()
          ->GetPrimaryMainFrame()
          ->IsInactiveAndDisallowActivation(
              content::DisallowActivationReasonId::kRequestPermission));

  // Start a prerender.
  GURL prerender_url =
      embedded_test_server()->GetURL("/prerenderer_geolocation_test.html");
  prerender_helper().AddPrerender(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);

  content::RenderFrameHost* prerender_render_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);

  ASSERT_TRUE(prerender_render_frame_host);

  // The main frame of an outer document is not a prerenderer.
  EXPECT_FALSE(
      GetActiveWebContents()
          ->GetPrimaryMainFrame()
          ->IsInactiveAndDisallowActivation(
              content::DisallowActivationReasonId::kRequestPermission));

  // The main frame of a newly created frame tree is a prerenderer. It is
  // inactive, all permission requests should be automatically denied.
  // (crbug.com/1126305): Do not use RFH::IsInactiveAndDisallowActivation() as
  // it will stop prerendering process.
  EXPECT_EQ(prerender_render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(
          GetActiveWebContents());
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_EQ(
      true,
      content::ExecJs(
          prerender_render_frame_host, "accessGeolocation();",
          content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE |
              content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  // Run a event loop so the page can fail the test.
  EXPECT_TRUE(content::ExecJs(prerender_render_frame_host, "runLoop();"));

  // Avoid race conditions, which can lead to a situation where we check for a
  // permission prompt before it was shown.
  base::RunLoop().RunUntilIdle();
  // `accessGeolocation` will request Geolocation permission which should be
  // automatically accepted, but it will not happen here, because the
  // permissions API is deferred in Prerenderer.
  EXPECT_EQ(0, bubble_factory->TotalRequestCount());

  // Activate the prerenderer.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  EXPECT_EQ(prerender_render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Wait for the completion of `accessGeolocation`.
  EXPECT_EQ(true, content::EvalJs(prerender_render_frame_host, "result;"));
  // Check the event sequence seen in the prerendered page.
  content::EvalJsResult results =
      content::EvalJs(prerender_render_frame_host, "eventsSeen");
  std::vector<std::string> eventsSeen;
  base::Value resultsList = results.ExtractList();
  for (const auto& result : resultsList.GetList())
    eventsSeen.push_back(result.GetString());
  EXPECT_THAT(eventsSeen, testing::ElementsAreArray(
                              {"accessGeolocation (prerendering: true)",
                               "prerenderingchange (prerendering: false)",
                               "getCurrentPosition (prerendering: false)"}));

  // Wait until a permission prompt is resolved.
  base::RunLoop().RunUntilIdle();
  // After the prerenderer activation, a deferred permission request will be
  // displayed.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelHTTPS,
                       PermissionsRequestedFromBFCacheTest) {
  GURL url_a = GetURL("a.test", "/title1.html");
  GURL url_b = GetURL("b.test", "/title1.html");
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(embedder_contents, url_a));
  content::RenderFrameHost* rfh_a = embedder_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh_a);
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  // Currently active RFH is not `kInBackForwardCache`.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(embedder_contents, url_b));
  content::RenderFrameHost* rfh_b = embedder_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh_b);
  EXPECT_EQ(origin_b, rfh_b->GetLastCommittedOrigin());
  // Currently active RFH is not `kInBackForwardCache`.
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // `rfh_a` is not deleted because it is in bfcahce.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // `rfh_a` is no longer `kActive` but `kInBackForwardCache`.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Verify that `HistoryGoBack` restores previously cached `rfh_a` and does
  // not create a new one.
  EXPECT_NE(rfh_a, embedder_contents->GetPrimaryMainFrame());
  // After `HistoryGoBack` `rfh_a` should be moved back from the BFCache.
  ASSERT_TRUE(HistoryGoBack(embedder_contents));
  EXPECT_EQ(rfh_a, embedder_contents->GetPrimaryMainFrame());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  EXPECT_TRUE(content::NavigateToURL(embedder_contents, url_b));
  // `rfh_a` is not deleted because it is in bfcahce.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // `rfh_a` is no longer `kActive` but `kInBackForwardCache`.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Verify and request permissions. The main frame works as expected but
  // permission verification fails on `rfh_a`. `rfh_a` gets evicted from the
  // BFCache.
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_FALSE(content::EvalJs(embedder_contents->GetPrimaryMainFrame(),
                               kCheckGeolocation)
                   .value.GetBool());

  EXPECT_EQ("granted", content::EvalJs(embedder_contents->GetPrimaryMainFrame(),
                                       kRequestGeolocation));
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Run JavaScript on a page in the back-forward cache. The page should be
  // evicted. As the frame is deleted, ExecJs returns false without executing.
  // Run without user gesture to prevent UpdateUserActivationState message
  // being sent back to browser.
  EXPECT_FALSE(content::ExecJs(rfh_a, kRequestGeolocation));

  // No permission prompt bubble has been shown for `rfh_a`.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();
  // `rfh_a` is deleted because it was evicted.
  EXPECT_TRUE(delete_observer_rfh_a.deleted());

  // 5) Go back to a.test and verify that it has no permissions.
  ASSERT_TRUE(HistoryGoBack(embedder_contents));
  content::RenderFrameHost* rfh_a_2 = embedder_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh_a_2);
  EXPECT_EQ(origin_a, rfh_a_2->GetLastCommittedOrigin());
  // Verify that `a.test` has no granted Geolocation permission despite it being
  // requested above.
  EXPECT_FALSE(content::EvalJs(rfh_a_2, kCheckGeolocation).value.GetBool());
}

class PermissionRequestFromExtension : public extensions::ExtensionApiTest {
 public:
  PermissionRequestFromExtension() {
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
  }

  ~PermissionRequestFromExtension() override = default;

  PermissionRequestFromExtension(const PermissionRequestFromExtension&) =
      delete;
  PermissionRequestFromExtension& operator=(
      const PermissionRequestFromExtension&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));

    ExtensionApiTest::SetUpOnMainThread();
  }

 protected:
  GURL GetTestServerInsecureUrl(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);

    GURL::Replacements replace_host_and_scheme;
    replace_host_and_scheme.SetHostStr("a.test");
    replace_host_and_scheme.SetSchemeStr("http");
    url = url.ReplaceComponents(replace_host_and_scheme);

    return url;
  }

  void EnsurePopupActive() {
    auto test_util = ExtensionActionTestHelper::Create(browser());
    EXPECT_TRUE(test_util->HasPopup());
    ASSERT_NO_FATAL_FAILURE(test_util->WaitForPopup());
    EXPECT_TRUE(test_util->HasPopup());
  }

  // Open an extension popup by clicking the browser action button associated
  // with `id`.
  content::WebContents* OpenPopupViaToolbar(const std::string& id) {
    EXPECT_FALSE(id.empty());
    content::CreateAndLoadWebContentsObserver popup_observer;
    ExtensionActionTestHelper::Create(browser())->Press(id);
    content::WebContents* popup = popup_observer.Wait();
    EnsurePopupActive();
    return popup;
  }

  void VerifyExtensionsPopupPage(std::string extension_path) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents);
    std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Enable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    extensions::ResultCatcher catcher;
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(extension_path));

    ASSERT_TRUE(extension);

    // Open a popup with the extension.
    content::WebContents* extension_popup =
        OpenPopupViaToolbar(extension->id());
    ASSERT_TRUE(extension_popup);

    // Wait for all JS tests to resolve their promises.
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

    // Showing permission prompts is not allowed on the extension's popup page.
    EXPECT_EQ(0, bubble_factory->TotalRequestCount());
  }

  void VerifyExtensionsOptionsPage(
      std::string extension_path,
      int shown_prompts,
      permissions::PermissionRequestManager::AutoResponseType type =
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents);
    std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Enable auto-accept of a permission request.
    bubble_factory->set_response_type(type);

    extensions::ResultCatcher catcher;
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(extension_path));

    ASSERT_TRUE(extension);
    ASSERT_TRUE(extensions::OptionsPageInfo::HasOptionsPage(extension));

    GURL options_url = extensions::OptionsPageInfo::GetOptionsPage(extension);
    EXPECT_TRUE(
        extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser()));

    // Opening the options page should take the new tab and use it, so we should
    // have only one tab, and it should be open to the options page.
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    EXPECT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));
    EXPECT_EQ(options_url, browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetLastCommittedURL());

    // Wait for all JS tests to resolve their promises.
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

    // Prompts for: Notifications, Geolocation, Camera, Microphone.
    EXPECT_EQ(shown_prompts, bubble_factory->TotalRequestCount());
  }

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       EmbeddedIframeWithPermissionsTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/embedded_into_iframe/has_permissions"));
  // Permissions work differently if they are not declared in an extension's
  // manifest.
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kGeolocation));
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kNotifications));

  GURL url = GetTestServerInsecureUrl("/extensions/test_file.html?succeed");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(main_rfh);

  content::RenderFrameHost* iframe_with_embedded_extension =
      content::FrameMatchingPredicate(
          main_rfh->GetPage(),
          base::BindRepeating(&content::FrameMatchesName,
                              "iframe_with_embedded_extension"));
  ASSERT_TRUE(iframe_with_embedded_extension);

  // Notifications are enabled by default if the Notifications permission is
  // declared in an extension's manifest.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension,
                              kCheckNotifications,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension,
                               kCheckGeolocation,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestGeolocation,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
  // Despite Geolocation being granted above, its state is `prompt`.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension,
                               kCheckGeolocation,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  // There was no permission prompt shown.
  EXPECT_EQ(0, bubble_factory->TotalRequestCount());

  // Microphone is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestMicrophone,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
  // Microphone is enabled.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckMicrophone,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // Camera is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckCamera,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestCamera,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
  // Camera is enabled.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckCamera,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // Only Camera and Microphone will show a prompt on permission request.
  EXPECT_EQ(2, bubble_factory->TotalRequestCount());
  EXPECT_TRUE(bubble_factory->RequestOriginSeen(
      iframe_with_embedded_extension->GetLastCommittedOrigin().GetURL()));
  EXPECT_TRUE(iframe_with_embedded_extension->GetLastCommittedOrigin()
                  .GetURL()
                  .SchemeIs(extensions::kExtensionScheme));
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       EmbeddedIframeWithNoPermissionsTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/embedded_into_iframe/no_permissions"));

  EXPECT_FALSE(extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kGeolocation));
  EXPECT_FALSE(extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kNotifications));

  GURL url = GetTestServerInsecureUrl("/extensions/test_file.html?succeed");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(main_rfh);

  content::RenderFrameHost* iframe_with_embedded_extension =
      content::FrameMatchingPredicate(
          main_rfh->GetPage(),
          base::BindRepeating(&content::FrameMatchesName,
                              "iframe_with_embedded_extension"));
  ASSERT_TRUE(iframe_with_embedded_extension);
  EXPECT_TRUE(iframe_with_embedded_extension->GetLastCommittedOrigin()
                  .GetURL()
                  .SchemeIs(extensions::kExtensionScheme));

  // Notification permission is disabled if 'notification' is not declared in an
  // extension's manifest.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension,
                               kCheckNotifications,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension,
                               kCheckGeolocation,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestGeolocation,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckGeolocation,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // A permission prompt is shown.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Microphone is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestMicrophone,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
  // Microphone is enabled.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckMicrophone,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // Camera is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckCamera,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestCamera,
                            content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
  // Camera is enabled.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckCamera,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // Geolocation, Camera and Microphone will show a prompt on permission
  // request.
  EXPECT_EQ(3, bubble_factory->TotalRequestCount());
}

// `host` has all needed permissions, hence an extension can use them.
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       ContentScriptAllowedTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(main_rfh);

  // Allow permissions on the main frame, so they became available for an
  // extension.
  VerifyPermissionsAllowed(main_rfh);

  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_content_script_allowed"));

  ASSERT_TRUE(extension);

  // Another navigation to activate the extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// `host` does not have needed permissions, hence an extension can request them.
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       ContentScriptPromptTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");

  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_content_script_prompt"));

  ASSERT_TRUE(extension);

  // Another navigation to activate the extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  // The above loaded extension requests Notifications, Geolocation, Camera,
  // Microphone.
  EXPECT_EQ(4, bubble_factory->TotalRequestCount());
}

// Permissions requests are not allowed. All permissions denied.
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       ContentScriptDeniedTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_content_script_not_allowed"));

  ASSERT_TRUE(extension);

  GURL url = GetTestServerInsecureUrl("/extensions/test_file.html");

  // Another navigation to activate the extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  // No permission prompts has been shown.
  EXPECT_EQ(0, bubble_factory->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       BackgroundV3HasPermissionsTest) {
  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_background_v3/has_permissions"));

  ASSERT_TRUE(extension);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       BackgroundV3NoPermissionsTest) {
  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_background_v3/has_permissions"));

  ASSERT_TRUE(extension);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       BackgroundV2HasPermissionsTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_background_v2/has_permissions"));

  ASSERT_TRUE(extension);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  // No permission prompt has been shown.
  EXPECT_EQ(0, bubble_factory->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       BackgroundV2NoPermissionsTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-accept of a permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "permissions_test/request_from_background_v2/no_permissions"));

  ASSERT_TRUE(extension);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  // No permission prompt has been shown.
  EXPECT_EQ(0, bubble_factory->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       PopupPageNoPermissonsV2Test) {
  VerifyExtensionsPopupPage(
      "permissions_test/request_from_popup_v2/no_permissions");
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       PopupPageHasPermissonsV2Test) {
  VerifyExtensionsPopupPage(
      "permissions_test/request_from_popup_v2/has_permissions");
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       PopupPageNoPermissonsV3Test) {
  VerifyExtensionsPopupPage(
      "permissions_test/request_from_popup_v3/no_permissions");
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       PopupPageHasPermissonsV3Test) {
  VerifyExtensionsPopupPage(
      "permissions_test/request_from_popup_v3/has_permissions");
}

// crbug.com/1356314 Failed on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OptionsPageNoPermissonsV2Test \
  DISABLED_OptionsPageNoPermissonsV2Test
#else
#define MAYBE_OptionsPageNoPermissonsV2Test OptionsPageNoPermissonsV2Test
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       MAYBE_OptionsPageNoPermissonsV2Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v2/no_permissions",
      /*shown_prompts=*/4);
}

// crbug.com/1356314 Failed on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OptionsPageHasPermissonsV2Test \
  DISABLED_OptionsPageHasPermissonsV2Test
#else
#define MAYBE_OptionsPageHasPermissonsV2Test OptionsPageHasPermissonsV2Test
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       MAYBE_OptionsPageHasPermissonsV2Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v2/has_permissions",
      /*shown_prompts=*/2);
}

// crbug.com/1356314 Failed on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OptionsPageNoPermissonsV3Test \
  DISABLED_OptionsPageNoPermissonsV3Test
#else
#define MAYBE_OptionsPageNoPermissonsV3Test OptionsPageNoPermissonsV3Test
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       MAYBE_OptionsPageNoPermissonsV3Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v3/no_permissions",
      /*shown_prompts=*/4);
}

// crbug.com/1356314 Failed on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OptionsPageHasPermissonsV3Test \
  DISABLED_OptionsPageHasPermissonsV3Test
#else
#define MAYBE_OptionsPageHasPermissonsV3Test OptionsPageHasPermissonsV3Test
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       MAYBE_OptionsPageHasPermissonsV3Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v3/has_permissions",
      /*shown_prompts=*/2);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       OptionsPageHasPermissonsV3NegativeTest) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v3/has_permissions_negative",
      /*shown_prompts=*/2,
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       ExtensionAccessToCSPSandboxedFrameTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL(
      "example.com", "/extensions/page_with_sandbox_csp.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  extensions::ResultCatcher catcher;
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("sandbox_csp"));

  ASSERT_TRUE(extension);

  // Open a popup with the extension.
  content::WebContents* extension_popup = OpenPopupViaToolbar(extension->id());
  ASSERT_TRUE(extension_popup);

  // Wait for all JS tests to resolve their promises.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // anonymous namespace
