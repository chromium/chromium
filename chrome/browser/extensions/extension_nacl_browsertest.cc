// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/nacl/common/nacl_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::PluginService;
using content::WebContents;
using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;

namespace {

const char kExtensionId[] = "bjjcibdiodkkeanflmiijlcfieiemced";

// This class tests that the Native Client plugin is blocked unless the
// .nexe is part of an extension from the Chrome Webstore.
class NaClExtensionTest : public extensions::ExtensionBrowserTest {
 public:
  NaClExtensionTest() { feature_list_.InitAndEnableFeature(kNaclAllow); }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  enum InstallType {
    INSTALL_TYPE_COMPONENT,
    INSTALL_TYPE_UNPACKED,
    INSTALL_TYPE_FROM_WEBSTORE,
    INSTALL_TYPE_NON_WEBSTORE,
  };

  enum PluginType {
    PLUGIN_TYPE_NONE = 0,
    PLUGIN_TYPE_EMBED = 1,
    PLUGIN_TYPE_CONTENT_HANDLER = 2,
    PLUGIN_TYPE_ALL = PLUGIN_TYPE_EMBED |
                      PLUGIN_TYPE_CONTENT_HANDLER,
  };

  const Extension* InstallExtension(const base::FilePath& file_path,
                                    InstallType install_type) {
    extensions::ExtensionRegistry* registry = extension_registry();
    const Extension* extension = nullptr;
    switch (install_type) {
      case INSTALL_TYPE_COMPONENT:
        if (LoadExtensionAsComponent(file_path)) {
          extension = registry->enabled_extensions().GetByID(kExtensionId);
        }
        break;

      case INSTALL_TYPE_UNPACKED:
        // Install the extension from a folder so it's unpacked.
        if (LoadExtension(file_path)) {
          extension = registry->enabled_extensions().GetByID(kExtensionId);
        }
        break;

      case INSTALL_TYPE_FROM_WEBSTORE:
        // Install native_client.crx from the webstore.
        if (InstallExtensionFromWebstore(file_path, 1)) {
          extension = registry->enabled_extensions().GetByID(
              last_loaded_extension_id());
        }
        break;

      case INSTALL_TYPE_NON_WEBSTORE:
        // Install native_client.crx but not from the webstore.
        if (extensions::ExtensionBrowserTest::InstallExtension(file_path, 1)) {
          extension = registry->enabled_extensions().GetByID(
              last_loaded_extension_id());
        }
        break;
    }
    return extension;
  }

  const Extension* InstallExtension(InstallType install_type) {
    base::FilePath file_path = test_data_dir_.AppendASCII("native_client");
    return InstallExtension(file_path, install_type);
  }

  const Extension* InstallHostedApp() {
    base::FilePath file_path = test_data_dir_.AppendASCII(
        "native_client_hosted_app");
    return InstallExtension(file_path, INSTALL_TYPE_FROM_WEBSTORE);
  }

  bool IsNaClPluginLoaded() {
    // Make sure plugins are loaded off disk first.
    {
      base::RunLoop run_loop;
      PluginService::GetInstance()->GetPlugins(base::BindLambdaForTesting(
          [&](const std::vector<content::WebPluginInfo>&) {
            run_loop.Quit();
          }));
      run_loop.Run();
    }

    static const base::FilePath path(nacl::kInternalNaClPluginFileName);
    content::WebPluginInfo info;
    return PluginService::GetInstance()->GetPluginInfoByPath(path, &info);
  }

  void CheckPluginsCreated(const GURL& url, PluginType expected_to_succeed) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    // Don't run tests if the NaCl plugin isn't loaded.
    if (!IsNaClPluginLoaded())
      return;

    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    bool embedded_plugin_created =
        content::EvalJs(web_contents, "EmbeddedPluginCreated();").ExtractBool();
    bool content_handler_plugin_created =
        content::EvalJs(web_contents, "ContentHandlerPluginCreated();")
            .ExtractBool();

    EXPECT_EQ(embedded_plugin_created,
              (expected_to_succeed & PLUGIN_TYPE_EMBED) != 0);
    EXPECT_EQ(content_handler_plugin_created,
              (expected_to_succeed & PLUGIN_TYPE_CONTENT_HANDLER) != 0);
  }

  void CheckPluginsCreated(const Extension* extension,
                           PluginType expected_to_succeed) {
    CheckPluginsCreated(extension->GetResourceURL("test.html"),
                        expected_to_succeed);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the NaCl plugin isn't blocked for Webstore extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_WebStoreExtension) {
  const Extension* extension = InstallExtension(INSTALL_TYPE_FROM_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(extension, PLUGIN_TYPE_ALL);
}

// Test that the NaCl plugin is blocked for non-Webstore extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_NonWebStoreExtension) {
  const Extension* extension = InstallExtension(INSTALL_TYPE_NON_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(extension, PLUGIN_TYPE_NONE);
}

// Test that the NaCl plugin isn't blocked for component extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_ComponentExtension) {
  const Extension* extension = InstallExtension(INSTALL_TYPE_COMPONENT);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(), ManifestLocation::kComponent);
  CheckPluginsCreated(extension, PLUGIN_TYPE_ALL);
}

// Test that the NaCl plugin isn't blocked for unpacked extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_UnpackedExtension) {
  const Extension* extension = InstallExtension(INSTALL_TYPE_UNPACKED);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(), ManifestLocation::kUnpacked);
  CheckPluginsCreated(extension, PLUGIN_TYPE_ALL);
}

// Test that the NaCl plugin is blocked for non chrome-extension urls, except
// if it's a content (MIME type) handler.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_NonExtensionScheme) {
  const Extension* extension = InstallExtension(INSTALL_TYPE_FROM_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(
      embedded_test_server()->GetURL("/extensions/native_client/test.html"),
      PLUGIN_TYPE_CONTENT_HANDLER);
}

// Test that NaCl plugin isn't blocked for hosted app URLs.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_HostedApp) {
  GURL url =
      embedded_test_server()->GetURL("/extensions/native_client/test.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  replace_host.ClearPort();
  url = url.ReplaceComponents(replace_host);

  const Extension* extension = InstallHostedApp();
  ASSERT_TRUE(extension);
  CheckPluginsCreated(url, PLUGIN_TYPE_ALL);
}

// Verify that there is no renderer crash when PNaCl plugin is loaded in a
// subframe with a remote parent / main frame.  This is a regression test
// for https://crbug.com/728295.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, MainFrameIsRemote) {
  // The test tries to load a PNaCl plugin into an *extension* frame to avoid
  // running into the following error: "Only unpacked extensions and apps
  // installed from the Chrome Web Store can load NaCl modules without enabling
  // Native Client in about:flags."
  extensions::TestExtensionDir ext_dir;
  ext_dir.WriteFile(FILE_PATH_LITERAL("subframe.html"),
                    "<html><body>Extension frame</body></html>");
  ext_dir.WriteManifest(
      R"(
      {
        "name": "ChromeSitePerProcessTest.MainFrameIsRemote",
        "version": "0.1",
        "manifest_version": 2,
        "web_accessible_resources": [ "subframe.html" ]
      }
      )");
  const extensions::Extension* extension =
      LoadExtension(ext_dir.UnpackedPath());

  // Navigate to a page with an iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the subframe to the extension's html file.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents, "test", extension->GetResourceURL("subframe.html")));

  // Sanity check - the test setup should cause main frame and subframe to be in
  // a different process.
  content::RenderFrameHost* subframe = ChildFrameAt(web_contents, 0);
  EXPECT_NE(web_contents->GetPrimaryMainFrame()->GetProcess(),
            subframe->GetProcess());

  // Insert a plugin element into the subframe.  Before the fix from
  // https://crrev.com/2932703005 this would have trigerred a crash reported in
  // https://crbug.com/728295.
  std::string script = R"(
      var embed = document.createElement("embed");
      embed.id = "test_nexe";
      embed.name = "nacl_module";
      embed.type = "application/x-pnacl";
      embed.src = "doesnt-exist.nmf";
      new Promise(resolve => {
        embed.addEventListener('error', function() {
            resolve(true);
        });
        document.body.appendChild(embed);
      });
       )";
  EXPECT_EQ(true, EvalJs(subframe, script));

  // If we get here, then it means that the renderer didn't crash (the crash
  // would have prevented the "error" event from firing and so
  // EvalJs above wouldn't return).
}

}  // namespace
