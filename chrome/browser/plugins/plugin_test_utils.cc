// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_test_utils.h"

#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test_utils.h"

// static
std::string PluginTestUtils::RunTestScript(base::StringPiece test_script,
                                           content::WebContents* contents,
                                           const std::string& element_id) {
  std::string script = base::StringPrintf(
      "var plugin = window.document.getElementById('%s');"
      "if (plugin === undefined ||"
      "    (plugin.nodeName !== 'OBJECT' && plugin.nodeName !== 'EMBED')) {"
      "  window.domAutomationController.send('error');"
      "} else {"
      "  %s"
      "}",
      element_id.c_str(), test_script.data());
  std::string result;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(contents, script, &result));
  return result;
}

// static
void PluginTestUtils::WaitForPlaceholderReady(content::WebContents* contents,
                                              const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data === 'placeholderReady') {"
      "    window.domAutomationController.send('ready');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.hasAttribute('placeholderReady')) {"
      "  window.domAutomationController.send('ready');"
      "  plugin.removeEventListener('message', handleEvent);"
      "}",
      contents, element_id);
  ASSERT_EQ("ready", result);
}
