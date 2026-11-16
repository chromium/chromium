// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Returns true if |window.scriptExecuted| is true for the given frame.
bool WasFrameWithScriptLoaded(content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return false;
  }
  return content::EvalJs(render_frame_host, "!!window.scriptExecuted")
      .ExtractBool();
}

class ExtensionCSPBypassTest : public ExtensionBrowserTest {
 public:
  ExtensionCSPBypassTest() = default;

  ExtensionCSPBypassTest(const ExtensionCSPBypassTest&) = delete;
  ExtensionCSPBypassTest& operator=(const ExtensionCSPBypassTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("same-origin.com", "127.0.0.1");
    host_resolver()->AddRule("cross-origin.com", "127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  const Extension* AddExtension(bool is_component, bool all_urls_permission) {
    TestExtensionDir dir;

    std::string unique_name = base::StringPrintf(
        "component=%d, all_urls=%d", is_component, all_urls_permission);
    auto manifest =
        base::DictValue()
            .Set("name", unique_name)
            .Set("version", "1")
            .Set("manifest_version", 3)
            .Set("web_accessible_resources",
                 base::ListValue().Append(
                     base::DictValue()
                         .Set("resources", base::ListValue().Append("*"))
                         .Set("matches", base::ListValue().Append("*://*/*"))));

    if (all_urls_permission) {
      manifest.Set("host_permissions", base::ListValue().Append("<all_urls>"));
    }
    if (is_component) {
      // LoadExtensionAsComponent requires the manifest to contain a key.
      std::string key;
      EXPECT_TRUE(Extension::ProducePEM(unique_name, &key));
      manifest.Set("key", key);
    }

    dir.WriteFile(FILE_PATH_LITERAL("script.js"), "");
    dir.WriteManifest(manifest);

    const Extension* extension = nullptr;
    if (is_component) {
      extension = LoadExtensionAsComponent(dir.UnpackedPath());
    } else {
      extension = LoadExtension(dir.UnpackedPath());
    }
    CHECK(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  bool CanLoadScript(const Extension* extension) {
    content::RenderFrameHost* render_frame_host =
        GetActiveWebContents()->GetPrimaryMainFrame();
    std::string code = base::StringPrintf(
        R"(
        function canLoadScript() {
          const s = document.createElement('script');
          try {
            s.src = '%s';
            document.body.appendChild(s);
          } catch(e) {
            // Blocked by TrustedTypes CSP.
            return false;
          }

          // Not blocked by CSP.
          return true;
        }
        canLoadScript();
        )",
        extension->GetResourceURL("script.js").spec().c_str());
    return EvalJs(render_frame_host, code).ExtractBool();
  }

  content::RenderFrameHost* GetFrameByName(const std::string& name) {
    return content::FrameMatchingPredicate(
        GetActiveWebContents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, name));
  }

 private:
  std::vector<TestExtensionDir> temp_dirs_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, LoadWebAccessibleScript) {
  const Extension* component_ext_with_permission = AddExtension(true, true);
  const Extension* component_ext_without_permission = AddExtension(true, false);
  const Extension* ext_with_permission = AddExtension(false, true);
  const Extension* ext_without_permission = AddExtension(false, false);

  // chrome-extension:-URLs can always bypass CSP in normal pages.
  GURL non_webui_url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), non_webui_url));

  EXPECT_TRUE(CanLoadScript(component_ext_with_permission));
  EXPECT_TRUE(CanLoadScript(component_ext_without_permission));
  EXPECT_TRUE(CanLoadScript(ext_with_permission));
  EXPECT_TRUE(CanLoadScript(ext_without_permission));

  // chrome-extension:-URLs can never bypass CSP in WebUI.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL(chrome::kChromeUIExtensionsURL)));

  EXPECT_FALSE(CanLoadScript(component_ext_with_permission));
  EXPECT_FALSE(CanLoadScript(component_ext_without_permission));
  EXPECT_FALSE(CanLoadScript(ext_with_permission));
  EXPECT_FALSE(CanLoadScript(ext_without_permission));
}

// Tests that an extension can add a cross-origin iframe to a page
// whose CSP disallows iframes. Regression test for https://crbug.com/41129074.
IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, InjectIframe) {
  // Install an extension that can add a cross-origin iframe to a document.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("csp/add_iframe_extension"));
  ASSERT_TRUE(extension);

  // Navigate to a page that has CSP with 'frame-src: none' to block any
  // iframes. Use the "same-origin.com" hostname as the test will add iframes to
  // "cross-origin.com" to make clear they are cross-origin.
  GURL test_url = embedded_test_server()->GetURL(
      "same-origin.com", "/extensions/csp/page_with_frame_csp.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), test_url));

  // First, verify that adding an iframe to the page from the main world will
  // fail. Add the frame. Its onload event fires even if it's blocked
  // (see https://crbug.com/40361841), and reports back.
  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "addIframe();"));

  // Use WasFrameWithScriptLoaded() to check whether the target frame really
  // loaded.
  content::RenderFrameHost* frame = GetFrameByName("added-by-page");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasFrameWithScriptLoaded(frame));

  // Second, verify that adding an iframe to the page from the extension will
  // succeed. Click a button whose event handler runs in the extension's world
  // which bypasses CSP, and adds the iframe.
  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.querySelector('#addIframeButton').click();"));
  std::string ack;
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);
  frame = GetFrameByName("added-by-extension");
  ASSERT_TRUE(frame);
  EXPECT_TRUE(WasFrameWithScriptLoaded(frame));
}

// CSP:frame-ancestor is not bypassed by extensions.
IN_PROC_BROWSER_TEST_F(ExtensionCSPBypassTest, FrameAncestors) {
  std::string manifest = R"(
    {
      "name": "CSP frame-ancestors",
      "manifest_version": 3,
      "version": "0.1",
      "action": {
       "default_popup": "popup.html"
      }
    }
  )";

  std::string popup = R"(
    <!doctype html>
    <html>
      <iframe src = "$1"></iframe>
    </html>
  )";

  GURL iframe_url = embedded_test_server()->GetURL(
      "/extensions/csp/frame-ancestors-none.html");
  popup = base::ReplaceStringPlaceholders(popup, {iframe_url.spec()}, nullptr);

  TestExtensionDir test_dir;
  test_dir.WriteManifest(manifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), popup);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("Framing '*' violates the following*");

  GURL popup_url = extension->GetResourceURL("popup.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), popup_url));

  // The iframe must be blocked because of CSP.
  ASSERT_TRUE(console_observer.Wait());
  content::RenderFrameHost* main_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_EQ(popup_url, main_frame->GetLastCommittedURL());
  EXPECT_EQ(iframe_url, child_frame->GetLastCommittedURL());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().opaque());
}

// Summary of `ExtensionIframeAntiFramingBrowserTest` scenarios below.
// +----------------------------------+-----------------------+----------------+
// | Embedding Scenario               | Target HTTP Header(s) | Expected       |
// | (Extension Host Permissions)     |                       | Outcome        |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com                | X-Frame-Options: DENY | a.com embed    |
// | (Ext has a.com host permissions) |                       | blocked        |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com                | X-Frame-Options: DENY | a.com embed    |
// | (Ext has no host permissions)    |                       | blocked        |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com > a.com/sub    | X-Frame-Options:      | a.com embed    |
// | (Ext has a.com host permissions) | SAMEORIGIN            | commits,       |
// |                                  |                       | a.com/sub      |
// |                                  |                       | embed blocked  |
// +----------------------------------+-----------------------+----------------+
// | example.com > Extension > a.com  | X-Frame-Options: DENY | Extension      |
// | (Ext has a.com host permissions) |                       | embed commits, |
// |                                  |                       | a.com embed    |
// |                                  |                       | blocked        |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com                | Content-Security-     | a.com embed    |
// | (Ext has a.com host permissions) | Policy:               | blocked        |
// |                                  | frame-ancestors       |                |
// |                                  | 'none' +              |                |
// |                                  | X-Frame-Options:      |                |
// |                                  | ALLOWALL              |                |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com                | Content-Security-     | a.com embed    |
// | (Ext has a.com host permissions) | Policy:               | blocked        |
// |                                  | frame-ancestors       |                |
// |                                  | 'self'                |                |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com > b.com        | Content-Security-     | a.com embed    |
// | (Ext has a.com host permissions) | Policy:               | commits,       |
// |                                  | frame-ancestors       | b.com embed    |
// |                                  | https://a.com         | blocked        |
// +----------------------------------+-----------------------+----------------+
// | Extension > a.com > b.com        | Content-Security-     | a.com embed    |
// | (Ext has <all_urls> host         | Policy:               | commits,       |
// | permissions)                     | frame-ancestors       | b.com embed    |
// |                                  | 'self'                | blocked        |
// +----------------------------------+-----------------------+----------------+
// | Extension A > Extension B >      | Content-Security-     | Extension B    |
// | a.com                            | Policy:               | embed commits, |
// | (Ext A has no host permissions,  | frame-ancestors       | a.com embed    |
// | Ext B has a.com host             | chrome-extension://   | blocked        |
// | permissions)                     | <ext_b_id>            |                |
// +----------------------------------+-----------------------+----------------+
// | Extension A > Extension B >      | Content-Security-     | Extension B    |
// | a.com                            | Policy:               | embed commits, |
// | (Ext A has no host permissions,  | frame-ancestors       | a.com embed    |
// | Ext B has a.com host             | chrome-extension://   | blocked        |
// | permissions)                     | <ext_a_id>            |                |
// +----------------------------------+-----------------------+----------------+
// | Extension > evil.com >           | Content-Security-     | evil.com and   |
// | Extension > good.com             | Policy:               | Extension      |
// | (Ext has <all_urls> host         | frame-ancestors       | embeds commit, |
// | permissions)                     | chrome-extension://   | good.com embed |
// |                                  | <ext_id>              | blocked        |
// |                                  | https://good.com      |                |
// +----------------------------------+-----------------------+----------------+
//
// Test fixture for verifying how HTTP anti-framing headers (`X-Frame-Options`
// and `Content-Security-Policy: frame-ancestors`) interact with top-level and
// nested `chrome-extension:` frames, `host_permissions`, and
// `web_accessible_resources`.
class ExtensionIframeAntiFramingBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionIframeAntiFramingBrowserTest() = default;
  ExtensionIframeAntiFramingBrowserTest(
      const ExtensionIframeAntiFramingBrowserTest&) = delete;
  ExtensionIframeAntiFramingBrowserTest& operator=(
      const ExtensionIframeAntiFramingBrowserTest&) = delete;
  ~ExtensionIframeAntiFramingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule(/*host_pattern=*/"*", /*ip_literal=*/"127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();

    UseHttpsTestServer();
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {"a.com", "b.com", "example.com", "evil.com",
                             "good.com"};
    embedded_test_server()->SetSSLConfig(cert_config);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ExtensionIframeAntiFramingBrowserTest::HandleAntiFramingRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  // HTTP anti-framing headers attached to a test server response.
  struct AntiFramingHeaders {
    std::string_view xfo;
    std::string_view csp;
  };

  // Configuration for creating and loading an unpacked test extension with an
  // extension_page.html frame document.
  struct ExtensionConfig {
    // Optional JSON string entry for the manifest `"host_permissions"` list
    // (for example, `R"("https://a.com/*")"`).
    std::string_view host_permissions_entry;
    // When true, lists extension_page.html in the manifest
    // `"web_accessible_resources"` so cross-origin web pages and other
    // extensions are allowed to embed extension_page.html in an `<iframe>`.
    bool expose_page_in_web_accessible_resources = false;
  };

  // Outcome of dynamically appending and navigating a child `<iframe>`.
  struct IframeNavigationResult {
    bool succeeded = false;
    net::Error net_error = net::ERR_UNEXPECTED;
    raw_ptr<content::RenderFrameHost> frame = nullptr;
  };

  // Returns the serialized HTTPS origin (including ephemeral port) for `host`
  // on `embedded_test_server()`, suitable for `frame-ancestors` source lists.
  std::string GetHttpsOrigin(std::string_view host) const {
    return embedded_test_server()->GetOrigin(std::string(host)).Serialize();
  }

  // Builds an HTTPS URL on `host` and `path` served without any anti-framing
  // HTTP headers.
  GURL GetUrlWithoutAntiFramingHeaders(
      std::string_view host,
      std::string_view path = "/anti_framing/page.html") const {
    return embedded_test_server()->GetURL(host, path);
  }

  // Builds an HTTPS URL on `host` and `path` served with the specified
  // `X-Frame-Options` and/or `Content-Security-Policy` HTTP headers.
  GURL GetAntiFramingUrl(
      std::string_view host,
      AntiFramingHeaders headers,
      std::string_view path = "/anti_framing/page.html") const {
    CHECK(!headers.xfo.empty() || !headers.csp.empty());
    GURL url = embedded_test_server()->GetURL(host, path);
    if (!headers.xfo.empty()) {
      url = net::AppendQueryParameter(url, /*name=*/"xfo", headers.xfo);
    }
    if (!headers.csp.empty()) {
      url = net::AppendQueryParameter(url, /*name=*/"csp", headers.csp);
    }
    return url;
  }

  // Creates and loads an unpacked extension configured according to `config`,
  // retaining its `TestExtensionDir` for the lifetime of the test.
  const Extension* LoadFramingExtension(const ExtensionConfig& config) {
    static constexpr char kManifestTemplate[] = R"({
      "name": "Framing Test Extension",
      "version": "1.0",
      "manifest_version": 3,
      "host_permissions": [%s],
      "web_accessible_resources": [%s]
    })";
    static constexpr char kExtensionPageWebAccessibleResourcesEntry[] = R"({
      "resources": ["extension_page.html"],
      "matches": ["<all_urls>"],
      "extension_ids": ["*"]
    })";
    static constexpr char kFrameHtml[] =
        "<!doctype html><html><body></body></html>";

    TestExtensionDir dir;
    dir.WriteManifest(
        base::StringPrintf(kManifestTemplate, config.host_permissions_entry,
                           config.expose_page_in_web_accessible_resources
                               ? kExtensionPageWebAccessibleResourcesEntry
                               : ""));
    dir.WriteFile(FILE_PATH_LITERAL("extension_page.html"), kFrameHtml);

    const Extension* extension = LoadExtension(dir.UnpackedPath());
    extension_dirs_.push_back(std::move(dir));
    return extension;
  }

  // Navigates the active tab's primary main frame to `url` and returns the
  // resulting `content::RenderFrameHost`, or `nullptr` if navigation fails.
  content::RenderFrameHost* NavigateMainFrame(const GURL& url) {
    content::WebContents* web_contents = GetActiveWebContents();
    CHECK(web_contents);
    bool navigated = false;
    {
      SCOPED_TRACE("Waiting for primary main frame navigation");
      navigated = NavigateToURL(web_contents, url);
    }
    if (!navigated) {
      return nullptr;
    }
    return web_contents->GetPrimaryMainFrame();
  }

  // Appends a child `<iframe>` navigating to `url` inside `parent_frame`, waits
  // for the navigation to finish loading, and returns its navigation outcome.
  IframeNavigationResult EmbedIframe(content::RenderFrameHost* parent_frame,
                                     const GURL& url) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(parent_frame);
    CHECK(web_contents);
    const size_t child_index = CountChildFrames(parent_frame);

    content::TestNavigationObserver nav_observer(web_contents);
    nav_observer.set_expected_initial_url(url);
    const std::string script = content::JsReplace(
        R"(
          const iframe = document.createElement('iframe');
          iframe.src = $1;
          document.body.appendChild(iframe);
        )",
        url);
    CHECK(content::ExecJs(parent_frame, script));
    {
      SCOPED_TRACE("Waiting for child iframe navigation to finish");
      nav_observer.Wait();
    }

    content::RenderFrameHost* child_frame =
        content::ChildFrameAt(parent_frame, child_index);
    CHECK(child_frame);
    return {
        .succeeded = nav_observer.last_navigation_succeeded(),
        .net_error = nav_observer.last_net_error_code(),
        .frame = child_frame,
    };
  }

  // Asserts that an intermediate `<iframe>` navigation succeeded cleanly.
  void AssertIframeCommitted(const IframeNavigationResult& result,
                             const GURL& expected_url) {
    ASSERT_TRUE(result.succeeded);
    ASSERT_EQ(net::OK, result.net_error);
    ASSERT_FALSE(result.frame->IsErrorDocument());
    ASSERT_EQ(url::Origin::Create(expected_url),
              result.frame->GetLastCommittedOrigin());
  }

  // Expects that a leaf `<iframe>` navigation was blocked by anti-framing
  // headers.
  void ExpectBlockedByResponse(const IframeNavigationResult& result) {
    EXPECT_FALSE(result.succeeded);
    EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, result.net_error);
    EXPECT_TRUE(result.frame->IsErrorDocument());
    EXPECT_TRUE(result.frame->GetLastCommittedOrigin().opaque());
  }

 private:
  // Serves minimal HTML documents for /anti_framing/* and /sub paths so tests
  // can build multi-frame hierarchies without static files. Echoes any "xfo"
  // and "csp" URL query parameters as `X-Frame-Options` and
  // `Content-Security-Policy` HTTP response headers, or serves no anti-framing
  // headers when omitted.
  static std::unique_ptr<net::test_server::HttpResponse>
  HandleAntiFramingRequest(const net::test_server::HttpRequest& request) {
    const GURL url = request.GetURL();
    const std::string_view path = url.path();
    if (!path.starts_with("/anti_framing/") && path != "/sub") {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content("<!doctype html><html><body>Loaded</body></html>");

    for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
      if (it.GetKey() == "xfo") {
        response->AddCustomHeader("X-Frame-Options", it.GetUnescapedValue());
      } else if (it.GetKey() == "csp") {
        response->AddCustomHeader("Content-Security-Policy",
                                  it.GetUnescapedValue());
      }
    }
    return response;
  }

  // Counts immediate child frames under `parent_frame` so newly appended
  // `<iframe>` elements can be retrieved deterministically by index.
  static size_t CountChildFrames(content::RenderFrameHost* parent_frame) {
    size_t count = 0;
    while (content::ChildFrameAt(parent_frame, count)) {
      ++count;
    }
    return count;
  }

  std::vector<TestExtensionDir> extension_dirs_;
};

// In Extension > a.com (where the extension has a.com host permissions), the
// a.com `<iframe>` embed is blocked because of a.com's `X-Frame-Options: DENY`
// HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    ExtensionWithHostPermissionEmbedsWeb_XFrameOptionsDenyBlocks) {
  const Extension* extension =
      LoadFramingExtension({.host_permissions_entry = R"("https://a.com/*")"});
  ASSERT_TRUE(extension);

  // a.com serves `X-Frame-Options: DENY` to forbid all framing.
  const GURL target_url = GetAntiFramingUrl("a.com", {.xfo = "DENY"});

  // Navigate to the extension's page and verify embedding a.com is blocked
  // even with host permissions.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  ExpectBlockedByResponse(EmbedIframe(main_frame, target_url));
}

// In Extension > a.com (where the extension has no host permissions), the
// a.com `<iframe>` embed is blocked because of a.com's `X-Frame-Options: DENY`
// HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    ExtensionWithoutHostPermissionEmbedsWeb_XFrameOptionsDenyBlocks) {
  const Extension* extension = LoadFramingExtension({});
  ASSERT_TRUE(extension);

  // a.com serves `X-Frame-Options: DENY` to forbid all framing.
  const GURL target_url = GetAntiFramingUrl("a.com", {.xfo = "DENY"});

  // Navigate to the extension's page and verify embedding a.com is blocked.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  ExpectBlockedByResponse(EmbedIframe(main_frame, target_url));
}

// In Extension > a.com > a.com/sub (where the extension has a.com host
// permissions), the a.com/sub `<iframe>` embed is blocked because of
// a.com/sub's `X-Frame-Options: SAMEORIGIN` HTTP header.
IN_PROC_BROWSER_TEST_F(ExtensionIframeAntiFramingBrowserTest,
                       NestedWebEmbedding_XFrameOptionsSameOriginBlocks) {
  const Extension* extension =
      LoadFramingExtension({.host_permissions_entry = R"("https://a.com/*")"});
  ASSERT_TRUE(extension);

  // a.com serves no anti-framing headers, while a.com/sub serves
  // `X-Frame-Options: SAMEORIGIN` (allowing only same-origin ancestors).
  const GURL a_url = GetUrlWithoutAntiFramingHeaders("a.com");
  const GURL a_sub_url =
      GetAntiFramingUrl("a.com", {.xfo = "SAMEORIGIN"}, /*path=*/"/sub");

  // Navigate to the extension's page and verify the intermediate a.com embed
  // succeeds.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult a_result = EmbedIframe(main_frame, a_url);
  ASSERT_NO_FATAL_FAILURE(AssertIframeCommitted(a_result, a_url));

  // Embed a.com/sub inside a.com. Even though immediate parent a.com is
  // same-origin, the top-level ancestor extension is cross-origin, so
  // a.com/sub is blocked.
  ExpectBlockedByResponse(EmbedIframe(a_result.frame, a_sub_url));
}

// In example.com > Extension > a.com (where the extension has a.com host
// permissions), the a.com `<iframe>` embed is blocked because of a.com's
// `X-Frame-Options: DENY` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    WebEmbedsExtensionWithHostPermission_XFrameOptionsDenyBlocks) {
  const Extension* extension = LoadFramingExtension({
      .host_permissions_entry = R"("https://a.com/*")",
      // Expose extension_page.html so example.com can embed the extension page.
      .expose_page_in_web_accessible_resources = true,
  });
  ASSERT_TRUE(extension);

  // example.com serves no anti-framing headers, while a.com serves
  // `X-Frame-Options: DENY` to forbid all framing.
  const GURL top_web_url = GetUrlWithoutAntiFramingHeaders("example.com");
  const GURL ext_page_url = extension->GetResourceURL("extension_page.html");
  const GURL target_url = GetAntiFramingUrl("a.com", {.xfo = "DENY"});

  // Navigate to example.com and verify the intermediate extension_page.html
  // embed succeeds.
  content::RenderFrameHost* main_frame = NavigateMainFrame(top_web_url);
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult ext_frame_result =
      EmbedIframe(main_frame, ext_page_url);
  ASSERT_NO_FATAL_FAILURE(
      AssertIframeCommitted(ext_frame_result, ext_page_url));

  // Embed a.com inside the extension's child frame and verify
  // `X-Frame-Options: DENY` blocks the navigation.
  ExpectBlockedByResponse(EmbedIframe(ext_frame_result.frame, target_url));
}

// In Extension > a.com (where the extension has a.com host permissions), the
// a.com `<iframe>` embed is blocked because a.com's
// `Content-Security-Policy: frame-ancestors 'none'` HTTP header overrides its
// `X-Frame-Options: ALLOWALL` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    ExtensionWithHostPermissionEmbedsWeb_FrameAncestorsNoneOverridesAllowAllBlocks) {
  const Extension* extension =
      LoadFramingExtension({.host_permissions_entry = R"("https://a.com/*")"});
  ASSERT_TRUE(extension);

  // a.com serves both `X-Frame-Options: ALLOWALL` (allowing framing) and
  // `Content-Security-Policy: frame-ancestors 'none'` (forbidding all framing,
  // which takes precedence over `X-Frame-Options`).
  const GURL target_url = GetAntiFramingUrl(
      "a.com", {.xfo = "ALLOWALL", .csp = "frame-ancestors 'none'"});

  // Navigate to the extension's page and verify embedding a.com is blocked.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  ExpectBlockedByResponse(EmbedIframe(main_frame, target_url));
}

// In Extension > a.com (where the extension has a.com host permissions), the
// a.com `<iframe>` embed is blocked because of a.com's
// `Content-Security-Policy: frame-ancestors 'self'` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    ExtensionWithHostPermissionEmbedsWeb_FrameAncestorsSelfBlocks) {
  const Extension* extension =
      LoadFramingExtension({.host_permissions_entry = R"("https://a.com/*")"});
  ASSERT_TRUE(extension);

  // a.com serves `Content-Security-Policy: frame-ancestors 'self'` to allow
  // only same-origin ancestors.
  const GURL target_url =
      GetAntiFramingUrl("a.com", {.csp = "frame-ancestors 'self'"});

  // Navigate to the extension's page and verify the cross-origin extension is
  // blocked from embedding a.com even with host permissions.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  ExpectBlockedByResponse(EmbedIframe(main_frame, target_url));
}

// In Extension > a.com > b.com (where the extension has a.com host
// permissions), the b.com `<iframe>` embed is blocked because of b.com's
// `Content-Security-Policy: frame-ancestors https://a.com` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    NestedWebEmbedding_FrameAncestorsOnlyAllowsParentWebBlocks) {
  const Extension* extension =
      LoadFramingExtension({.host_permissions_entry = R"("https://a.com/*")"});
  ASSERT_TRUE(extension);

  // a.com serves no anti-framing headers, while b.com serves
  // `frame-ancestors https://a.com` to allow framing only by a.com.
  const GURL a_url = GetUrlWithoutAntiFramingHeaders("a.com");
  const std::string b_csp =
      base::StrCat({"frame-ancestors ", GetHttpsOrigin("a.com")});
  const GURL b_url = GetAntiFramingUrl("b.com", {.csp = b_csp});

  // Navigate to the extension's page and verify the intermediate a.com embed
  // succeeds.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult a_result = EmbedIframe(main_frame, a_url);
  ASSERT_NO_FATAL_FAILURE(AssertIframeCommitted(a_result, a_url));

  // Embed b.com inside a.com. Even though immediate parent a.com matches
  // b.com's `frame-ancestors` header, the top-level ancestor extension does
  // not, so b.com is blocked.
  ExpectBlockedByResponse(EmbedIframe(a_result.frame, b_url));
}

// In Extension > a.com > b.com (where the extension has <all_urls> host
// permissions), the b.com `<iframe>` embed is blocked because of b.com's
// `Content-Security-Policy: frame-ancestors 'self'` HTTP header.
IN_PROC_BROWSER_TEST_F(ExtensionIframeAntiFramingBrowserTest,
                       NestedWebEmbedding_FrameAncestorsSelfBlocks) {
  const Extension* extension =
      LoadFramingExtension({.host_permissions_entry = R"("<all_urls>")"});
  ASSERT_TRUE(extension);

  // a.com serves no anti-framing headers, while b.com serves
  // `frame-ancestors 'self'` to allow only same-origin b.com ancestors.
  const GURL a_url = GetUrlWithoutAntiFramingHeaders("a.com");
  const GURL b_url =
      GetAntiFramingUrl("b.com", {.csp = "frame-ancestors 'self'"});

  // Navigate to the extension's page and verify the intermediate a.com embed
  // succeeds.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(extension->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult a_result = EmbedIframe(main_frame, a_url);
  ASSERT_NO_FATAL_FAILURE(AssertIframeCommitted(a_result, a_url));

  // Embed b.com inside a.com. Both cross-origin ancestors (a.com and the
  // extension) are disallowed by b.com's `frame-ancestors 'self'` header, so
  // b.com is blocked.
  ExpectBlockedByResponse(EmbedIframe(a_result.frame, b_url));
}

// In Extension A > Extension B > a.com (where extension A has no host
// permissions and extension B has a.com host permissions), the a.com
// `<iframe>` embed is blocked because of a.com's `Content-Security-Policy:
// frame-ancestors chrome-extension://<ext_b_id>` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    MultiExtensionEmbedding_FrameAncestorsOnlyAllowsFinalExtensionBlocks) {
  const Extension* ext_a = LoadFramingExtension({});
  ASSERT_TRUE(ext_a);
  const Extension* ext_b = LoadFramingExtension({
      .host_permissions_entry = R"("https://a.com/*")",
      // Expose extension_page.html so extension A can embed extension B's page.
      .expose_page_in_web_accessible_resources = true,
  });
  ASSERT_TRUE(ext_b);

  // a.com serves `frame-ancestors` allowing only extension B's origin.
  const GURL ext_b_page_url = ext_b->GetResourceURL("extension_page.html");
  const std::string csp =
      base::StrCat({"frame-ancestors ", ext_b->origin().Serialize()});
  const GURL target_url = GetAntiFramingUrl("a.com", {.csp = csp});

  // Navigate to extension A's page and verify embedding extension B's page
  // succeeds via `web_accessible_resources`.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(ext_a->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult ext_b_frame_result =
      EmbedIframe(main_frame, ext_b_page_url);
  ASSERT_NO_FATAL_FAILURE(
      AssertIframeCommitted(ext_b_frame_result, ext_b_page_url));

  // Embed a.com inside extension B's child frame. Even though immediate parent
  // extension B is allowed by a.com's `frame-ancestors` header, top-level
  // ancestor extension A is not, so a.com is blocked.
  ExpectBlockedByResponse(EmbedIframe(ext_b_frame_result.frame, target_url));
}

// In Extension A > Extension B > a.com (where extension A has no host
// permissions and extension B has a.com host permissions), the a.com
// `<iframe>` embed is blocked because of a.com's `Content-Security-Policy:
// frame-ancestors chrome-extension://<ext_a_id>` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    MultiExtensionEmbedding_FrameAncestorsOnlyAllowsTopExtensionBlocks) {
  const Extension* ext_a = LoadFramingExtension({});
  ASSERT_TRUE(ext_a);
  const Extension* ext_b = LoadFramingExtension({
      .host_permissions_entry = R"("https://a.com/*")",
      // Expose extension_page.html so extension A can embed extension B's page.
      .expose_page_in_web_accessible_resources = true,
  });
  ASSERT_TRUE(ext_b);

  // a.com serves `frame-ancestors` allowing only extension A's origin.
  const GURL ext_b_page_url = ext_b->GetResourceURL("extension_page.html");
  const std::string csp =
      base::StrCat({"frame-ancestors ", ext_a->origin().Serialize()});
  const GURL target_url = GetAntiFramingUrl("a.com", {.csp = csp});

  // Navigate to extension A's page and verify embedding extension B's page
  // succeeds via `web_accessible_resources`.
  content::RenderFrameHost* main_frame =
      NavigateMainFrame(ext_a->GetResourceURL("extension_page.html"));
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult ext_b_frame_result =
      EmbedIframe(main_frame, ext_b_page_url);
  ASSERT_NO_FATAL_FAILURE(
      AssertIframeCommitted(ext_b_frame_result, ext_b_page_url));

  // Embed a.com inside extension B's child frame. Even though top-level
  // ancestor extension A is allowed by a.com's `frame-ancestors` header,
  // intermediate parent extension B is not, so a.com is blocked.
  ExpectBlockedByResponse(EmbedIframe(ext_b_frame_result.frame, target_url));
}

// In Extension > evil.com > Extension > good.com (where the extension has
// <all_urls> host permissions), the good.com `<iframe>` embed is blocked
// because of good.com's `Content-Security-Policy: frame-ancestors
// chrome-extension://<ext_id> https://good.com` HTTP header.
IN_PROC_BROWSER_TEST_F(
    ExtensionIframeAntiFramingBrowserTest,
    SandwichedWebEmbedding_FrameAncestorsExcludesMiddleWebBlocks) {
  const Extension* extension = LoadFramingExtension({
      .host_permissions_entry = R"("<all_urls>")",
      // Expose extension_page.html so evil.com can embed the extension page.
      .expose_page_in_web_accessible_resources = true,
  });
  ASSERT_TRUE(extension);

  // evil.com serves no anti-framing headers, while good.com serves
  // `frame-ancestors` allowing only the extension and https://good.com.
  const GURL evil_url = GetUrlWithoutAntiFramingHeaders("evil.com");
  const GURL ext_page_url = extension->GetResourceURL("extension_page.html");
  const std::string good_csp =
      base::StrCat({"frame-ancestors ", extension->origin().Serialize(), " ",
                    GetHttpsOrigin("good.com")});
  const GURL good_url = GetAntiFramingUrl("good.com", {.csp = good_csp});

  // Navigate to the extension's page and verify both intermediate embeds
  // (evil.com and extension_page.html) succeed.
  content::RenderFrameHost* main_frame = NavigateMainFrame(ext_page_url);
  ASSERT_TRUE(main_frame);
  const IframeNavigationResult evil_result = EmbedIframe(main_frame, evil_url);
  ASSERT_NO_FATAL_FAILURE(AssertIframeCommitted(evil_result, evil_url));
  const IframeNavigationResult ext_child_result =
      EmbedIframe(evil_result.frame, ext_page_url);
  ASSERT_NO_FATAL_FAILURE(
      AssertIframeCommitted(ext_child_result, ext_page_url));

  // Embed good.com inside the extension's child frame. `frame-ancestors`
  // checks every ancestor in the frame tree, not just the immediate parent.
  // Even though both extension frames match good.com's `frame-ancestors`
  // header, intermediate ancestor evil.com does not, so good.com is blocked.
  ExpectBlockedByResponse(EmbedIframe(ext_child_result.frame, good_url));
}

}  // namespace extensions
