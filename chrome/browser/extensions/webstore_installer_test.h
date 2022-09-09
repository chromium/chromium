// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_TEST_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace contents {
class WebContents;
}

namespace net {
namespace test_server {
struct HttpRequest;
}
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

  void RunTest(const std::string& test_function_name);

  void RunTest(content::WebContents* web_contents,
               const std::string& test_function_name);

  // Passes |i| to |test_function_name|, and expects that function to
  // return one of "FAILED", "KEEPGOING" or "DONE". KEEPGOING should be
  // returned if more tests remain to be run and the current test succeeded,
  // FAILED is returned when a test fails, and DONE is returned by the last
  // test if it succeeds.
  // This methods returns true iff there are more tests that need to be run.
  bool RunIndexedTest(const std::string& test_function_name, int i);

  // Runs a test without waiting for any results from the renderer.
  void RunTestAsync(const std::string& test_function_name);

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
