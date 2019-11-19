// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class FilePath;
}

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
  // Flags used to configure how the tests are run.
  // TODO(aa): Many of these are dupes of ExtensionBrowserTest::Flags. Combine
  // somehow?
  enum Flags {
    kFlagNone = 0,

    // Allow the extension to run in incognito mode.
    kFlagEnableIncognito = 1 << 0,

    // Launch the test page in an incognito window.
    kFlagUseIncognito = 1 << 1,

    // Allow file access for the extension.
    kFlagEnableFileAccess = 1 << 2,

    // Loads the extension with location COMPONENT.
    kFlagLoadAsComponent = 1 << 3,

    // Launch the extension as a platform app.
    kFlagLaunchPlatformApp = 1 << 4,

    // Don't fail when the loaded manifest has warnings.
    kFlagIgnoreManifestWarnings = 1 << 5,

    // Allow manifest versions older that Extension::kModernManifestVersion.
    // Used to test old manifest features.
    kFlagAllowOldManifestVersions = 1 << 6,

    // Load the extension using //extensions/test/data/ as the root path instead
    // of loading from //chrome/test/data/extensions/api_test/.
    kFlagUseRootExtensionsDir = 1 << 7,

    // Pass the FOR_LOGIN_SCREEN flag when loading the extension. This flag is
    // usually provided for force-installed extension on the login screen. This
    // also sets the location to EXTERNAL_POLICY.
    kFlagLoadForLoginScreen = 1 << 8,

    // Load the event page extension as a Service Worker based background
    // extension.
    kFlagRunAsServiceWorkerBasedExtension = 1 << 9,
  };

  ExtensionApiTest();
  ~ExtensionApiTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Loads |extension_name| and waits for pass / fail notification.
  // |extension_name| is a directory in "chrome/test/data/extensions/api_test".
  bool RunExtensionTest(const std::string& extension_name);

  // Same as RunExtensionTest, except run with the specific |flags| (as defined
  // in the Flags enum).
  bool RunExtensionTestWithFlags(const std::string& extension_name, int flags);

  // Similar to RunExtensionTest, except sets an additional string argument
  // |customArg| to the test config object.
  bool RunExtensionTestWithArg(const std::string& extension_name,
                               const char* custom_arg);

  // Similar to RunExtensionTest, except sets an additional string arguments
  // |customArg| to the test config object and |flags| (as defined in the Flags
  // enum).
  bool RunExtensionTestWithFlagsAndArg(const std::string& extension_name,
                                       const char* custom_arg,
                                       int flags);

  // Same as RunExtensionTest, but enables the extension for incognito mode.
  bool RunExtensionTestIncognito(const std::string& extension_name);

  // Same as RunExtensionTest, but ignores any warnings in the manifest.
  bool RunExtensionTestIgnoreManifestWarnings(
      const std::string& extension_name);

  // Same as RunExtensionTest, allow old manifest ersions.
  bool RunExtensionTestAllowOldManifestVersion(
      const std::string& extension_name);

  // Same as RunExtensionTest, but loads extension as component.
  bool RunComponentExtensionTest(const std::string& extension_name);

  // Same as RunComponentExtensionTest, but provides extra arg.
  bool RunComponentExtensionTestWithArg(const std::string& extension_name,
                                        const char* custom_arg);

  // Same as RunExtensionTest, but disables file access.
  bool RunExtensionTestNoFileAccess(const std::string& extension_name);

  // Same as RunExtensionTestIncognito, but disables file access.
  bool RunExtensionTestIncognitoNoFileAccess(const std::string& extension_name);

  // If not empty, Load |extension_name|, load |page_url| and wait for pass /
  // fail notification from the extension API on the page. Note that if
  // |page_url| is not a valid url, it will be treated as a resource within
  // the extension. |extension_name| is a directory in
  // "test/data/extensions/api_test".
  bool RunExtensionSubtest(const std::string& extension_name,
                           const std::string& page_url);

  // Same as RunExtensionSubtest, except run with the specific |flags|
  // (as defined in the Flags enum).
  bool RunExtensionSubtest(const std::string& extension_name,
                           const std::string& page_url,
                           int flags);

  // As above but with support for injecting a custom argument into the test
  // config.
  bool RunExtensionSubtestWithArg(const std::string& extension_name,
                                  const std::string& page_url,
                                  const char* custom_arg);

  // As above but with support for custom flags defined in Flags above.
  bool RunExtensionSubtestWithArgAndFlags(const std::string& extension_name,
                                          const std::string& page_url,
                                          const char* custom_arg,
                                          int flags);

  // Load |page_url| and wait for pass / fail notification from the extension
  // API on the page.
  bool RunPageTest(const std::string& page_url);
  bool RunPageTest(const std::string& page_url, int flags);

  // Similar to RunExtensionTest, except used for running tests in platform app
  // shell windows.
  bool RunPlatformAppTest(const std::string& extension_name);

  // Similar to RunPlatformAppTest, except sets an additional string argument
  // |customArg| to the test config object.
  bool RunPlatformAppTestWithArg(const std::string& extension_name,
                                 const char* custom_arg);

  // Similar to RunPlatformAppTest, with custom |flags| (as defined in the Flags
  // enum). The kFlagLaunchPlatformApp flag is automatically added.
  bool RunPlatformAppTestWithFlags(const std::string& extension_name,
                                   int flags);

  // Similar to RunPlatformAppTestWithFlags above, except it has an additional
  // string argument |customArg| to the test config object.
  bool RunPlatformAppTestWithFlags(const std::string& extension_name,
                                   const char* custom_arg,
                                   int flags);

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

  // Start the test FTP server, and store details of its state. Those
  // details will be available to JavaScript tests using
  // chrome.test.getConfig().
  bool StartFTPServer(const base::FilePath& root_directory);

  // Sets the additional string argument |customArg| to the test config object,
  // which is available to javascript tests using chrome.test.getConfig().
  void SetCustomArg(base::StringPiece custom_arg);

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

 private:
  bool RunExtensionTestImpl(const std::string& extension_name,
                            const std::string& test_page,
                            const char* custom_arg,
                            int flags);

  // Hold details of the test, set in C++, which can be accessed by
  // javascript using chrome.test.getConfig().
  std::unique_ptr<base::DictionaryValue> test_config_;

  // Hold the test WebSocket server.
  std::unique_ptr<net::SpawnedTestServer> websocket_server_;

  // Hold the test FTP server.
  std::unique_ptr<net::SpawnedTestServer> ftp_server_;

  // Test data directory shared with //extensions.
  base::FilePath shared_test_data_dir_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APITEST_H_
