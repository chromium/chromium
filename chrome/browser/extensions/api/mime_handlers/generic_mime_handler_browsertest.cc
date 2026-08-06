// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/mime_handler/mime_handler_registry.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kTestExtensionDir[] = "generic_mime_handler";
constexpr char kTestPdfPath[] = "/test.pdf";
constexpr char kHeaderProbePdfPath[] = "/header-probe.pdf";
constexpr char kAuthTokenHeaderName[] = "X-Auth-Token";

// A strict policy whose `default-src 'none'` fallback would block the
// embedder document's inline styles absent the plugin-intercepted exemption.
constexpr char kStrictCspHeader[] =
    "default-src 'none'; img-src 'self'; media-src 'self'";

// Serves `pdf_body` at kHeaderProbePdfPath with a mix of response headers:
// CORS-safelisted ones (Content-Type via set_content_type, plus Cache-Control
// and Last-Modified) that a generic handler must still see, and a
// non-safelisted X-Auth-Token that must be filtered out. `pdf_body` is the
// real test.pdf, so the response body is a valid document rather than an
// ad-hoc literal (the test inspects only headers, so its exact bytes do not
// matter, but a valid PDF keeps the fixture honest).
std::unique_ptr<net::test_server::HttpResponse>
HandlePdfWithExtraResponseHeaders(
    const std::string& pdf_body,
    const net::test_server::HttpRequest& request) {
  // RegisterRequestHandler invokes every registered handler in turn; returning
  // nullptr for any other path lets the static-file handlers serve it.
  if (request.relative_url != kHeaderProbePdfPath) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type(kPdfMimeType);
  response->AddCustomHeader("Cache-Control", "max-age=0");
  response->AddCustomHeader("Last-Modified", "Wed, 21 Oct 2026 07:28:00 GMT");
  response->AddCustomHeader(kAuthTokenHeaderName, "s3cr3t");
  response->set_content(pdf_body);
  return response;
}

}  // namespace

class GenericMimeHandlerBrowserTest : public ExtensionApiTest {
 public:
  GenericMimeHandlerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiMimeHandler},
        /*disabled_features=*/{});
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    const base::FilePath test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    // Serve the real test.pdf so the MIME handler routing sees an actual
    // application/pdf payload, and the extension's host pages (handler.html,
    // embed_host.html, empty.html) from the extension's fixture directory.
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("pdf"));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII(kTestExtensionDir));
    // Reuse the shared test PDF as the probe response body, so it is a real,
    // valid document.
    std::string pdf_body;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(
          test_data_dir_.AppendASCII(kTestExtensionDir).AppendASCII("test.pdf"),
          &pdf_body));
    }
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HandlePdfWithExtraResponseHeaders, std::move(pdf_body)));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Returns the RFH identified by `MimeHandlerStreamManager` as the MIME
  // handler extension host in the active tab, or nullptr if none.
  content::RenderFrameHost* FindMimeHandlerExtensionFrame() {
    content::WebContents* web_contents = GetActiveWebContents();
    auto* manager =
        mime_handler::MimeHandlerStreamManager::FromWebContents(web_contents);
    if (!manager) {
      return nullptr;
    }
    content::RenderFrameHost* extension_frame = nullptr;
    web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
      if (manager->IsExtensionHost(rfh)) {
        extension_frame = rfh;
      }
    });
    return extension_frame;
  }

  // Loads the test extension, navigates to `url` (a resource the handler
  // intercepts), blocks until handler.js calls `chrome.test.succeed()`, and
  // returns the extension RFH (nullptr on failure). Also verifies the
  // architectural invariant that the extension frame is a cross-origin
  // iframe of a top-level embedder frame -- the scenario these tests target.
  content::RenderFrameHost* LoadHandlerAndGetExtensionFrame(const GURL& url) {
    EXPECT_TRUE(
        LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler")));
    EXPECT_TRUE(OpenTestURL(url, /*open_in_incognito=*/false));
    content::RenderFrameHost* extension_frame = FindMimeHandlerExtensionFrame();
    if (!extension_frame) {
      return nullptr;
    }
    EXPECT_FALSE(extension_frame->IsInPrimaryMainFrame());
    content::RenderFrameHost* embedder_frame = extension_frame->GetParent();
    EXPECT_TRUE(embedder_frame);
    EXPECT_TRUE(embedder_frame->IsInPrimaryMainFrame());
    EXPECT_NE(extension_frame->GetLastCommittedOrigin(),
              embedder_frame->GetLastCommittedOrigin());
    return extension_frame;
  }

  content::RenderFrameHost* LoadHandlerAndGetExtensionFrame() {
    return LoadHandlerAndGetExtensionFrame(
        embedded_test_server()->GetURL(kTestPdfPath));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that navigating to an application/pdf URL handled by a generic
// MIME handler extension loads the handler page in an OOPIF and that
// chrome.mimeHandler.getStreamInfo() returns correct stream metadata.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest, GetStreamInfo) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir));
  ASSERT_TRUE(extension);

  // Verify the extension registered as a generic MIME handler.
  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  ASSERT_TRUE(handler);
  auto* registry = MimeHandlerRegistry::Get(profile());
  ASSERT_TRUE(registry);
  ASSERT_FALSE(handler->IsPluginExtension());
  ASSERT_TRUE(handler->CanEmbedMimeType(kPdfMimeType));
  ASSERT_EQ(extension->id(), registry->GetHandlerForMimeType(kPdfMimeType));

  // Set up ResultCatcher before navigation so it catches the extension's
  // chrome.test.succeed() call.
  ResultCatcher catcher;

  // Navigate to an application/pdf resource. The throttle should intercept
  // this and route it through the generic MIME handler's OOPIF path.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(kTestPdfPath)));

  // The handler.js in the extension calls chrome.test.succeed() after
  // verifying getStreamInfo fields and fetching the stream data.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// A generic (third-party) MIME handler sees exactly the CORS-safelisted
// response headers fetch() would expose cross-origin: the safelisted
// Content-Type / Cache-Control / Last-Modified stay visible, while the custom
// X-Auth-Token is filtered out. handler.js already asserts mimeType and the
// %PDF- body on this same navigation, so this test covers only responseHeaders.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       GetStreamInfoFiltersNonSafelistedResponseHeaders) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  ResultCatcher catcher;
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    embedded_test_server()->GetURL(kHeaderProbePdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  content::RenderFrameHost* extension_frame = FindMimeHandlerExtensionFrame();
  ASSERT_TRUE(extension_frame);

  // Whether getStreamInfo()'s responseHeaders contains `name`
  // (case-insensitive).
  auto has_header = [&](const char* name) {
    return content::EvalJs(
        extension_frame,
        content::JsReplace(
            "chrome.mimeHandler.getStreamInfo().then(info =>"
            "  Object.keys(info.responseHeaders)"
            "    .some(k => k.toLowerCase() === $1.toLowerCase()))",
            name));
  };

  // Every CORS-safelisted response header the server sent must stay visible.
  // These positive checks are also the vacuity guard: were responseHeaders
  // ever empty or getStreamInfo() to fail, they would fail here instead of
  // letting the X-Auth-Token check below pass against an empty object.
  EXPECT_EQ(true, has_header("Content-Type"));
  EXPECT_EQ(true, has_header("Cache-Control"));
  EXPECT_EQ(true, has_header("Last-Modified"));

  // The non-safelisted header must be stripped.
  EXPECT_EQ(false, has_header("X-Auth-Token"));
}

// A PDF served with a strict Content-Security-Policy must still render at
// full size. The response's CSP applies to the synthetic embedder document;
// if its inline styles are dropped, the handler iframe collapses to the
// replaced-element default 300x150 instead of filling the page.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       CSPDoesNotBlockEmbedderStyles) {
  // The set-header-with-file default handler serves the shared test PDF with
  // the strict CSP attached.
  const GURL csp_pdf_url(embedded_test_server()->GetURL(
      std::string("/set-header-with-file/chrome/test/data/pdf/test.pdf"
                  "?Content-Security-Policy: ") +
      kStrictCspHeader));
  content::RenderFrameHost* extension_frame =
      LoadHandlerAndGetExtensionFrame(csp_pdf_url);
  ASSERT_TRUE(extension_frame);

  // Cross-process frame geometry propagates asynchronously from the parent
  // renderer's layout; wait for it before comparing bounds.
  content::WaitForHitTestData(extension_frame);

  content::WebContents* web_contents = GetActiveWebContents();
  const gfx::Rect embedder_rect =
      web_contents->GetPrimaryMainFrame()->GetView()->GetViewBounds();
  ASSERT_FALSE(embedder_rect.IsEmpty());
  EXPECT_EQ(embedder_rect, extension_frame->GetView()->GetViewBounds());

  // Vacuity guard: the site CSP must actually be enforced on the embedder
  // document. `default-src 'none'` makes `connect-src` fall back to 'none',
  // so a fetch() from the embedder must be blocked.
  EXPECT_EQ(false,
            content::EvalJs(
                web_contents->GetPrimaryMainFrame(),
                content::JsReplace("fetch($1).then(() => true, () => false)",
                                   csp_pdf_url)));
}

// A blob: PDF's synthetic embedder document inherits its CSP from the
// creator document rather than from response headers. The inherited
// style-src 'none' must not break the embedder's inline styles either.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       InheritedCSPDoesNotBlockEmbedderStyles) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                              "/blob_pdf_iframe_csp.html")));

  // Open the blob PDF in an iframe and wait for both the navigation and the
  // handler's chrome.test.succeed().
  ResultCatcher catcher;
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(),
                              "openBlobPdfInIframe()"));
  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The synthetic embedder document committed in the iframe sets margin:0
  // inline; without the exemption the inherited CSP drops it and the body
  // regains the default 8px margin.
  content::RenderFrameHost* embedder_host =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(embedder_host);
  EXPECT_EQ("0px",
            content::EvalJs(
                embedder_host,
                "getComputedStyle(document.body).getPropertyValue('margin')"));
}

// Verifies that a generic MIME handler with `can_embed: false` is selected
// for top-level navigations but bypassed for embedded loads.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       EmbeddedLoadHonorsCanEmbed) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("generic_mime_handler_no_embed"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(extension->id(),
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));
  EXPECT_NE(extension->id(),
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/true));
}

// With the handler disabled via options, the navigation-path lookup
// (PluginUtils::GetExtensionIdForMimeType) must not return the
// extension id, and the resulting page must not contain an extension
// frame owned by this handler.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisabledHandlerFallsBackOnNavigation) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  ASSERT_TRUE(extension);

  // Before disable: extension is the active handler along both paths.
  auto* registry = MimeHandlerRegistry::Get(profile());
  ASSERT_TRUE(registry);
  ASSERT_EQ(extension->id(), registry->GetHandlerForMimeType(kPdfMimeType));
  ASSERT_EQ(extension->id(),
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));

  // Disable for the PDF MIME type.
  registry->SetEnabledForMimeType(extension->id(), kPdfMimeType, false);

  // Both lookup paths now skip this handler and fall through to the
  // built-in PDF extension, which is always registered for
  // application/pdf in this browsertest profile.
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            registry->GetHandlerForMimeType(kPdfMimeType));
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));

  // Navigate. The extension must NOT own any frame in the resulting page.
  const GURL pdf_url = embedded_test_server()->GetURL("/test.pdf");
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, pdf_url));

  // Walk the frame tree. No frame should be owned by the extension.
  const std::string extension_host = extension->id();
  bool found_extension_frame = false;
  web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    const GURL& url = rfh->GetLastCommittedURL();
    if (url.SchemeIs(kExtensionScheme) && url.host() == extension_host) {
      found_extension_frame = true;
    }
  });
  EXPECT_FALSE(found_extension_frame);
}

// Full JS-to-navigation end-to-end: the test extension's handler.js
// calls chrome.mimeHandler.setMimeHandlerOptions({enabled:false}), then
// a subsequent navigation confirms the pref took effect through the
// navigation path.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisableViaApiFallsBackOnNextNavigation) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  ASSERT_TRUE(extension);

  auto* registry = MimeHandlerRegistry::Get(profile());
  ASSERT_TRUE(registry);
  ASSERT_EQ(extension->id(), registry->GetHandlerForMimeType(kPdfMimeType));

  // First navigation: /test.pdf?action=disable. handler.js branches
  // to calling chrome.mimeHandler.setMimeHandlerOptions and then
  // chrome.test.succeed(). Wait for ResultCatcher to observe success.
  ResultCatcher catcher;
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                              "/test.pdf?action=disable")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // JS setMimeHandlerOptions has persisted the pref. Registry now
  // reports the handler disabled for the PDF MIME type.
  EXPECT_FALSE(registry->IsEnabledForMimeType(extension->id(), kPdfMimeType));

  // Both lookup paths now skip this handler and fall through to the
  // built-in PDF extension, which is always registered for
  // application/pdf in this browsertest profile.
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            registry->GetHandlerForMimeType(kPdfMimeType));
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));

  // Second navigation: plain /test.pdf. The extension must NOT own any
  // frame (built-in PDF or native fallback handles the response).
  ASSERT_TRUE(
      NavigateToURL(web_contents, embedded_test_server()->GetURL("/test.pdf")));

  // Walk only the active (primary) frame tree. The first navigation's
  // document may still be retained in the BackForwardCache and would be
  // visited by WebContents::ForEachRenderFrameHost.
  const std::string extension_host = extension->id();
  bool found_extension_frame = false;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        const GURL& url = rfh->GetLastCommittedURL();
        if (url.SchemeIs(kExtensionScheme) && url.host() == extension_host) {
          found_extension_frame = true;
        }
      });
  EXPECT_FALSE(found_extension_frame);
}

// A generic MIME handler renders a top-level PDF as an embedded
// cross-origin OOPIF. Disabling the extension closes its tab.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisablingTopLevelHandlerClosesTab) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir));
  ASSERT_TRUE(extension);

  // Tab 0 hosts the PDF viewer. Wait for handler.js to call
  // chrome.test.succeed() so the top-level StreamInfo is claimed.
  ResultCatcher catcher;
  content::WebContents* pdf_web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(pdf_web_contents,
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  auto* manager =
      mime_handler::MimeHandlerStreamManager::FromWebContents(pdf_web_contents);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->GetTopLevelHandlerExtensionId());
  ASSERT_EQ(extension->id(), *manager->GetTopLevelHandlerExtensionId());

  // Open a second tab so the viewer tab is not the last tab, which would
  // be NTP-replaced (reusing its WebContents) rather than closed.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  content::WebContentsDestroyedWatcher destroyed_watcher(pdf_web_contents);
  DisableExtension(extension->id());
  destroyed_watcher.Wait();
  EXPECT_TRUE(destroyed_watcher.IsDestroyed());
}

// Uninstalling the handler while the viewer tab is the browser's only tab
// replaces that tab with the NTP instead of closing it, without crashing.
// The single-tab case is distinct: the viewer's WebContents is reused for
// the replacement navigation rather than destroyed. Regression test for
// crbug.com/542646771.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       UninstallingTopLevelHandlerLastTabShowsNtp) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir));
  ASSERT_TRUE(extension);

  ResultCatcher catcher;
  content::WebContents* pdf_web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(pdf_web_contents,
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  content::TestNavigationObserver ntp_observer(pdf_web_contents);
  UninstallExtension(extension->id());
  ntp_observer.Wait();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mime_handler::MimeHandlerStreamManager::FromWebContents(
      pdf_web_contents));
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(),
            pdf_web_contents->GetLastCommittedURL());
}

// Disabling the handler while the viewer tab has a beforeunload handler
// shows the confirmation dialog instead of crashing, and accepting the
// dialog closes the tab. Regression test for crbug.com/532002220.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisablingTopLevelHandlerWithBeforeUnloadShowsDialog) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir));
  ASSERT_TRUE(extension);

  ResultCatcher catcher;
  content::WebContents* pdf_web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(pdf_web_contents,
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Register a beforeunload handler on the embedder main frame: its web
  // process is unaffected by the extension unload, so the tab close is
  // guaranteed to reach the handler and prompt.
  ASSERT_TRUE(content::ExecJs(
      pdf_web_contents->GetPrimaryMainFrame(),
      "window.addEventListener('beforeunload', e => e.preventDefault());"));
  content::PrepContentsForBeforeUnloadTest(pdf_web_contents);

  // Open a second tab so the viewer tab is not the last tab, which would
  // be NTP-replaced (reusing its WebContents) rather than closed.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  content::WebContentsDestroyedWatcher destroyed_watcher(pdf_web_contents);
  DisableExtension(extension->id());

  // The stream must not outlive the extension, even while the tab close is
  // blocked on the beforeunload prompt.
  EXPECT_FALSE(mime_handler::MimeHandlerStreamManager::FromWebContents(
      pdf_web_contents));

  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  dialog->view()->AcceptAppModalDialog();
  destroyed_watcher.Wait();
  EXPECT_TRUE(destroyed_watcher.IsDestroyed());
}

// Rejecting the beforeunload dialog keeps the viewer tab open: the stream is
// already gone by then, so the tab survives without the handler and can still
// be closed normally afterwards.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisablingTopLevelHandlerWithBeforeUnloadCancelKeepsTab) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir));
  ASSERT_TRUE(extension);

  ResultCatcher catcher;
  content::WebContents* pdf_web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(pdf_web_contents,
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Register a beforeunload handler on the embedder main frame: its web
  // process is unaffected by the extension unload, so the tab close is
  // guaranteed to reach the handler and prompt.
  ASSERT_TRUE(content::ExecJs(
      pdf_web_contents->GetPrimaryMainFrame(),
      "window.addEventListener('beforeunload', e => e.preventDefault());"));
  content::PrepContentsForBeforeUnloadTest(pdf_web_contents);

  // Open a second tab so the viewer tab is not the last tab, which would
  // be NTP-replaced (reusing its WebContents) rather than closed.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  content::WebContentsDestroyedWatcher destroyed_watcher(pdf_web_contents);
  DisableExtension(extension->id());

  // The stream must not outlive the extension, even though the tab ends
  // up staying open.
  EXPECT_FALSE(mime_handler::MimeHandlerStreamManager::FromWebContents(
      pdf_web_contents));

  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  dialog->view()->CancelAppModalDialog();

  // Round-trip through the embedder renderer; this pumps the UI loop, so
  // an erroneous pending tab close would complete before the checks below.
  ASSERT_TRUE(content::ExecJs(pdf_web_contents->GetPrimaryMainFrame(), "true"));
  EXPECT_FALSE(destroyed_watcher.IsDestroyed());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // The surviving tab still closes normally: the beforeunload prompt runs
  // again and accepting it completes the close.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
  dialog = ui_test_utils::WaitForAppModalDialog();
  dialog->view()->AcceptAppModalDialog();
  destroyed_watcher.Wait();
  EXPECT_TRUE(destroyed_watcher.IsDestroyed());
}

// Verifies that every permissions policy feature is enabled on the
// MIME handler extension's outermost frame, restoring the
// top-level-frame parity these extensions had before being embedded
// as cross-origin iframes.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       AllPermissionsPolicyFeaturesEnabledInExtensionFrame) {
  content::RenderFrameHost* extension_frame = LoadHandlerAndGetExtensionFrame();
  ASSERT_TRUE(extension_frame);

  // EXPECT (not ASSERT) so a single missing feature surfaces all gaps.
  for (const auto& [feature, _] : network::GetPermissionsPolicyFeatureList(
           extension_frame->GetLastCommittedOrigin())) {
    SCOPED_TRACE(testing::Message()
                 << "feature index: " << static_cast<int>(feature));
    EXPECT_TRUE(extension_frame->IsFeatureEnabled(feature));
  }
}

// Verifies that the MIME handler extension frame can delegate
// permissions policy features to a cross-origin child iframe via the
// `allow` attribute.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       FeatureDelegatedToCrossOriginChildFrame) {
  content::RenderFrameHost* extension_frame = LoadHandlerAndGetExtensionFrame();
  ASSERT_TRUE(extension_frame);

  // `local-fonts` is EnableForSelf-by-default; without delegation a
  // cross-origin child would not receive it.
  GURL child_url = embedded_test_server()->GetURL("cdn.example", "/echo");
  content::TestNavigationObserver iframe_observer(GetActiveWebContents());
  ASSERT_TRUE(content::ExecJs(
      extension_frame,
      content::JsReplace("const f = document.createElement('iframe');"
                         "f.allow = 'local-fonts';"
                         "f.src = $1;"
                         "document.body.appendChild(f);",
                         child_url)));
  iframe_observer.Wait();

  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(extension_frame, 0);
  ASSERT_TRUE(child_frame);
  EXPECT_TRUE(child_frame->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kLocalFonts));
}

// Verifies that a user permission granted to the embedder origin does
// NOT leak to the MIME handler extension frame. The override opens the
// permissions-policy gate for every feature on the extension frame, but
// permissions-policy is only a gate -- the actual permission check is
// keyed on the calling document's origin. The extension frame's origin
// remains `chrome-extension://<ID>/`, so a grant scoped to the embedder
// origin must read as `prompt` (i.e., not granted) when queried from
// the extension.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       EmbedderPermissionGrantDoesNotLeakToExtensionFrame) {
  // Grant geolocation to the embedder origin before navigation.
  // Geolocation is `EnableForSelf` in permissions-policy AND gated by a
  // per-origin user grant, which makes it the right discriminator: if
  // the override accidentally laundered permissions across origins, the
  // extension frame would read `granted` here.
  GURL embedder_url = embedded_test_server()->GetURL("/test.pdf");
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(embedder_url, embedder_url,
                                      ContentSettingsType::GEOLOCATION,
                                      CONTENT_SETTING_ALLOW);

  content::RenderFrameHost* extension_frame = LoadHandlerAndGetExtensionFrame();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* embedder_frame = extension_frame->GetParent();
  ASSERT_TRUE(embedder_frame);

  // Sanity: the permissions-policy gate is open in the extension frame.
  // If this fails, a `prompt` result below would be a false positive --
  // the gate, not the grant, would be blocking access.
  ASSERT_TRUE(extension_frame->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kGeolocation));

  constexpr char kQueryGeolocation[] =
      "navigator.permissions.query({name: 'geolocation'})"
      "    .then(status => status.state)";

  // The embedder grant resolves against the embedder origin.
  EXPECT_EQ("granted", content::EvalJs(embedder_frame, kQueryGeolocation));

  // From the extension frame the grant must not appear. A regression to
  // `granted` would mean the override is laundering host-page
  // permissions to the extension origin.
  EXPECT_EQ("prompt", content::EvalJs(extension_frame, kQueryGeolocation));
}

}  // namespace extensions
