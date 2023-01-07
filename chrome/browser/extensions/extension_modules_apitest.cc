// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_host_resolver.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"

namespace extensions {

namespace {

constexpr const char kExampleURL[] = "www.example.com";

void MonitorModuleRequest(bool* seen_module_request,
                          const net::test_server::HttpRequest& request) {
  const GURL url = request.GetURL();
  if (url.SchemeIsCryptographic() && url.path() == "/hello_module.js")
    *seen_module_request = true;
}

}  // namespace

class ExtensionModuleTest : public ExtensionApiTest {
 public:
  ExtensionModuleTest() = default;

  ExtensionModuleTest(const ExtensionModuleTest&) = delete;
  ExtensionModuleTest& operator=(const ExtensionModuleTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule(kExampleURL, "127.0.0.1");
    ExtensionApiTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionModuleTest, TestModulesAvailable) {
  ASSERT_TRUE(RunExtensionTest("modules")) << message_;
}

// Tests that extensions can load modules from web (e.g. https:// url).
IN_PROC_BROWSER_TEST_F(ExtensionModuleTest, ModuleFromWeb) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(base::FilePath(FILE_PATH_LITERAL(
      "chrome/test/data/extensions/api_test/module_from_web")));
  bool https_server_seen_module_request = false;
  https_server.RegisterRequestMonitor(base::BindRepeating(
      &MonitorModuleRequest, &https_server_seen_module_request));
  ASSERT_TRUE(https_server.Start());

  const GURL https_module_src =
      https_server.GetURL(kExampleURL, "/hello_module.js");

  TestExtensionDir test_dir;
  // The manifest requires host and CSP permission to |kExampleURL|.
  test_dir.WriteManifest(base::StringPrintf(R"({
      "name": "Modules over https",
      "manifest_version": 2,
      "version": "0.1",
      "background": {"page": "background.html"},
      "content_security_policy":
          "script-src 'self' https://%s:*; object-src 'self'",
      "permissions": ["https://%s:*/*"]
  })", kExampleURL, kExampleURL));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.html"),
                     R"(<script src="background.js" type="module"></script>)");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(
                         R"(import {helloModule} from '%s'; helloModule();)",
                         https_module_src.spec().c_str()));

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  EXPECT_TRUE(https_server_seen_module_request);
}

}  // namespace extensions
