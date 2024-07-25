// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/show_feedback_page.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

class ShowFeedbackPageBrowserTest : public InProcessBrowserTest {
 public:
  ShowFeedbackPageBrowserTest() {}
  ~ShowFeedbackPageBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList scope_feature_list_;
};

// Test that when the policy of UserFeedbackAllowed is false, feedback app is
// not opened.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest, UserFeedbackDisallowed) {
  base::HistogramTester histogram_tester;
  std::string unused;
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceBrowserCommand,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused,
                           /*autofill_metadata=*/base::Value::Dict());
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               false);
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceBrowserCommand,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused,
                           /*autofill_metadata=*/base::Value::Dict());
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

// Test that when the policy of UserFeedbackAllowed is true, feedback app is
// opened and the os_feedback is used when the feature kOsFeedback is enabled.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       OsFeedbackIsOpenedWhenFeatureEnabled) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  base::HistogramTester histogram_tester;
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  const GURL page_url = chrome::GetTargetTabUrl(
      browser()->session_id(), browser()->tab_strip_model()->active_index());
  const GURL expected_url(base::StrCat(
      {ash::kChromeUIOSFeedbackUrl, "/?page_url=",
       base::EscapeQueryParamValue(page_url.spec(), /*use_plus=*/false)}));
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  std::string unused;
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               true);
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceBrowserCommand,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused,
                           /*autofill_metadata=*/base::Value::Dict());
  navigation_observer.Wait();

  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  const GURL visible_url = chrome::FindLastActive()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetVisibleURL();
  EXPECT_TRUE(visible_url.has_query());
  EXPECT_EQ(expected_url, visible_url);
}

// Test that when parameters appended include:
// - `extra_diagnostics` string.
// - `description_template` string.
// - `description_placeholder_text` string.
// - `category_tag` string.
// - `page_url` GURL.
// - `from_assistant` set true.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       OsFeedbackAdditionalAssistantContextAddedToUrl) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
  std::string unused;
  const GURL page_url = chrome::GetTargetTabUrl(
      browser()->session_id(), browser()->tab_strip_model()->active_index());
  const std::string extra_diagnostics = "extra diagnostics param";
  const std::string description_template = "Q1: Question one?";
  const std::string description_placeholder_text =
      "Thanks for giving feedback on the Camera app";
  const std::string category_tag = "category tag param";
  GURL expected_url(base::StrCat(
      {ash::kChromeUIOSFeedbackUrl, "/?extra_diagnostics=",
       base::EscapeQueryParamValue(extra_diagnostics, /*use_plus=*/false),
       "&description_template=",
       base::EscapeQueryParamValue(description_template, /*use_plus=*/false),
       "&description_placeholder_text=",
       base::EscapeQueryParamValue(description_placeholder_text,
                                   /*use_plus=*/false),
       "&category_tag=",
       base::EscapeQueryParamValue(category_tag, /*use_plus=*/false),
       "&page_url=",
       base::EscapeQueryParamValue(page_url.spec(), /*use_plus=*/false),
       "&from_assistant=",
       base::EscapeQueryParamValue("true", /*use_plus=*/false)}));

  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               true);
  chrome::ShowFeedbackPage(
      browser(), feedback::kFeedbackSourceAssistant,
      /*description_template=*/description_template,
      /*description_placeholder_text=*/description_placeholder_text,
      /*category_tag=*/category_tag,
      /*extra_diagnostics=*/extra_diagnostics,
      /*autofill_metadata=*/base::Value::Dict());
  navigation_observer.Wait();

  const GURL visible_url = chrome::FindLastActive()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetVisibleURL();
  EXPECT_TRUE(visible_url.has_query());
  EXPECT_EQ(expected_url, visible_url);
}

// Test that when parameters appended include:
// - `extra_diagnostics` string.
// - `description_template` string.
// - `description_placeholder_text` string.
// - `category_tag` string.
// - `page_url` GURL.
// - `from_settings_search` set true.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       OsFeedbackAdditionalSettingsSearchContextAddedToUrl) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
  std::string unused;
  const GURL page_url = chrome::GetTargetTabUrl(
      browser()->session_id(), browser()->tab_strip_model()->active_index());
  const std::string extra_diagnostics = "extra diagnostics param";
  const std::string description_template = "Q1: Question one?";
  const std::string description_placeholder_text =
      "Thanks for giving feedback on the Camera app";
  const std::string category_tag = "category tag param";
  GURL expected_url(base::StrCat(
      {ash::kChromeUIOSFeedbackUrl, "/?extra_diagnostics=",
       base::EscapeQueryParamValue(extra_diagnostics, /*use_plus=*/false),
       "&description_template=",
       base::EscapeQueryParamValue(description_template, /*use_plus=*/false),
       "&description_placeholder_text=",
       base::EscapeQueryParamValue(description_placeholder_text,
                                   /*use_plus=*/false),
       "&category_tag=",
       base::EscapeQueryParamValue(category_tag, /*use_plus=*/false),
       "&page_url=",
       base::EscapeQueryParamValue(page_url.spec(), /*use_plus=*/false),
       "&from_settings_search=",
       base::EscapeQueryParamValue("true", /*use_plus=*/false)}));

  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               true);

  chrome::ShowFeedbackPage(
      browser(), feedback::kFeedbackSourceOsSettingsSearch,
      /*description_template=*/description_template,
      /*description_placeholder_text=*/description_placeholder_text,
      /*category_tag=*/category_tag,
      /*extra_diagnostics=*/extra_diagnostics,
      /*autofill_metadata=*/base::Value::Dict());
  navigation_observer.Wait();

  const GURL visible_url = chrome::FindLastActive()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetVisibleURL();
  EXPECT_TRUE(visible_url.has_query());
  EXPECT_EQ(expected_url, visible_url);
}

// Test that when parameters appended include:
// - `extra_diagnostics` string.
// - `description_template` string.
// - `description_placeholder_text` string.
// - `category_tag` string.
// - `page_url` GURL.
// - `from_autofill` set true.
// - `autofill_metadata` string.
IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest,
                       OsFeedbackAdditionalAutofillMetadataAddedToUrl) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
  std::string unused;
  const GURL page_url = chrome::GetTargetTabUrl(
      browser()->session_id(), browser()->tab_strip_model()->active_index());
  const std::string extra_diagnostics = "extra diagnostics param";
  const std::string description_template = "Q1: Question one?";
  const std::string description_placeholder_text =
      "Thanks for giving feedback on Autofill";
  const std::string category_tag = "category tag param";
  base::Value::Dict autofill_metadata = base::test::ParseJsonDict(
      R"({"form_signature": "123", "source_url": "test url"})");
  std::string expected_autofill_metadata;
  base::JSONWriter::Write(autofill_metadata, &expected_autofill_metadata);

  GURL expected_url(base::StrCat(
      {ash::kChromeUIOSFeedbackUrl, "/?extra_diagnostics=",
       base::EscapeQueryParamValue(extra_diagnostics, /*use_plus=*/false),
       "&description_template=",
       base::EscapeQueryParamValue(description_template, /*use_plus=*/false),
       "&description_placeholder_text=",
       base::EscapeQueryParamValue(description_placeholder_text,
                                   /*use_plus=*/false),
       "&category_tag=",
       base::EscapeQueryParamValue(category_tag, /*use_plus=*/false),
       "&page_url=",
       base::EscapeQueryParamValue(page_url.spec(), /*use_plus=*/false),
       "&from_autofill=",
       base::EscapeQueryParamValue("true", /*use_plus=*/false),
       "&autofill_metadata=",
       base::EscapeQueryParamValue(expected_autofill_metadata,
                                   /*use_plus=*/false)}));

  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               true);

  chrome::ShowFeedbackPage(
      browser(), feedback::kFeedbackSourceAutofillContextMenu,
      /*description_template=*/description_template,
      /*description_placeholder_text=*/description_placeholder_text,
      /*category_tag=*/category_tag,
      /*extra_diagnostics=*/extra_diagnostics,
      /*autofill_metadata=*/std::move(autofill_metadata));
  navigation_observer.Wait();

  const GURL visible_url = chrome::FindLastActive()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetVisibleURL();
  EXPECT_TRUE(visible_url.has_query());
  EXPECT_EQ(expected_url, visible_url);
}

IN_PROC_BROWSER_TEST_F(ShowFeedbackPageBrowserTest, FeedbackFlowAI) {
  std::string unused;
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceAI,
                           /*description_template=*/unused,
                           /*description_placeholder_text=*/unused,
                           /*category_tag=*/unused,
                           /*extra_diagnostics=*/unused,
                           /*autofill_metadata=*/base::Value::Dict());
  EXPECT_EQ(chrome::kChromeUIFeedbackURL,
            FeedbackDialog::GetInstanceForTest()->GetDialogContentURL());
}

}  // namespace ash
