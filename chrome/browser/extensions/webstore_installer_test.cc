// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer_test.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"

using net::test_server::HttpRequest;

WebstoreInstallerTest::WebstoreInstallerTest(
    const std::string& webstore_domain,
    const std::string& test_data_path,
    const std::string& crx_filename,
    const std::string& verified_domain,
    const std::string& unverified_domain)
    : webstore_domain_(webstore_domain),
      test_data_path_(test_data_path),
      crx_filename_(crx_filename),
      verified_domain_(verified_domain),
      unverified_domain_(unverified_domain) {
}

WebstoreInstallerTest::~WebstoreInstallerTest() {}

void WebstoreInstallerTest::SetUpCommandLine(base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &WebstoreInstallerTest::ProcessServerRequest, base::Unretained(this)));
  // We start the test server now instead of in
  // SetUpInProcessBrowserTestFixture so that we can get its port number.
  ASSERT_TRUE(embedded_test_server()->Start());

  net::HostPortPair host_port = embedded_test_server()->host_port_pair();
  test_gallery_url_ =
      base::StringPrintf("http://%s:%d/%s", webstore_domain_.c_str(),
                         host_port.port(), test_data_path_.c_str());
  command_line->AppendSwitchASCII(
      switches::kAppsGalleryURL, test_gallery_url_);

  GURL crx_url = GenerateTestServerUrl(webstore_domain_, crx_filename_);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryUpdateURL, crx_url.spec());

  // Allow tests to call window.gc(), so that we can check that callback
  // functions don't get collected prematurely.
  command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                  "--expose-gc");
}

void WebstoreInstallerTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();

  host_resolver()->AddRule(webstore_domain_, "127.0.0.1");
  host_resolver()->AddRule(verified_domain_, "127.0.0.1");
  host_resolver()->AddRule(unverified_domain_, "127.0.0.1");
}

GURL WebstoreInstallerTest::GenerateTestServerUrl(
    const std::string& domain,
    const std::string& page_filename) {
  GURL page_url = embedded_test_server()->GetURL(base::StringPrintf(
      "/%s/%s", test_data_path_.c_str(), page_filename.c_str()));

  GURL::Replacements replace_host;
  replace_host.SetHostStr(domain);
  return page_url.ReplaceComponents(replace_host);
}

void WebstoreInstallerTest::ProcessServerRequest(const HttpRequest& request) {}

void WebstoreInstallerTest::AutoAcceptInstall() {
  install_auto_confirm_.reset();  // Destroy any old override first.
  install_auto_confirm_ =
      std::make_unique<extensions::ScopedTestDialogAutoConfirm>(
          extensions::ScopedTestDialogAutoConfirm::ACCEPT);
}

void WebstoreInstallerTest::AutoCancelInstall() {
  install_auto_confirm_.reset();  // Destroy any old override first.
  install_auto_confirm_ =
      std::make_unique<extensions::ScopedTestDialogAutoConfirm>(
          extensions::ScopedTestDialogAutoConfirm::CANCEL);
}
