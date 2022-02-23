// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
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
#include "components/embedder_support/switches.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
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

content::WebContents* OpenPopup(Browser* browser, const GURL& url) {
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
            _ => { domAutomationController.send('granted')},
            _ => { domAutomationController.send('denied')});
        )";

constexpr char kCheckCamera[] = R"((async () => {
     const PermissionStatus =
        await navigator.permissions.query({name: 'camera'});
     return PermissionStatus.state === 'granted';
    })();)";

constexpr char kRequestCamera[] = R"(
    var constraints = { video: true };
    window.focus();
    navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
        domAutomationController.send('granted');
    })
    .catch(function(err) {
        domAutomationController.send('denied');
    });
    )";

constexpr char kRequestMicrophone[] = R"(
    var constraints = { audio: true };
    window.focus();
    navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
        domAutomationController.send('granted');
    })
    .catch(function(err) {
        domAutomationController.send('denied');
    });
    )";

constexpr char kCheckMic[] = R"((async () => {
     const PermissionStatus =
        await navigator.permissions.query({name: 'microphone'});
     return PermissionStatus.state === 'granted';
    })();)";

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

  bool is_notification = request_permission_script == kRequestNotifications;
  EXPECT_EQ("granted",
            content::EvalJs(main_rfh, request_permission_script,
                            is_notification
                                ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                .ExtractString());
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
        opener_or_embedder_contents->GetMainFrame();
    bool is_notification = request_permission_script == kRequestNotifications;
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
    EXPECT_EQ("granted",
              content::EvalJs(opener_rfh, request_permission_script,
                              is_notification
                                  ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                  : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());
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
    EXPECT_EQ("granted",
              content::EvalJs(test_rfh, request_permission_script,
                              is_notification
                                  ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                  : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());

    // There should not be the 2nd prompt.
    EXPECT_EQ(1, bubble_factory->TotalRequestCount());
  }

  void VerifyPermissionsDeniedForFencedFrame(
      content::WebContents* embedder_contents,
      content::RenderFrameHost* fenced_rfh,
      const std::string& request_permission_script,
      const std::string& check_permission_script) {
    content::RenderFrameHost* embedder_main_rfh =
        embedder_contents->GetMainFrame();
    bool is_notification = request_permission_script == kRequestNotifications;
    ASSERT_FALSE(content::EvalJs(embedder_main_rfh, check_permission_script)
                     .value.GetBool());
    ASSERT_FALSE(
        content::EvalJs(fenced_rfh, check_permission_script).value.GetBool());

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            embedder_contents);
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
              content::EvalJs(embedder_main_rfh, request_permission_script,
                              is_notification
                                  ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                  : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());
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
    EXPECT_EQ("denied",
              content::EvalJs(fenced_rfh, request_permission_script,
                              is_notification
                                  ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                  : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());

    // There should not be the 2nd prompt.
    EXPECT_EQ(1, bubble_factory->TotalRequestCount());
  }

  // getUserMedia requires focus. It should be verified only on a popup window.
  void VerifyPopupWindowGetUserMedia(content::WebContents* opener_contents,
                                     content::WebContents* popup_contents) {
    content::RenderFrameHost* opener_rfh = opener_contents->GetMainFrame();
    content::RenderFrameHost* popup_rfh = popup_contents->GetMainFrame();

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
    EXPECT_EQ("granted",
              content::EvalJs(popup_rfh, kRequestCamera,
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());
    EXPECT_EQ(1, bubble_factory->TotalRequestCount());

    // Disable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::NONE);

    EXPECT_TRUE(content::EvalJs(popup_rfh, kCheckCamera).value.GetBool());
    EXPECT_TRUE(content::EvalJs(opener_rfh, kCheckCamera).value.GetBool());
  }

  void VerifyPermissionsDeniedForPortal(
      content::WebContents* portal_contents,
      const std::string& request_permission_script,
      const std::string& check_permission_script) {
    content::RenderFrameHost* portal_main_rfh = portal_contents->GetMainFrame();
    bool is_notification = request_permission_script == kRequestNotifications;
    ASSERT_FALSE(content::EvalJs(portal_main_rfh, check_permission_script)
                     .value.GetBool());

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(portal_contents);
    std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    // Enable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    // Move the web contents to the foreground.
    portal_main_rfh->GetView()->Focus();
    ASSERT_TRUE(portal_main_rfh->GetView()->HasFocus());

    // Request permission on the portal contents.
    EXPECT_EQ("denied",
              content::EvalJs(portal_main_rfh, request_permission_script,
                              is_notification
                                  ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                  : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                  .ExtractString());
    EXPECT_EQ(0, bubble_factory->TotalRequestCount());

    // Disable auto-accept of a permission request.
    bubble_factory->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::NONE);

    EXPECT_FALSE(content::EvalJs(portal_main_rfh, check_permission_script)
                     .value.GetBool());
  }

  void VerifyPermissionsDeniedForPortal(content::WebContents* portal_contents) {
    const struct {
      std::string check_permission;
      std::string request_permission;
    } kTests[] = {
        {kCheckNotifications, kRequestNotifications},
        {kCheckGeolocation, kRequestGeolocation},
        {kCheckCamera, kRequestCamera},
    };

    for (const auto& test : kTests) {
      VerifyPermissionsDeniedForPortal(portal_contents, test.request_permission,
                                       test.check_permission);
    }
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

  void VerifyPermissionsAlreadyGranted(content::WebContents* web_contents) {
    const struct {
      std::string check_permission;
      std::string request_permission;
    } kTests[] = {
        {kCheckNotifications, kRequestNotifications},
        {kCheckGeolocation, kRequestGeolocation},
        {kCheckCamera, kRequestCamera},
    };

    for (const auto& test : kTests) {
      ASSERT_TRUE(
          content::EvalJs(web_contents, test.check_permission).value.GetBool());
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
                content::EvalJs(rfh, test.request_permission,
                                test.request_permission == kRequestNotifications
                                    ? content::EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                    : content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                    .ExtractString());

      ASSERT_FALSE(content::EvalJs(rfh, test.check_permission).value.GetBool());
    }
  }

  // Tests of permissions behavior for an inheritance and embedding of an
  // origin.
  class PermissionsSecurityModelInteractiveUITest
      : public InProcessBrowserTest {
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
        ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                  url, 1);
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
                                      popup_contents->GetMainFrame());
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

  VerifyPermissionsExceptGetUserMedia(opener_contents,
                                      blob_popup_contents->GetMainFrame());
  VerifyPopupWindowGetUserMedia(opener_contents, blob_popup_contents);
}

IN_PROC_BROWSER_TEST_F(PermissionsSecurityModelInteractiveUITest,
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

  VerifyPermissionsExceptGetUserMedia(embedder_contents, embedded_iframe_rfh);
  VerifyPermission(embedder_contents, embedded_iframe_rfh, kRequestCamera,
                   kCheckCamera);
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
          chrome::FindBrowserWithWebContents(popup_iframe_web_contents), fs_url,
          1);

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

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ true);

  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ false);
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

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ true);

  content::EvalJsResult result = content::EvalJs(
      embedder_contents, "history.pushState({}, {}, 'about:blank');");
  EXPECT_EQ(std::string(), result.error);
  EXPECT_EQ("about:blank", main_rfh->GetLastCommittedURL().spec());
  EXPECT_TRUE(main_rfh->GetLastCommittedURL().IsAboutBlank());

  VerifyPermissionsForFile(main_rfh, /*expect_granted*/ false);
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
  EXPECT_EQ(
      GURL(chrome::kChromeUINewTabURL),
      embedder_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL());
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
  EXPECT_EQ(
      GURL(chrome::kChromeUINewTabURL),
      embedder_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL());
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
      fenced_frame_test_helper().CreateFencedFrame(web_contents->GetMainFrame(),
                                                   fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  VerifyPermissionsDeniedForFencedFrame(web_contents, fenced_frame_host);
}

class PermissionRequestWithPortalTest
    : public PermissionsSecurityModelInteractiveUITest {
 public:
  PermissionRequestWithPortalTest() = default;
  ~PermissionRequestWithPortalTest() override = default;

  PermissionRequestWithPortalTest(const PermissionRequestWithPortalTest&) =
      delete;
  PermissionRequestWithPortalTest& operator=(
      const PermissionRequestWithPortalTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals,
                              blink::features::kPortalsCrossOrigin},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestWithPortalTest,
                       PermissionsRequestedFromPortalTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* contents = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1, tab_strip_model->count());

  // `contents` is in a default state.
  EXPECT_FALSE(contents->IsPortal());
  VerifyPermissionsAllowed(contents->GetMainFrame());

  EXPECT_EQ(true, content::EvalJs(contents, "loadPromise"));
  std::vector<content::WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  EXPECT_EQ(1u, inner_web_contents.size());
  content::WebContents* portal_contents = inner_web_contents[0];

  // `portal_contents` is in a portal state. All permissions will be
  // automatically denied.
  EXPECT_TRUE(portal_contents->IsPortal());
  VerifyPermissionsDeniedForPortal(portal_contents);

  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(1, tab_strip_model->count());
  // After a portal activation, `portal_contents` became a top-level
  // web_contents in a tab.
  EXPECT_EQ(portal_contents, tab_strip_model->GetActiveWebContents());

  // Because `portal_contents` was activated, it stopped being a portal and its
  // predecessor (i.e. the page that was previously embedding the portal) got
  // put into a portal itself. So `contents` here is the predecessor and is a
  // portal now, and `portal_contents` is now a top-level web_contents and isn't
  // a portal anymore.
  EXPECT_TRUE(contents->IsPortal());
  EXPECT_FALSE(portal_contents->IsPortal());

  // All permissoins are automatically denied for `contents`
  VerifyPermissionsDeniedForPortal(contents);
  // Permissions were previously granted to `contents`, hence they are now
  // granted to `portal_contents` as well because they have the same origin.
  VerifyPermissionsAlreadyGranted(portal_contents);
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
    prerender_helper_.SetUp(embedded_test_server());
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
      GetActiveWebContents()->GetMainFrame()->IsInactiveAndDisallowActivation(
          content::DisallowActivationReasonId::kRequestPermission));

  // Start a prerender.
  GURL prerender_url =
      embedded_test_server()->GetURL("/prerenderer_geolocation_test.html");
  prerender_helper().AddPrerender(prerender_url);
  int host_id = prerender_helper().AddPrerender(prerender_url);

  content::RenderFrameHost* prerender_render_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);

  ASSERT_TRUE(prerender_render_frame_host);

  // The main frame of an outer document is not a prerenderer.
  EXPECT_FALSE(
      GetActiveWebContents()->GetMainFrame()->IsInactiveAndDisallowActivation(
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
  for (auto& result : resultsList.GetListDeprecated())
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

class PermissionRequestWithBFCacheTest
    : public PermissionsSecurityModelInteractiveUITest {
 public:
  PermissionRequestWithBFCacheTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PermissionRequestWithBFCacheTest() override = default;

  PermissionRequestWithBFCacheTest(const PermissionRequestWithBFCacheTest&) =
      delete;
  PermissionRequestWithBFCacheTest& operator=(
      const PermissionRequestWithBFCacheTest&) = delete;

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
  }

 protected:
  GURL GetURL(const std::string& hostname, const std::string& relative_url) {
    return https_test_server_.GetURL(hostname, relative_url);
  }

  net::EmbeddedTestServer* GetHttpsServer() { return &https_test_server_; }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_test_server_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestWithBFCacheTest,
                       PermissionsRequestedFromBFCacheTest) {
  ASSERT_TRUE(GetHttpsServer()->Start());
  GURL url_a = GetURL("a.test", "/title1.html");
  GURL url_b = GetURL("b.test", "/title1.html");
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(embedder_contents, url_a));
  content::RenderFrameHost* rfh_a = embedder_contents->GetMainFrame();
  ASSERT_TRUE(rfh_a);
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  // Currently active RFH is not `kInBackForwardCache`.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(embedder_contents, url_b));
  content::RenderFrameHost* rfh_b = embedder_contents->GetMainFrame();
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
  EXPECT_NE(rfh_a, embedder_contents->GetMainFrame());
  // After `HistoryGoBack` `rfh_a` should be moved back from the BFCache.
  ASSERT_TRUE(HistoryGoBack(embedder_contents));
  EXPECT_EQ(rfh_a, embedder_contents->GetMainFrame());
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

  EXPECT_FALSE(
      content::EvalJs(embedder_contents->GetMainFrame(), kCheckGeolocation)
          .value.GetBool());

  EXPECT_EQ("granted", content::EvalJs(embedder_contents->GetMainFrame(),
                                       kRequestGeolocation,
                                       content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
                           .ExtractString());
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Run JavaScript on a page in the back-forward cache. The page should be
  // evicted. As the frame is deleted, ExecJs returns false without executing.
  // Run without user gesture to prevent UpdateUserActivationState message
  // being sent back to browser.
  EXPECT_FALSE(content::ExecJs(rfh_a, kRequestGeolocation,
                               content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // No permission prompt bubble has been shown for `rfh_a`.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();
  // `rfh_a` is deleted because it was evicted.
  EXPECT_TRUE(delete_observer_rfh_a.deleted());

  // 5) Go back to a.test and verify that it has no permissions.
  ASSERT_TRUE(HistoryGoBack(embedder_contents));
  content::RenderFrameHost* rfh_a_2 = embedder_contents->GetMainFrame();
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
    EXPECT_TRUE(test_util->WaitForPopup());
    EXPECT_TRUE(test_util->HasPopup());
  }

  // Open an extension popup by clicking the browser action button associated
  // with `id`.
  content::WebContents* OpenPopupViaToolbar(const std::string& id) {
    EXPECT_FALSE(id.empty());
    content::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    ExtensionActionTestHelper::Create(browser())->Press(id);
    popup_observer.Wait();
    EnsurePopupActive();
    const auto& source =
        static_cast<const content::Source<content::WebContents>&>(
            popup_observer.source());
    return source.ptr();
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
  content::RenderFrameHost* main_rfh = web_contents->GetMainFrame();
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
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());
  // Despite Geolocation being granted above, its state is `prompt`.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension,
                               kCheckGeolocation,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  // There was no permission prompt shown.
  EXPECT_EQ(0, bubble_factory->TotalRequestCount());

  // Microphone is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckMic,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestMicrophone,
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());
  // Microphone is enabled.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckMic,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // Camera is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckCamera,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestCamera,
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());
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
  content::RenderFrameHost* main_rfh = web_contents->GetMainFrame();
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
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckGeolocation,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // A permission prompt is shown.
  EXPECT_EQ(1, bubble_factory->TotalRequestCount());

  // Microphone is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckMic,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestMicrophone,
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());
  // Microphone is enabled.
  EXPECT_TRUE(content::EvalJs(iframe_with_embedded_extension, kCheckMic,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                  .value.GetBool());

  // Camera is disabled by default.
  EXPECT_FALSE(content::EvalJs(iframe_with_embedded_extension, kCheckCamera,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());
  EXPECT_EQ("granted",
            content::EvalJs(iframe_with_embedded_extension, kRequestCamera,
                            content::EXECUTE_SCRIPT_USE_MANUAL_REPLY, 1)
                .ExtractString());
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
  content::RenderFrameHost* main_rfh = web_contents->GetMainFrame();
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

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       OptionsPageNoPermissonsV2Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v2/no_permissions",
      /*shown_prompts=*/4);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       OptionsPageHasPermissonsV2Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v2/has_permissions",
      /*shown_prompts=*/2);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       OptionsPageNoPermissonsV3Test) {
  VerifyExtensionsOptionsPage(
      "permissions_test/request_from_options_v3/no_permissions",
      /*shown_prompts=*/4);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestFromExtension,
                       OptionsPageHasPermissonsV3Test) {
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

}  // anonymous namespace
