// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.batch;

import static org.junit.Assert.assertFalse;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Tests for {@link BlankCTATabInitialStateRule}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@NullMarked
public class BlankCTATabInitialStateRuleTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testResetTabStateFast_dismissesLingeringModalDialog() throws Throwable {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();

        // Open 2 tabs and leave NewTabGroupDialogFacility open as if a prior batched test failed
        // mid-dialog.
        int firstTabId = firstPage.loadedTabElement.value().getId();
        RegularNewTabPageStation secondPage = firstPage.openNewTabFast();
        int secondTabId = secondPage.loadedTabElement.value().getId();
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();

        TabSwitcherListEditorFacility<RegularTabSwitcherStation> editor =
                tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);
        NewTabGroupDialogFacility<RegularTabSwitcherStation> dialog =
                editor.openAppMenuWithEditor().groupTabs();
        dialog.pickColor(TabGroupColorId.BLUE);

        // Run the batched initial-state rule between tests and verify the next test can start on a
        // blank page without being blocked by the lingering modal dialog.
        BlankCTATabInitialStateRule initialStateRule =
                new BlankCTATabInitialStateRule(
                        mCtaTestRule.getActivityTestRule(), /* clearAllTabState= */ false);
        initialStateRule
                .apply(
                        new Statement() {
                            @Override
                            public void evaluate() {}
                        },
                        Description.EMPTY)
                .evaluate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Modal dialog should be dismissed during batched state reset",
                            assumeNonNull(mCtaTestRule.getActivity().getModalDialogManager())
                                    .isShowing());
                });
        mCtaTestRule.startOnBlankPage();
    }
}
