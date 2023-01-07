// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_TEST_H_

#include <string>

#include "chrome/browser/extensions/extension_browsertest.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace net::test_server {
struct HttpRequest;
}

class WebstoreInstallerTest : public extensions::ExtensionBrowserTest {
 public:
  WebstoreInstallerTest(const std::string& webstore_domain,
                        const std::string& test_data_path,
                        const std::string& crx_filename,
                        const std::string& verified_domain,
                        const std::string& unverified_domain);

  WebstoreInstallerTest(const WebstoreInstallerTest&) = delete;
  WebstoreInstallerTest& operator=(const WebstoreInstallerTest&) = delete;

  ~WebstoreInstallerTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  GURL GenerateTestServerUrl(const std::string& domain,
                             const std::string& page_filename);

  // Can be overridden to inspect requests to the embedded test server.
  virtual void ProcessServerRequest(
      const net::test_server::HttpRequest& request);

  // Configures command line switches to simulate a user accepting the install
  // prompt.
  void AutoAcceptInstall();

  // Configures command line switches to simulate a user cancelling the install
  // prompt.
  void AutoCancelInstall();

  std::string webstore_domain_;
  std::string test_data_path_;
  std::string crx_filename_;
  std::string verified_domain_;
  std::string unverified_domain_;
  std::string test_gallery_url_;

  std::unique_ptr<extensions::ScopedTestDialogAutoConfirm>
      install_auto_confirm_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_TEST_H_
