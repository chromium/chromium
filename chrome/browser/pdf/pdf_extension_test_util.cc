// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_util.h"

#include "content/public/test/browser_test_utils.h"

namespace pdf_extension_test_util {

bool EnsurePDFHasLoaded(content::WebContents* web_contents) {
  bool load_success = false;
  CHECK(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.addEventListener('message', event => {"
      "  if (event.origin !="
      "          'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai' ||"
      "      event.data.type != 'documentLoaded') {"
      "    return;"
      "  }"
      "  window.domAutomationController.send("
      "       event.data.load_state == 'success');"
      "});"
      "document.getElementsByTagName('embed')[0].postMessage("
      "    {type: 'initialize'});",
      &load_success));
  return load_success;
}

}  // namespace pdf_extension_test_util
