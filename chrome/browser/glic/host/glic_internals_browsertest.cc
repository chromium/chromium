// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_internals_ui.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
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

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       HotkeyGlobalScopeMigratedCheckboxTogglesLocalPref) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals")));
  ASSERT_TRUE(contents->GetWebUI());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);

  // Set initial state to not migrated.
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kNotMigrated));

  constexpr char kSwitchToDebugControlsAndVerifyInitial[] = R"js(
    (async () => {
      const app = document.querySelector('glic-internals-app');
      if (!app) {
        return 'app-missing';
      }

      // Switch to Debug Controls tab and wait for rendering.
      app.selectedTabIndex_ = 1;
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        if (app.shadowRoot.querySelector('#hotkeyGlobalScopeMigratedCheckbox')) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      const checkbox =
          app.shadowRoot.querySelector('#hotkeyGlobalScopeMigratedCheckbox');
      if (!checkbox) {
        return 'missing-checkbox';
      }

      if (checkbox.checked !== false) {
        return `expected-unchecked-initially-got-${checkbox.checked}`;
      }

      return 'ok';
    })()
  )js";

  EXPECT_EQ("ok",
            content::EvalJs(contents, kSwitchToDebugControlsAndVerifyInitial));

  // Toggle checkbox on.
  {
    base::RunLoop run_loop;
    PrefChangeRegistrar pref_registrar;
    pref_registrar.Init(local_state);
    pref_registrar.Add(prefs::kGlicDefaultHotkeyScope, run_loop.QuitClosure());

    constexpr char kClickCheckbox[] = R"js(
      (() => {
        const app = document.querySelector('glic-internals-app');
        const checkbox =
            app.shadowRoot.querySelector('#hotkeyGlobalScopeMigratedCheckbox');
        if (!checkbox) {
          return 'missing-checkbox';
        }
        checkbox.click();
        return 'ok';
      })()
    )js";

    EXPECT_EQ("ok", content::EvalJs(contents, kClickCheckbox));
    run_loop.Run();
    EXPECT_NE(local_state->GetInteger(prefs::kGlicDefaultHotkeyScope),
              std::to_underlying(prefs::DefaultHotkeyScope::kNotMigrated));
  }

  // Toggle checkbox off.
  {
    base::RunLoop run_loop;
    PrefChangeRegistrar pref_registrar;
    pref_registrar.Init(local_state);
    pref_registrar.Add(prefs::kGlicDefaultHotkeyScope, run_loop.QuitClosure());

    constexpr char kClickCheckbox[] = R"js(
      (() => {
        const app = document.querySelector('glic-internals-app');
        const checkbox =
            app.shadowRoot.querySelector('#hotkeyGlobalScopeMigratedCheckbox');
        if (!checkbox) {
          return 'missing-checkbox';
        }
        checkbox.click();
        return 'ok';
      })()
    )js";

    EXPECT_EQ("ok", content::EvalJs(contents, kClickCheckbox));
    run_loop.Run();
    EXPECT_EQ(local_state->GetInteger(prefs::kGlicDefaultHotkeyScope),
              std::to_underlying(prefs::DefaultHotkeyScope::kNotMigrated));
  }
}

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       NavigatingToDebugHashOpensDebugTab) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals#debug")));
  ASSERT_TRUE(contents->GetWebUI());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));

  constexpr char kVerifyDebugTab[] = R"js(
    (async () => {
      const app = document.querySelector('glic-internals-app');
      if (!app) {
        return 'app-missing';
      }
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        if (app.shadowRoot.querySelector('#debug-controls-contents')) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (app.selectedTabIndex_ !== 1) {
        return `expected-tab-index-1-got-${app.selectedTabIndex_}`;
      }

      const generalContents =
          app.shadowRoot.querySelector('#general-contents');
      if (!generalContents || !generalContents.hidden) {
        return 'general-contents-should-be-hidden';
      }

      const debugContents =
          app.shadowRoot.querySelector('#debug-controls-contents');
      if (!debugContents || debugContents.hidden) {
        return 'debug-contents-should-not-be-hidden';
      }

      if (window.location.hash !== '#debug') {
        return `expected-hash-#debug-got-${window.location.hash}`;
      }

      return 'ok';
    })()
  )js";

  EXPECT_EQ("ok", content::EvalJs(contents, kVerifyDebugTab));
}

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       NavigatingToGeneralHashOpensGeneralTab) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals#general")));
  ASSERT_TRUE(contents->GetWebUI());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));

  constexpr char kVerifyGeneralTab[] = R"js(
    (async () => {
      const app = document.querySelector('glic-internals-app');
      if (!app) {
        return 'app-missing';
      }
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        if (app.shadowRoot.querySelector('#general-contents')) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (app.selectedTabIndex_ !== 0) {
        return `expected-tab-index-0-got-${app.selectedTabIndex_}`;
      }

      const generalContents =
          app.shadowRoot.querySelector('#general-contents');
      if (!generalContents || generalContents.hidden) {
        return 'general-contents-should-not-be-hidden';
      }

      const debugContents =
          app.shadowRoot.querySelector('#debug-controls-contents');
      if (!debugContents || !debugContents.hidden) {
        return 'debug-contents-should-be-hidden';
      }

      if (window.location.hash !== '#general') {
        return `expected-hash-#general-got-${window.location.hash}`;
      }

      return 'ok';
    })()
  )js";

  EXPECT_EQ("ok", content::EvalJs(contents, kVerifyGeneralTab));
}

IN_PROC_BROWSER_TEST_F(GlicInternalsBrowserTest,
                       TabSwitchingUpdatesUrlHashAndHashchangeUpdatesTab) {
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(contents);
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL("chrome://glic/internals")));
  ASSERT_TRUE(contents->GetWebUI());

  EXPECT_EQ(true, content::EvalJs(contents, kWaitForInternalsLoaded));

  constexpr char kTestTabSwitching[] = R"js(
    (async () => {
      const app = document.querySelector('glic-internals-app');
      if (!app) {
        return 'app-missing';
      }
      let tabs = null;
      let tabElements = [];
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        tabs = app.shadowRoot ? app.shadowRoot.querySelector('#tabs') : null;
        if (tabs && tabs.shadowRoot) {
          await tabs.updateComplete;
          tabElements = tabs.shadowRoot.querySelectorAll('.tab');
          if (tabElements.length >= 2) {
            break;
          }
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (!tabs || !tabs.shadowRoot) {
        return 'missing-cr-tabs';
      }
      if (tabElements.length < 2) {
        return `expected-at-least-2-tabs-got-${tabElements.length}`;
      }

      // Verify initially on general tab.
      if (app.selectedTabIndex_ !== 0) {
        return `expected-initial-tab-0-got-${app.selectedTabIndex_}`;
      }

      // 1. Click Debug Controls tab button.
      tabElements[1].click();
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        const gen = app.shadowRoot ? app.shadowRoot.querySelector('#general-contents') : null;
        const dbg = app.shadowRoot ? app.shadowRoot.querySelector('#debug-controls-contents') : null;
        if (app.selectedTabIndex_ === 1 && window.location.hash === '#debug' &&
            gen && gen.hidden === true && dbg && dbg.hidden === false) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (app.selectedTabIndex_ !== 1) {
        return `expected-tab-1-after-click-got-${app.selectedTabIndex_}`;
      }
      if (window.location.hash !== '#debug') {
        return `expected-hash-#debug-after-click-got-${window.location.hash}`;
      }
      if (app.shadowRoot.querySelector('#general-contents').hidden !== true) {
        return 'general-contents-should-be-hidden-after-switch-to-debug';
      }
      if (app.shadowRoot.querySelector('#debug-controls-contents').hidden !== false) {
        return 'debug-contents-should-be-visible-after-switch-to-debug';
      }

      // 2. Click General tab button.
      const freshTabs = tabs.shadowRoot.querySelectorAll('.tab');
      freshTabs[0].click();
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        const gen = app.shadowRoot ? app.shadowRoot.querySelector('#general-contents') : null;
        const dbg = app.shadowRoot ? app.shadowRoot.querySelector('#debug-controls-contents') : null;
        if (app.selectedTabIndex_ === 0 && window.location.hash === '#general' &&
            gen && gen.hidden === false && dbg && dbg.hidden === true) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (app.selectedTabIndex_ !== 0) {
        return `expected-tab-0-after-click-got-${app.selectedTabIndex_}`;
      }
      if (window.location.hash !== '#general') {
        return `expected-hash-#general-after-click-got-${window.location.hash}`;
      }
      if (app.shadowRoot.querySelector('#general-contents').hidden !== false) {
        return 'general-contents-should-be-visible-after-switch-to-general';
      }
      if (app.shadowRoot.querySelector('#debug-controls-contents').hidden !== true) {
        return 'debug-contents-should-be-hidden-after-switch-to-general';
      }

      // 3. Navigate via location.hash change to #debug.
      window.location.hash = '#debug';
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        const gen = app.shadowRoot ? app.shadowRoot.querySelector('#general-contents') : null;
        const dbg = app.shadowRoot ? app.shadowRoot.querySelector('#debug-controls-contents') : null;
        if (app.selectedTabIndex_ === 1 && window.location.hash === '#debug' &&
            gen && gen.hidden === true && dbg && dbg.hidden === false) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (app.selectedTabIndex_ !== 1) {
        return `expected-tab-1-after-hashchange-got-${app.selectedTabIndex_}`;
      }
      if (window.location.hash !== '#debug') {
        return `expected-hash-#debug-after-hashchange-got-${window.location.hash}`;
      }
      if (app.shadowRoot.querySelector('#general-contents').hidden !== true) {
        return 'general-contents-should-be-hidden-after-hashchange-to-debug';
      }
      if (app.shadowRoot.querySelector('#debug-controls-contents').hidden !== false) {
        return 'debug-contents-should-be-visible-after-hashchange-to-debug';
      }

      // 4. Navigate via location.hash change to #general.
      window.location.hash = '#general';
      for (let i = 0; i < 50; ++i) {
        await app.updateComplete;
        const gen = app.shadowRoot ? app.shadowRoot.querySelector('#general-contents') : null;
        const dbg = app.shadowRoot ? app.shadowRoot.querySelector('#debug-controls-contents') : null;
        if (app.selectedTabIndex_ === 0 && window.location.hash === '#general' &&
            gen && gen.hidden === false && dbg && dbg.hidden === true) {
          break;
        }
        await new Promise(resolve => setTimeout(resolve, 50));
      }

      if (app.selectedTabIndex_ !== 0) {
        return `expected-tab-0-after-hashchange-got-${app.selectedTabIndex_}`;
      }
      if (window.location.hash !== '#general') {
        return `expected-hash-#general-after-hashchange-got-${window.location.hash}`;
      }
      if (app.shadowRoot.querySelector('#general-contents').hidden !== false) {
        return 'general-contents-should-be-visible-after-hashchange-to-general';
      }
      if (app.shadowRoot.querySelector('#debug-controls-contents').hidden !== true) {
        return 'debug-contents-should-be-hidden-after-hashchange-to-general';
      }

      return 'ok';
    })()
  )js";

  EXPECT_EQ("ok", content::EvalJs(contents, kTestTabSwitching));
}

}  // namespace glic
