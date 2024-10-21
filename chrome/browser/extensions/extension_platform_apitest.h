// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_

#include <string>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/extensions/extension_platform_browsertest.h"

namespace base {
class FilePath;
}

namespace extensions {

// The general flow of these API tests should work like this:
// (1) Setup initial browser state (e.g. create some bookmarks for the
//     bookmark test)
// (2) Call ASSERT_TRUE(RunExtensionTest(name));
// (3) In your extension code, run your test and call chrome.test.pass or
//     chrome.test.fail
// (4) Verify expected browser state.
// TODO(erikkay): There should also be a way to drive events in these tests.
class ExtensionPlatformApiTest : public ExtensionPlatformBrowserTest {
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

  explicit ExtensionPlatformApiTest(
      ContextType context_type = ContextType::kNone);
  ~ExtensionPlatformApiTest() override;

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

  // Sets the additional string argument |customArg| to the test config object,
  // which is available to javascript tests using chrome.test.getConfig().
  void SetCustomArg(std::string_view custom_arg);

  // All extensions tested by ExtensionPlatformApiTest are in the "api_test"
  // dir.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  const base::FilePath& shared_test_data_dir() const {
    return shared_test_data_dir_;
  }

  // If it failed, what was the error message?
  std::string message_;

  base::Value::Dict* GetTestConfig() { return test_config_.get(); }

 private:
  // Hold details of the test, set in C++, which can be accessed by
  // javascript using chrome.test.getConfig().
  std::unique_ptr<base::Value::Dict> test_config_;

  // Test data directory shared with //extensions.
  base::FilePath shared_test_data_dir_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_APITEST_H_
