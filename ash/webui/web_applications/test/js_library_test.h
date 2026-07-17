// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_WEB_APPLICATIONS_TEST_JS_LIBRARY_TEST_H_
#define ASH_WEBUI_WEB_APPLICATIONS_TEST_JS_LIBRARY_TEST_H_

#include <memory>

#include "chrome/test/base/ash/mojo_web_ui_browser_test.h"

namespace content {
class WebUIControllerFactory;
}  // namespace content

// Base test class used to test JS libraries for System Apps. It setups
// chrome://system-app-test and chrome-untrusted://system-app-test URLs and
// loads files from ash/webui/system_apps/public/js/.
class JsLibraryTest : public MojoWebUIBrowserTest {
 public:
  JsLibraryTest();
  ~JsLibraryTest() override;

  JsLibraryTest(const JsLibraryTest&) = delete;
  JsLibraryTest& operator=(const JsLibraryTest&) = delete;

 private:
  std::unique_ptr<content::WebUIControllerFactory> factory_;
};

#endif  // ASH_WEBUI_WEB_APPLICATIONS_TEST_JS_LIBRARY_TEST_H_
