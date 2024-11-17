// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

BASE_FEATURE(kDisableJavascriptOptimizerByDefault,
             "DisableJavascriptOptimizerByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool AreV8OptimizationsDisabledOnActiveWebContents(Browser* browser) {
  content::WebContents* web_contents =
      browser->GetActiveTabInterface()->GetContents();
  content::RenderProcessHost* rph =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  return rph->AreV8OptimizationsDisabled();
}

}  // namespace

class JavascriptOptimizerFeatureBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  JavascriptOptimizerFeatureBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          kDisableJavascriptOptimizerByDefault);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kDisableJavascriptOptimizerByDefault);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the effect of the DisableJavascriptOptimizerByDefault feature
// flag.
IN_PROC_BROWSER_TEST_P(JavascriptOptimizerFeatureBrowserTest,
                       CheckFeatureHasEffect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));

  EXPECT_EQ(GetParam(),
            AreV8OptimizationsDisabledOnActiveWebContents(browser()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         JavascriptOptimizerFeatureBrowserTest,
                         testing::Values(true, false));

typedef InProcessBrowserTest JavascriptOptimizerBrowserTest;

// Test that V8 optimization is disabled when the user disables v8 optimization
// by default via chrome://settings regardless of the status of the
// DisableJavascriptOptimizerByDefault experiment.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest,
                       V8SiteSettingDefaultOff) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents(browser()));
}

// Test that V8 optimization is disabled when the user disables v8 optimization
// via chrome://settings for a specific site regardless of the status of the
// DisableJavascriptOptimizerByDefault experiment.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest,
                       DisabledViaSiteSpecificSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  GURL::Replacements url_replacements;
  url_replacements.SetPortStr("");
  GURL embedded_origin =
      embedded_test_server()->GetOrigin().GetURL().ReplaceComponents(
          url_replacements);
  map->SetContentSettingDefaultScope(embedded_origin, embedded_origin,
                                     ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                     ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents(browser()));
}
