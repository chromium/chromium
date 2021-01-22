// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_util.h"

#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf_extension_test_util {

testing::AssertionResult EnsurePDFHasLoaded(
    content::WebContents* web_contents) {
  bool load_success = false;
  if (!content::ExecuteScriptAndExtractBool(
          web_contents,
          "window.addEventListener('message', event => {"
          "  if (event.origin !=="
          "          'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai') {"
          "    return;"
          "  }"
          "  if (event.data.type === 'documentLoaded') {"
          "    window.domAutomationController.send("
          "        event.data.load_state === 'success');"
          "  } else if (event.data.type === 'passwordPrompted') {"
          "    window.domAutomationController.send(true);"
          "  }"
          "});"
          "document.getElementsByTagName('embed')[0].postMessage("
          "    {type: 'initialize'});",
          &load_success)) {
    return testing::AssertionFailure()
           << "Cannot communicate with PDF extension.";
  }
  return load_success ? testing::AssertionSuccess()
                      : (testing::AssertionFailure() << "Load failed.");
}

}  // namespace pdf_extension_test_util
