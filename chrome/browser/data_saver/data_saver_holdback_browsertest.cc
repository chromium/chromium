// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/field_trial_param_associator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "data_saver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Tests if the save data header holdback works as expected.
class DataSaverHoldbackBrowserTest : public InProcessBrowserTest,
                                     public testing::WithParamInterface<bool> {
 protected:
  DataSaverHoldbackBrowserTest() { ConfigureHoldbackExperiment(); }
  void SetUp() override {
    test_server_.ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(test_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void VerifySaveDataHeader(const std::string& expected_header_value) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/echoheader?Save-Data")));
    EXPECT_EQ(
        expected_header_value,
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "document.body.textContent;"));
  }

  void VerifySaveDataAPI(bool expected_header_set) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), test_server_.GetURL("/net_info.html")));
    EXPECT_EQ(expected_header_set, RunScriptExtractBool("getSaveData()"));
  }

  void ConfigureHoldbackExperiment() {
    std::map<std::string, std::string> params;
    params["holdback_web"] = GetParam() ? "true" : "false";
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kDataSaverHoldback, {params}}}, {});
  }

  void TearDown() override {
    data_saver::ResetIsDataSaverEnabledForTesting();
    InProcessBrowserTest::TearDown();
  }

 private:
  bool RunScriptExtractBool(const std::string& script) {
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           script, content::EXECUTE_SCRIPT_USE_MANUAL_REPLY)
        .ExtractBool();
  }

  net::EmbeddedTestServer test_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The data saver holdback is enabled only if the first param is true.
INSTANTIATE_TEST_SUITE_P(All, DataSaverHoldbackBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(DataSaverHoldbackBrowserTest,
                       DataSaverEnabledWithHoldbackEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  data_saver::OverrideIsDataSaverEnabledForTesting(true);

  // If holdback is enabled, then the save-data header should not be set.
  if (GetParam()) {
    VerifySaveDataHeader("None");
  } else {
    VerifySaveDataHeader("on");
  }
  VerifySaveDataAPI(!GetParam());
}
