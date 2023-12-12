// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_
#define ASH_WEBUI_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_

#include <string>

#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"

class MediaAppUiBrowserTest : public SandboxedWebUiAppTestBase {
 public:
  MediaAppUiBrowserTest();
  ~MediaAppUiBrowserTest() override;

  MediaAppUiBrowserTest(const MediaAppUiBrowserTest&) = delete;
  MediaAppUiBrowserTest& operator=(const MediaAppUiBrowserTest&) = delete;

  // Returns the contents of the JavaScript library used to help test the
  // sandboxed frame.
  static std::string AppJsTestLibrary();

  // Loads the test helpers in in the given WebUI in preparation for testing.
  static void PrepareAppForTest(content::WebContents* web_ui);
};

#endif  // ASH_WEBUI_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_
