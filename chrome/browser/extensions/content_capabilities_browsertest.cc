// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/permissions_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/special_storage_policy.h"

using extensions::Extension;
using extensions::ExtensionBuilder;

class ContentCapabilitiesTest : public extensions::ExtensionApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        crx_file::id_util::GenerateIdForPath(
            base::MakeAbsoluteFilePath(test_extension_dir_.UnpackedPath())));
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_https_test_server().ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/content_capabilities"));
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*",
                             embedded_https_test_server().base_url().host());
  }

  // Builds an extension manifest with the given content_capabilities matches
  // and permissions. The extension always has the same (allowlisted) ID.
  scoped_refptr<const Extension> LoadExtensionWithCapabilities(
      const std::string& matches,
      const std::string& permissions,
      const std::string& extension_permissions = "[]") {
    std::string manifest = base::StringPrintf(
        "{\n"
        "  \"name\": \"content_capabilities test extensions\",\n"
        "  \"version\": \"1\",\n"
        "  \"manifest_version\": 3,\n"
        "  \"content_capabilities\": {\n"
        "    \"matches\": %s,\n"
        "    \"permissions\": %s\n"
        "  },\n"
        "  \"permissions\": %s\n"
        "}\n",
        matches.c_str(), permissions.c_str(), extension_permissions.c_str());
    test_extension_dir_.WriteManifest(manifest);
    return LoadExtension(test_extension_dir_.UnpackedPath());
  }

  std::string MakeJSONList(const std::string& s0 = "",
                           const std::string& s1 = "",
                           const std::string& s2 = "") {
    std::vector<std::string_view> v;
    if (!s0.empty())
      v.push_back(s0);
    if (!s1.empty())
      v.push_back(s1);
    if (!s2.empty())
      v.push_back(s2);
    std::string list = base::JoinString(v, "\",\"");
    if (!list.empty())
      list = "\"" + list + "\"";
    return "[" + list + "]";
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetTestURLFor(const std::string& host) {
    std::string port =
        base::NumberToString(embedded_https_test_server().port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_https_test_server()
        .GetURL("/" + host + ".html")
        .ReplaceComponents(replacements);
  }

  content::RenderFrameHost* GetRenderFrameHost() {
    return content::ToRenderFrameHost(web_contents()).render_frame_host();
  }

  void SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetRenderFrameHost()->GetBrowserContext()->GetPermissionController();
    url::Origin origin = url::Origin::Create(GetTestURLFor("foo.example.com"));
    SetPermissionControllerOverrideForDevTools(
        permission_controller, origin,
        blink::PermissionType::CLIPBOARD_READ_WRITE, status);
  }

  void SetPermissionOverrideForSanitizedWriteTests(
      blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetRenderFrameHost()->GetBrowserContext()->GetPermissionController();
    url::Origin origin = url::Origin::Create(GetTestURLFor("foo.example.com"));
    SetPermissionControllerOverrideForDevTools(
        permission_controller, origin,
        blink::PermissionType::CLIPBOARD_SANITIZED_WRITE, status);
  }

  void LoadExtensionWithCapabilitiesAndNavigateToPage(
      std::string read_write_permission) {
    scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
        MakeJSONList("https://foo.example.com/*"), read_write_permission);
    content::RenderFrameHost* rfh_tab = ui_test_utils::NavigateToURL(
        browser(), GetTestURLFor("foo.example.com"));
    content::WebContents::FromRenderFrameHost(rfh_tab)->Focus();
  }

  void CheckSiteCanRead(bool expected) {
    content::WebContents::FromRenderFrameHost(GetRenderFrameHost())->Focus();
    EXPECT_EQ(expected, content::ExecJs(web_contents(),
                                        "navigator.clipboard.readText()"));
  }

  void CheckSiteCanWrite(bool expected) {
    content::WebContents::FromRenderFrameHost(GetRenderFrameHost())->Focus();
    EXPECT_EQ(
        expected,
        content::ExecJs(web_contents(), "navigator.clipboard.writeText('Test')",
                        content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  // Run some script in the context of the given origin and in the presence of
  // the given extension. This is used to wrap calls into the JS test functions
  // defined by
  // $(DIR_TEST_DATA)/extensions/content_capabilities/capability_tests.js.
  testing::AssertionResult TestScriptResult(const Extension* extension,
                                            const GURL& url,
                                            const char* code) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    if (!content::EvalJs(web_contents(), code).ExtractBool()) {
      return testing::AssertionFailure();
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult CanReadClipboard(const Extension* extension,
                                            const GURL& url) {
    return TestScriptResult(extension, url, "tests.canReadClipboard()");
  }

  testing::AssertionResult CanWriteClipboard(const Extension* extension,
                                             const GURL& url) {
    return TestScriptResult(extension, url, "tests.canWriteClipboard()");
  }

  testing::AssertionResult CanReadClipboardInAboutBlankFrame(
      const Extension* extension,
      const GURL& url) {
    return TestScriptResult(extension, url,
                            "tests.canReadClipboardInAboutBlankFrame()");
  }

  testing::AssertionResult CanWriteClipboardInAboutBlankFrame(
      const Extension* extension,
      const GURL& url) {
    return TestScriptResult(extension, url,
                            "tests.canWriteClipboardInAboutBlankFrame()");
  }

  testing::AssertionResult HasUnlimitedStorage(const Extension* extension,
                                               const GURL& url) {
    if (profile()->GetSpecialStoragePolicy()->IsStorageUnlimited(url))
      return testing::AssertionSuccess();
    return testing::AssertionFailure();
  }

 private:
  extensions::TestExtensionDir test_extension_dir_;
};

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, NoCapabilities) {
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("https://foo.example.com/*"), MakeJSONList());
  EXPECT_FALSE(
      CanReadClipboard(extension.get(), GetTestURLFor("foo.example.com")));
  // TODO(dcheng): This should be false, but we cannot currently execute testing
  // script without a user gesture.
  EXPECT_TRUE(
      CanWriteClipboard(extension.get(), GetTestURLFor("foo.example.com")));
  EXPECT_FALSE(
      HasUnlimitedStorage(extension.get(), GetTestURLFor("foo.example.com")));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, ClipboardRead) {
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("https://foo.example.com/*"), MakeJSONList("clipboardRead"));
  EXPECT_TRUE(
      CanReadClipboard(extension.get(), GetTestURLFor("foo.example.com")));
  EXPECT_FALSE(
      CanReadClipboard(extension.get(), GetTestURLFor("bar.example.com")));
  EXPECT_TRUE(
      CanReadClipboardInAboutBlankFrame(extension.get(),
                                         GetTestURLFor("foo.example.com")));
  EXPECT_FALSE(
      CanReadClipboardInAboutBlankFrame(extension.get(),
                                         GetTestURLFor("bar.example.com")));
  // TODO(dcheng): This should be false, but we cannot currently execute testing
  // script without a user gesture.
  EXPECT_TRUE(
      CanWriteClipboard(extension.get(), GetTestURLFor("foo.example.com")));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, ClipboardWrite) {
  scoped_refptr<const Extension> extension =
      LoadExtensionWithCapabilities(MakeJSONList("https://foo.example.com/*"),
                                    MakeJSONList("clipboardWrite"));
  EXPECT_TRUE(
      CanWriteClipboard(extension.get(), GetTestURLFor("foo.example.com")));
  EXPECT_TRUE(
      CanWriteClipboardInAboutBlankFrame(extension.get(),
                                          GetTestURLFor("foo.example.com")));
  // TODO(dcheng): This should be false, but we cannot currently execute testing
  // script without a user gesture.
  EXPECT_TRUE(
      CanWriteClipboard(extension.get(), GetTestURLFor("bar.example.com")));
  if (base::FeatureList::IsEnabled(
          features::kUserActivationSameOriginVisibility)) {
    EXPECT_TRUE(CanWriteClipboardInAboutBlankFrame(
        extension.get(), GetTestURLFor("bar.example.com")));
  } else {
    // In UserActivationV2, acitvation doesn't propagate to a child frame.
    EXPECT_FALSE(CanWriteClipboardInAboutBlankFrame(
        extension.get(), GetTestURLFor("bar.example.com")));
  }

  EXPECT_FALSE(
      CanReadClipboard(extension.get(), GetTestURLFor("foo.example.com")));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, ClipboardReadWrite) {
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("https://foo.example.com/*"),
      MakeJSONList("clipboardRead", "clipboardWrite"));
  EXPECT_TRUE(
      CanReadClipboard(extension.get(), GetTestURLFor("foo.example.com")));
  EXPECT_TRUE(
      CanWriteClipboard(extension.get(), GetTestURLFor("foo.example.com")));
  EXPECT_FALSE(
      CanReadClipboard(extension.get(), GetTestURLFor("bar.example.com")));
  // TODO(dcheng): This should be false, but we cannot currently execute testing
  // script without a user gesture.
  EXPECT_TRUE(
      CanWriteClipboard(extension.get(), GetTestURLFor("bar.example.com")));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest,
                       AsyncClipboardReadWriteContentCapability) {
  LoadExtensionWithCapabilitiesAndNavigateToPage(
      "[\"clipboardRead\",\"clipboardWrite\"]");
  CheckSiteCanWrite(/*expected=*/true);
  CheckSiteCanRead(/*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest,
                       AsyncClipboardWriteContentCapability) {
  LoadExtensionWithCapabilitiesAndNavigateToPage("[\"clipboardWrite\"]");
  // Verifies that the extension capability, if any, takes precedence over the
  // permission setting.
  SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus::DENIED);
  CheckSiteCanWrite(/*expected=*/true);
  CheckSiteCanRead(/*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest,
                       AsyncClipboardReadContentCapability) {
  LoadExtensionWithCapabilitiesAndNavigateToPage("[\"clipboardRead\"]");
  SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus::DENIED);
  CheckSiteCanWrite(/*expected=*/false);
  CheckSiteCanRead(/*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest,
                       AsyncClipboardNoReadWriteContentCapability) {
  LoadExtensionWithCapabilitiesAndNavigateToPage("[]");
  SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus::GRANTED);
  CheckSiteCanWrite(/*expected=*/true);
  CheckSiteCanRead(/*expected=*/true);
  SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus::ASK);
  CheckSiteCanRead(/*expected=*/false);
  SetPermissionOverrideForSanitizedWriteTests(
      blink::mojom::PermissionStatus::ASK);
  CheckSiteCanWrite(/*expected=*/false);
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, UnlimitedStorage) {
  scoped_refptr<const Extension> extension =
      LoadExtensionWithCapabilities(MakeJSONList("https://foo.example.com/*"),
                                    MakeJSONList("unlimitedStorage"));
  EXPECT_TRUE(
      HasUnlimitedStorage(extension.get(), GetTestURLFor("foo.example.com")));
  EXPECT_FALSE(
      HasUnlimitedStorage(extension.get(), GetTestURLFor("bar.example.com")));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, WebUnlimitedStorageIsIsolated) {
  // This extension grants unlimited storage to bar.example.com but does not
  // have unlimitedStorage itself.
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("https://bar.example.com/*"),
      MakeJSONList("unlimitedStorage"), MakeJSONList("storage"));
  EXPECT_FALSE(
      HasUnlimitedStorage(extension.get(), extension->GetResourceURL("")));
  EXPECT_TRUE(
      HasUnlimitedStorage(extension.get(), GetTestURLFor("bar.example.com")));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest,
                       ExtensionUnlimitedStorageIsIsolated) {
  // This extension has unlimitedStorage but doesn't grant it to foo.example.com
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("https://foo.example.com/*"), MakeJSONList("clipboardRead"),
      MakeJSONList("unlimitedStorage"));

  EXPECT_TRUE(
      HasUnlimitedStorage(extension.get(), extension->GetResourceURL("")));
  EXPECT_FALSE(
      HasUnlimitedStorage(extension.get(), GetTestURLFor("foo.example.com")));
}
