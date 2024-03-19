// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_TEST_UTILS_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_TEST_UTILS_H_

#include <string>
#include <string_view>

namespace content {
class WebContents;
}

class PluginTestUtils {
 public:
  PluginTestUtils() = delete;
  PluginTestUtils(const PluginTestUtils&) = delete;
  PluginTestUtils& operator=(const PluginTestUtils&) = delete;

  // Runs the JavaScript |test_script|, which is provided 'plugin' as a variable
  // referencing the |element_id| element. Returns the string extracted from
  // window.domAutomationController.
  static std::string RunTestScript(std::string_view test_script,
                                   content::WebContents* contents,
                                   const std::string& element_id);

  // Blocks until the placeholder is ready.
  static void WaitForPlaceholderReady(content::WebContents* contents,
                                      const std::string& element_id);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_TEST_UTILS_H_
