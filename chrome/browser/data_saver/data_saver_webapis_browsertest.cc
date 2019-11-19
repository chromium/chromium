// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

// Verify that the saveData attribute in NetInfo JavaScript API is set
// correctly.
class DataSaverWebAPIsBrowserTest : public InProcessBrowserTest {
 protected:
  void EnableDataSaver(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
  }

  void SetUp() override {
    test_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(test_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void VerifySaveDataAPI(bool expected_header_set, Browser* browser = nullptr) {
    if (!browser)
      browser = InProcessBrowserTest::browser();
    ui_test_utils::NavigateToURL(browser,
                                 test_server_.GetURL("/net_info.html"));
    EXPECT_EQ(expected_header_set,
              RunScriptExtractBool(browser, "getSaveData()"));
  }

 private:
  bool RunScriptExtractBool(Browser* browser, const std::string& script) {
    bool data;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        browser->tab_strip_model()->GetActiveWebContents(), script, &data));
    return data;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  net::EmbeddedTestServer test_server_;
};

IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest, DataSaverEnabledJS) {
  EnableDataSaver(true);
  VerifySaveDataAPI(true);
}

IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest, DataSaverDisabledJS) {
  EnableDataSaver(false);
  VerifySaveDataAPI(false);
}

IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest, DataSaverToggleJS) {
  EnableDataSaver(false);
  VerifySaveDataAPI(false);

  EnableDataSaver(true);
  VerifySaveDataAPI(true);

  EnableDataSaver(false);
  VerifySaveDataAPI(false);
}

IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest,
                       DataSaverDisabledInIncognito) {
  EnableDataSaver(true);
  VerifySaveDataAPI(false, CreateIncognitoBrowser());
}
