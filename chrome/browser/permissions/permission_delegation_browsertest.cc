// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/permissions/chrome_permissions_client.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class PermissionDelegationBrowserTest : public InProcessBrowserTest {
 public:
  PermissionDelegationBrowserTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(0, 0)) {}

  PermissionDelegationBrowserTest(const PermissionDelegationBrowserTest&) =
      delete;
  PermissionDelegationBrowserTest& operator=(
      const PermissionDelegationBrowserTest&) = delete;

  ~PermissionDelegationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebContents());
    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(&embedded_https_test_server());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override {
    mock_permission_prompt_factory_.reset();
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return mock_permission_prompt_factory_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(PermissionDelegationBrowserTest, DelegatedToTwoFrames) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Main frame is on a.com, iframe 1 is on b.com and iframe 2 is on c.com.
  GURL main_frame_url =
      embedded_https_test_server().GetURL("a.com", "/two_iframes_blank.html");
  GURL iframe_url_1 =
      embedded_https_test_server().GetURL("b.com", "/simple.html");
  GURL iframe_url_2 =
      embedded_https_test_server().GetURL("c.com", "/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  content::RenderFrameHost* main_frame =
      GetWebContents()->GetPrimaryMainFrame();

  // Delegate permission to both frames.
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      "document.getElementById('iframe1').allow = 'geolocation *';"));
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      "document.getElementById('iframe2').allow = 'geolocation *';"));

  // Load the iframes.
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "iframe1", iframe_url_1));
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "iframe2", iframe_url_2));

  content::RenderFrameHost* frame_1 = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "iframe1"));
  EXPECT_NE(nullptr, frame_1);
  content::RenderFrameHost* frame_2 = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "iframe2"));
  EXPECT_NE(nullptr, frame_2);

  // Request permission from the first iframe.
  EXPECT_EQ(true, content::EvalJs(
                      frame_1,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));

  // A prompt should have been shown with the top level origin rather than the
  // iframe origin.
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_TRUE(prompt_factory()->RequestOriginSeen(
      main_frame_url.DeprecatedGetOriginAsURL()));
  EXPECT_FALSE(prompt_factory()->RequestOriginSeen(
      iframe_url_1.DeprecatedGetOriginAsURL()));
  EXPECT_FALSE(prompt_factory()->RequestOriginSeen(
      iframe_url_2.DeprecatedGetOriginAsURL()));

  // Request permission from the second iframe. Because it was granted to the
  // top level frame, it should also be granted to this iframe and there should
  // be no prompt.
  EXPECT_EQ(true, content::EvalJs(
                      frame_2,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());

  // Request permission from the top level frame. It should already be granted
  // to this iframe and there should be no prompt.
  EXPECT_EQ(true, content::EvalJs(
                      main_frame,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

content::RenderFrameHost* FindMimeHandlerExtensionFrame(
    content::WebContents* web_contents) {
  auto* manager =
      extensions::mime_handler::MimeHandlerStreamManager::FromWebContents(
          web_contents);
  if (!manager) {
    return nullptr;
  }
  content::RenderFrameHost* extension_frame = nullptr;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&](content::RenderFrameHost* rfh) {
        if (manager->IsExtensionHost(rfh)) {
          extension_frame = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return extension_frame;
}

}  // namespace

class MimeHandlerPermissionEmbeddingBrowserTest
    : public extensions::ExtensionApiTest {
 public:
  MimeHandlerPermissionEmbeddingBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiMimeHandler},
        /*disabled_features=*/{});
  }

 protected:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    const base::FilePath chrome_test_data =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    embedded_test_server()->ServeFilesFromDirectory(
        chrome_test_data.AppendASCII("pdf"));
    embedded_test_server()->ServeFilesFromDirectory(chrome_test_data);
    ASSERT_TRUE(StartEmbeddedTestServer());
    ASSERT_TRUE(
        LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler")));
  }

  content::RenderFrameHost* NavigateToPdfAndGetExtensionFrame(
      const GURL& pdf_url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
    content::WebContents* web_contents = GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    return FindMimeHandlerExtensionFrame(web_contents);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MimeHandlerPermissionEmbeddingBrowserTest,
                       ExtensionFrameOverride) {
  const GURL pdf_url = embedded_test_server()->GetURL("a.com", "/test.pdf");
  content::RenderFrameHost* extension_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(extension_frame);

  const GURL extension_origin =
      extension_frame->GetLastCommittedOrigin().GetURL();
  std::optional<GURL> embedding_origin_override =
      ChromePermissionsClient::GetInstance()->GetEmbeddingOriginOverride(
          extension_origin, GetActiveWebContents());

  ASSERT_TRUE(embedding_origin_override.has_value());
  EXPECT_EQ(extension_origin, *embedding_origin_override);
}

IN_PROC_BROWSER_TEST_F(MimeHandlerPermissionEmbeddingBrowserTest,
                       ChildFrameOverride) {
  const GURL pdf_url = embedded_test_server()->GetURL("a.com", "/test.pdf");
  content::RenderFrameHost* extension_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(extension_frame);

  // Inject a cross-origin iframe inside the extension frame. The
  // override must still resolve to the extension origin for permission
  // requests originating from this child.
  const GURL child_url = embedded_test_server()->GetURL("b.com", "/empty.html");
  ASSERT_TRUE(content::ExecJs(
      extension_frame,
      content::JsReplace("var f = document.createElement('iframe');"
                         "f.src = $1;"
                         "document.body.appendChild(f);",
                         child_url)));
  ASSERT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));

  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(extension_frame, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_NE(extensions::kExtensionScheme,
            child_frame->GetLastCommittedOrigin().scheme());

  const GURL extension_origin =
      extension_frame->GetLastCommittedOrigin().GetURL();
  const GURL child_origin = child_frame->GetLastCommittedOrigin().GetURL();
  std::optional<GURL> embedding_origin_override =
      ChromePermissionsClient::GetInstance()->GetEmbeddingOriginOverride(
          child_origin, GetActiveWebContents());

  ASSERT_TRUE(embedding_origin_override.has_value());
  EXPECT_EQ(extension_origin, *embedding_origin_override);
}

// Regression test for crbug.com/495538206: when a non-extension
// top-level page embeds a PDF in a child iframe, a permission request
// from the top-level origin must NOT be re-keyed to the MIME handler
// extension origin. Otherwise an evil.com page that embeds an
// invisible PDF could have its own getUserMedia() prompt attributed
// to the extension and persisted against the extension's content
// settings.
IN_PROC_BROWSER_TEST_F(MimeHandlerPermissionEmbeddingBrowserTest,
                       OuterFrameRequesterDoesNotInheritExtensionOrigin) {
  // /pdf/test-iframe.html is a top-level HTML page on a.com that loads
  // <iframe src="test-bookmarks.pdf">. The PDF is intercepted by the
  // MIME handler extension and renders as an OOPIF inside the iframe;
  // the top-level RFH is a.com and is NOT a descendant of the
  // extension OOPIF.
  const GURL outer_url =
      embedded_test_server()->GetURL("a.com", "/pdf/test-iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), outer_url));
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // WaitForLoadStop can return before the inner PDF stream has been
  // intercepted and the extension OOPIF established. Poll until the
  // extension OOPIF appears, otherwise the test below would pass
  // vacuously against the buggy implementation.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return FindMimeHandlerExtensionFrame(web_contents) != nullptr; }));

  const GURL outer_origin = outer_url.DeprecatedGetOriginAsURL();
  std::optional<GURL> embedding_origin_override =
      ChromePermissionsClient::GetInstance()->GetEmbeddingOriginOverride(
          outer_origin, web_contents);

  EXPECT_FALSE(embedding_origin_override.has_value())
      << "got override " << embedding_origin_override.value_or(GURL());
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
