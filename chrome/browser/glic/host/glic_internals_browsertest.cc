// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_internals_ui.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic {

namespace {

constexpr char kWaitForInternalsLoaded[] = R"js(
  customElements.whenDefined('glic-internals-app').then(() => true)
)js";

}  // namespace

class GlicInternalsBrowserTest : public PlatformBrowserTest {
 public:
  GlicInternalsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlic);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       InternalsSubpathInstantiatesGlicInternalsUI) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals")));
  ASSERT_TRUE(contents->GetWebUI());
  EXPECT_TRUE(contents->GetWebUI()->GetController()->GetAs<GlicInternalsUI>());
  EXPECT_FALSE(contents->GetWebUI()->GetController()->GetAs<GlicUI>());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));
}

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       InternalsTrailingSlashSubpathLoadsCorrectly) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals/")));
  ASSERT_TRUE(contents->GetWebUI());
  EXPECT_TRUE(contents->GetWebUI()->GetController()->GetAs<GlicInternalsUI>());
  EXPECT_FALSE(contents->GetWebUI()->GetController()->GetAs<GlicUI>());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));
}

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       DiagnosticsMarkdownGeneratesFromDOM) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals")));
  ASSERT_TRUE(contents->GetWebUI());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));

  constexpr char kCheckDiagnosticsMarkdown[] = R"js(
    (async () => {
      const app = document.querySelector('glic-internals-app');
      if (!app) {
        return 'app-missing';
      }
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        if (app.data_ &&
            app.shadowRoot.querySelector('#general-contents table')) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      const md = app.getDiagnosticsMarkdown_();
      if (!md.includes('# Glic Internals Diagnostics')) {
        return 'missing-header: ' + md;
      }
      if (!md.includes('## Enablement State') ||
          !md.includes('| Status | Property |')) {
        return 'missing-enablement-table: ' + md;
      }
      if (!md.includes('## Configuration') ||
          !md.includes('| Guest URL |')) {
        return 'missing-config-table: ' + md;
      }

      // Switch to Debug Controls tab and verify markdown still generates
      // identically even when the general tab is hidden.
      app.selectedTabIndex_ = 1;
      await app.updateComplete;
      const mdHidden = app.getDiagnosticsMarkdown_();
      const stripTimestamp = s => s.replace(/- \*\*Timestamp:\*\* [^\n]+\n/, '');
      if (stripTimestamp(md) !== stripTimestamp(mdHidden)) {
        return 'mismatch-when-hidden: ' + mdHidden;
      }

      return 'ok';
    })()
  )js";

  EXPECT_EQ("ok", content::EvalJs(contents, kCheckDiagnosticsMarkdown));
}

IN_PROC_BROWSER_TEST_F(
    GlicInternalsBrowserTest,
    DebugControlsEnumDropdownsExcludeBoundsAndSelectCleanly) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals")));
  ASSERT_TRUE(contents->GetWebUI());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));

  constexpr char kCheckDropdowns[] = R"js(
    (async () => {
      const app = document.querySelector('glic-internals-app');
      if (!app) {
        return 'app-missing';
      }

      // Switch to Debug Controls tab and wait for rendering.
      app.selectedTabIndex_ = 1;
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        if (app.shadowRoot.querySelector('#invokeActuationTargetSelect')) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      const selectIds = [
        'invokeInvocationSourceSelect',
        'invokeFreCompletionWaitModeSelect',
        'invokeFeatureModeSelect',
        'invokeActuationTargetSelect',
      ];

      for (const id of selectIds) {
        const select = app.shadowRoot.querySelector(`#${id}`);
        if (!select) {
          return `missing-select-${id}`;
        }
        const optionTexts = Array.from(select.options).map(o => o.text);
        if (optionTexts.includes('MIN_VALUE') ||
            optionTexts.includes('MAX_VALUE')) {
          return `bounds-present-in-${id}: ${JSON.stringify(optionTexts)}`;
        }
      }

      // Test selecting kUnknown (value 0) in Actuation Target.
      const actuationSelect =
          app.shadowRoot.querySelector('#invokeActuationTargetSelect');
      const actuationSelected = actuationSelect.selectedOptions[0].text;
      if (actuationSelected !== 'kAgentDecides') {
        return `unexpected-initial-actuation: ${actuationSelected}`;
      }

      actuationSelect.value = '0';
      actuationSelect.dispatchEvent(new Event('change'));
      await app.updateComplete;

      const actuationUpdated = actuationSelect.selectedOptions[0].text;
      if (actuationUpdated !== 'kUnknown') {
        return `failed-first-click-select-kUnknown: ${actuationUpdated}`;
      }
      if (app.invokeActuationTarget_ !== 0) {
        return `state-mismatch-kUnknown: ${app.invokeActuationTarget_}`;
      }

      // Test selecting kExperimentalTriggering (value 3) in Feature Mode.
      const featureSelect =
          app.shadowRoot.querySelector('#invokeFeatureModeSelect');
      const featureSelected = featureSelect.selectedOptions[0].text;
      if (featureSelected !== 'kUnspecified') {
        return `unexpected-initial-feature: ${featureSelected}`;
      }
      featureSelect.value = '3';
      featureSelect.dispatchEvent(new Event('change'));
      await app.updateComplete;

      const featureUpdated = featureSelect.selectedOptions[0].text;
      if (featureUpdated !== 'kExperimentalTriggering') {
        return 'failed-first-click-select-kExperimentalTriggering: ' +
            featureUpdated;
      }
      if (app.invokeFeatureMode_ !== 3) {
        return `state-mismatch-feature: ${app.invokeFeatureMode_}`;
      }

      return 'ok';
    })()
  )js";

  EXPECT_EQ("ok", content::EvalJs(contents, kCheckDropdowns));
}

}  // namespace glic
