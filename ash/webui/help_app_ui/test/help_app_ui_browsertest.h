// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_TEST_HELP_APP_UI_BROWSERTEST_H_
#define ASH_WEBUI_HELP_APP_UI_TEST_HELP_APP_UI_BROWSERTEST_H_

#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"

class HelpAppUiBrowserTest : public SandboxedWebUiAppTestBase {
 public:
  HelpAppUiBrowserTest();
  ~HelpAppUiBrowserTest() override;

  HelpAppUiBrowserTest(const HelpAppUiBrowserTest&) = delete;
  HelpAppUiBrowserTest& operator=(const HelpAppUiBrowserTest&) = delete;
};

#endif  // ASH_WEBUI_HELP_APP_UI_TEST_HELP_APP_UI_BROWSERTEST_H_
