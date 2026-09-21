// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_JSTEST_BASE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_JSTEST_BASE_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"

class FileManagerTestWebUIProvider;

namespace content {
class ScopedWebUIControllerFactoryRegistration;
}

class FileManagerJsTestBase : public InProcessBrowserTest {
 protected:
  explicit FileManagerJsTestBase(const base::FilePath& base_path);
  ~FileManagerJsTestBase() override;

  // Run the test from chrome://file_manager_test/.
  // |file| is relative path to //ui/file_manager/ .
  void RunTestURL(const std::string& file);

  // Set up & tear down
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  // Declared before `webui_controller_factory_`, which holds a raw pointer to
  // it, so that it outlives the factory.
  std::unique_ptr<FileManagerTestWebUIProvider> test_webui_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> webui_controller_factory_;
  std::unique_ptr<content::ScopedWebUIControllerFactoryRegistration>
      webui_controller_factory_registration_;
  base::FilePath base_path_;

  // Handles collection of code coverage.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;
};

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_JSTEST_BASE_H_
