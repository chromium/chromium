// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_WEB_APPLICATIONS_TEST_SANDBOXED_WEB_UI_TEST_BASE_H_
#define ASH_WEBUI_WEB_APPLICATIONS_TEST_SANDBOXED_WEB_UI_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/test/base/ash/mojo_web_ui_browser_test.h"

namespace content {

struct EvalJsResult;

}  // namespace content

// A base class that can be extended by SWA browser test to inject scripts.
class SandboxedWebUiAppTestBase : public MojoWebUIBrowserTest {
 public:
  // Initialize the test harnesss for the |host_url| web UI. Starts a content::
  // TestNavigationObserver watching for |sandboxed_url| and, when it loads,
  // automatically injects |scripts|, in order, into the sandboxed frame.
  SandboxedWebUiAppTestBase(const std::string& host_url,
                            const std::string& sandboxed_url,
                            const std::vector<base::FilePath>& scripts,
                            const std::string& guest_test_module = {},
                            const std::string& test_harness_module = {});
  ~SandboxedWebUiAppTestBase() override;

  SandboxedWebUiAppTestBase(const SandboxedWebUiAppTestBase&) = delete;
  SandboxedWebUiAppTestBase& operator=(const SandboxedWebUiAppTestBase&) =
      delete;

  // Runs the JavaScript test case corresponding to the current gtest. This
  // requires `test_harness_module` to be configured. If provided, `helper` is a
  // helper method on the current test harness that will be invoked with the
  // name of the current test case.
  void RunCurrentTest(const std::string& helper = {});

  // Configures and installs a handler to deliver testing resources into a
  // WebUIDataSource configured using MaybeConfigureTestableDataSource().
  // This installs the "default" handler which serves requests of the form
  // scheme://origin/<file>, where <file> is in |resource_files| and can be
  // found from |root_folder| under the source tree.
  // Tests can invoke SetTestableDataSourceRequestHandlerForTesting() directly
  // for more elaborate handlers.
  static void ConfigureDefaultTestRequestHandler(
      const base::FilePath& root_folder,
      const std::vector<std::string>& resource_files);

  // Returns the contents of the JavaScript library used to help test the
  // sandboxed frame.
  static std::string LoadJsTestLibrary(const base::FilePath& script_path);

  // Returns the sandboxed app frame within the provided |web_ui|.
  static content::RenderFrameHost* GetAppFrame(content::WebContents* web_ui);

  // Runs |script| in the untrusted app frame of |web_ui|. This function assumes
  // the first <iframe> element in |web_ui| is the untrusted (sandboxed)
  // content.
  static content::EvalJsResult EvalJsInAppFrame(content::WebContents* web_ui,
                                                const std::string& script);

  // MojoWebUIBrowserTest:
  void SetUpOnMainThread() override;

 private:
  class TestCodeInjector;

  std::unique_ptr<TestCodeInjector> injector_;
  const std::string host_url_;
  const std::string sandboxed_url_;
  const std::vector<base::FilePath> scripts_;
  const std::string guest_test_module_;
  const std::string test_harness_module_;
};

#endif  // ASH_WEBUI_WEB_APPLICATIONS_TEST_SANDBOXED_WEB_UI_TEST_BASE_H_
