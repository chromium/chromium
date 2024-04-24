// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_

#include <string>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace base {
class FilePath;
}

class GURL;

namespace extensions {
class Extension;

// The general flow of these API tests should work like this:
// (1) Setup initial browser state (e.g. create some bookmarks for the
//     bookmark test)
// (2) Call ASSERT_TRUE(RunExtensionTest(name));
// (3) In your extension code, run your test and call chrome.test.pass or
//     chrome.test.fail
// (4) Verify expected browser state.
// TODO(erikkay): There should also be a way to drive events in these tests.
class ExtensionApiTest : public ExtensionBrowserTest {
 public:
  struct RunOptions {
    // Start the test by opening the specified page URL. This must be an
    // absolute URL.
    const char* page_url = nullptr;

    // Start the test by opening the specified extension URL. This is treated
    // as a relative path to an extension resource.
    const char* extension_url = nullptr;

    // The custom arg to be passed into the test.
    const char* custom_arg = nullptr;

    // Launch the test page in an incognito window.
    bool open_in_incognito = false;

    // Launch the extension as a platform app.
    bool launch_as_platform_app = false;

    // Use //extensions/test/data/ as the root path instead of the default
    // path of //chrome/test/data/extensions/api_test/.
    bool use_extensions_root_dir = false;
  };

  explicit ExtensionApiTest(ContextType context_type = ContextType::kNone);
  ~ExtensionApiTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Loads the extension with |extension_name| and default RunOptions and
  // LoadOptions.
  [[nodiscard]] bool RunExtensionTest(const char* extension_name);

  [[nodiscard]] bool RunExtensionTest(const char* extension_name,
                                      const RunOptions& run_options);

  [[nodiscard]] bool RunExtensionTest(const char* extension_name,
                                      const RunOptions& run_options,
                                      const LoadOptions& load_options);

  [[nodiscard]] bool RunExtensionTest(const base::FilePath& extension_path,
                                      const RunOptions& run_options,
                                      const LoadOptions& load_options);

  // Opens the given |url| and waits for the next result from the
  // chrome.test API. If |open_in_incognito| is true, the URL is opened
  // in an off-the-record browser profile. This API is different from
  // RunExtensionTest as it doesn't load an extension.
  [[nodiscard]] bool OpenTestURL(const GURL& url,
                                 bool open_in_incognito = false);

  // Start the test server, and store details of its state. Those details
  // will be available to JavaScript tests using chrome.test.getConfig().
  bool StartEmbeddedTestServer();

  // Initialize the test server and store details of its state. Those details
  // will be available to JavaScript tests using chrome.test.getConfig().
  //
  // Starting the test server is done in two steps; first the server socket is
  // created and starts listening, followed by the start of an IO thread on
  // which the test server will accept connectons.
  //
  // In general you can start the test server using StartEmbeddedTestServer()
  // which handles both steps. When you need to register request handlers that
  // need the server's base URL (either directly or through GetURL()), you will
  // have to initialize the test server via this method first, get the URL and
  // register the handler, and finally start accepting connections on the test
  // server via InitializeEmbeddedTestServer().
  bool InitializeEmbeddedTestServer();

  // Start accepting connections on the test server. Initialize the test server
  // before calling this method via InitializeEmbeddedTestServer(), or use
  // StartEmbeddedTestServer() instead.
  void EmbeddedTestServerAcceptConnections();

  // Start the test WebSocket server, and store details of its state. Those
  // details will be available to javascript tests using
  // chrome.test.getConfig(). Enable HTTP basic authentication if needed.
  bool StartWebSocketServer(const base::FilePath& root_directory,
                            bool enable_basic_auth = false);

  // Sets the additional string argument |customArg| to the test config object,
  // which is available to javascript tests using chrome.test.getConfig().
  void SetCustomArg(std::string_view custom_arg);

  // Test that exactly one extension loaded.  If so, return a pointer to
  // the extension.  If not, return NULL and set message_.
  const Extension* GetSingleLoadedExtension();

  // All extensions tested by ExtensionApiTest are in the "api_test" dir.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  const base::FilePath& shared_test_data_dir() const {
    return shared_test_data_dir_;
  }

  // If it failed, what was the error message?
  std::string message_;

  base::Value::Dict* GetTestConfig() { return test_config_.get(); }

  // Creates a new secure test server that can be used in place of the default
  // HTTP embedded_test_server defined in BrowserTestBase. The new test server
  // can then be retrieved using the same embedded_test_server() method used
  // to get the BrowserTestBase HTTP server.
  void UseHttpsTestServer();

  // This will return either the https test server or the
  // default one specified in BrowserTestBase, depending on if an https test
  // server was created by calling UseHttpsTestServer().
  net::EmbeddedTestServer* embedded_test_server() {
    return (https_test_server_) ? https_test_server_.get()
                                : BrowserTestBase::embedded_test_server();
  }

 private:
  void OpenURL(const GURL& url, bool open_in_incognito);

  // Hold details of the test, set in C++, which can be accessed by
  // javascript using chrome.test.getConfig().
  std::unique_ptr<base::Value::Dict> test_config_;

  // Hold the test WebSocket server.
  std::unique_ptr<net::SpawnedTestServer> websocket_server_;

  // Test data directory shared with //extensions.
  base::FilePath shared_test_data_dir_;

  // Secure test server, isn't created by default. Needs to be
  // created using UseHttpsTestServer() and then called with
  // embedded_test_server().
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_
