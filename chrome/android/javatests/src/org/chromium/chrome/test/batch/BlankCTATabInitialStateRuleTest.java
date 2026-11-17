// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.batch;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Dialog;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.test.util.BlankUiTestActivity;

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
        runInitialStateRule();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Modal dialog should be dismissed during batched state reset",
                            assumeNonNull(mCtaTestRule.getActivity().getModalDialogManager())
                                    .isShowing());
                });
        mCtaTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    public void testResetState_restoresWindowFocus() throws Throwable {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();

        BlankUiTestActivity blankActivity = null;
        try {
            blankActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            BlankUiTestActivity.class);
            CriteriaHelper.pollUiThread(
                    () -> !cta.hasWindowFocus(),
                    "Chrome activity should lose window focus after launching another activity",
                    CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);

            if (ThreadUtils.runOnUiThreadBlocking(cta::isInMultiWindowMode)) {
                assertEquals(
                        ActivityState.RESUMED,
                        (int)
                                ThreadUtils.runOnUiThreadBlocking(
                                        () -> ApplicationStatus.getStateForActivity(cta)));
            }

            runInitialStateRule();

            assertTrue(
                    "Chrome activity should have window focus after batched state reset",
                    ThreadUtils.runOnUiThreadBlocking(cta::hasWindowFocus));
            onView(withId(R.id.url_bar))
                    .inRoot(withDecorView(is(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } finally {
            if (blankActivity != null) {
                ApplicationTestUtils.finishActivity(blankActivity);
            }
        }
        mCtaTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    public void testEnsureActivityFocused_allowsFocusedActivityDialog() {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        Dialog dialog =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Dialog d = new Dialog(cta);
                            d.setContentView(new View(cta));
                            d.show();
                            return d;
                        });
        try {
            CriteriaHelper.pollUiThread(
                    () ->
                            assumeNonNull(dialog.getWindow()).getDecorView().hasWindowFocus()
                                    && !cta.hasWindowFocus(),
                    "Dialog decor view should gain window focus while activity loses it",
                    CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);

            assertTrue(
                    "ensureActivityFocused should succeed when an owned dialog holds window focus",
                    ChromeActivityTestRule.ensureActivityFocused(cta));
            assertTrue(
                    "Dialog decor view should retain window focus",
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    assumeNonNull(dialog.getWindow())
                                            .getDecorView()
                                            .hasWindowFocus()));
        } finally {
            ThreadUtils.runOnUiThreadBlocking(dialog::dismiss);
        }
    }

    private void runInitialStateRule() throws Throwable {
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
    }
}
