// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_KEYBOARD_EXTENSION_BROWSER_TEST_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_KEYBOARD_EXTENSION_BROWSER_TEST_H_

#include "base/files/file_path.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}

// See the .cc for default values.
struct DefaultKeyboardExtensionBrowserTestConfig {
  DefaultKeyboardExtensionBrowserTestConfig();

  ~DefaultKeyboardExtensionBrowserTestConfig();

  // The filename of the base framework. This file should be in |test_dir_|.
  std::string base_framework_;

  // The virtual keyboard's extension id.
  extensions::ExtensionId extension_id_;

  // Path to the test directory.
  std::string test_dir_;

  // URL of the keyboard extension.
  std::string url_;
};

class DefaultKeyboardExtensionBrowserTest : public InProcessBrowserTest {
 public:
  // Injects javascript in |file| into the keyboard page and runs the methods in
  // |file| whose names match the expression "test*".
  void RunTest(const base::FilePath& file,
               const DefaultKeyboardExtensionBrowserTestConfig& config);

  // Returns the WebContents that the keyboard with extension |id| is in.
  content::WebContents* GetKeyboardWebContents(const std::string& id);

  // InProcessBrowserTest.
  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  // Accumulates the javascript and injects it when the test starts. The test
  // |file| is in directory |dir| relative to the root testing directory.
  void InjectJavascript(const base::FilePath& dir, const base::FilePath& file);

 private:
  std::string utf8_content_;
};

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_DEFAULT_KEYBOARD_EXTENSION_BROWSER_TEST_H_
