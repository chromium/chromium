// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "data_saver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

// Verify that the saveData attribute in NetInfo JavaScript API is set
// correctly.
class DataSaverWebAPIsBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    test_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(test_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void VerifySaveDataAPI(bool expected_header_set, Browser* browser = nullptr) {
    if (!browser)
      browser = InProcessBrowserTest::browser();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, test_server_.GetURL("/net_info.html")));
    EXPECT_EQ(expected_header_set,
              RunScriptExtractBool(browser, "getSaveData()"));
  }

  void TearDown() override {
    data_saver::ResetIsDataSaverEnabledForTesting();
    InProcessBrowserTest::TearDown();
  }

 private:
  bool RunScriptExtractBool(Browser* browser, const std::string& script) {
    return content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           script)
        .ExtractBool();
  }

  net::EmbeddedTestServer test_server_;
};

// TODO(crbug.com/40250644): Fix and enable test.
IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest,
                       DISABLED_DataSaverEnabledJS) {
  data_saver::OverrideIsDataSaverEnabledForTesting(true);
  VerifySaveDataAPI(true);
}

IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest, DataSaverDisabledJS) {
  data_saver::OverrideIsDataSaverEnabledForTesting(false);
  VerifySaveDataAPI(false);
}

// TODO(crbug.com/40250644): Fix and enable test.
IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest,
                       DISABLED_DataSaverToggleJS) {
  data_saver::OverrideIsDataSaverEnabledForTesting(false);
  VerifySaveDataAPI(false);

  data_saver::OverrideIsDataSaverEnabledForTesting(true);
  VerifySaveDataAPI(true);

  data_saver::OverrideIsDataSaverEnabledForTesting(false);
  VerifySaveDataAPI(false);
}

IN_PROC_BROWSER_TEST_F(DataSaverWebAPIsBrowserTest,
                       DataSaverDisabledInIncognito) {
  data_saver::OverrideIsDataSaverEnabledForTesting(true);
  VerifySaveDataAPI(false, CreateIncognitoBrowser());
}
