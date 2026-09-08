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

}  // namespace glic
