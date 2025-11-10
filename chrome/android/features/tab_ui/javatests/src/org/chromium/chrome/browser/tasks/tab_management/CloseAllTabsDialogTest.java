// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.SystemClock;
import android.view.MotionEvent;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Arrays;
import java.util.List;

/** An end-to-end test of the close all tabs dialog. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CloseAllTabsDialogTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet()
                            .value(false, false)
                            .name("NonIncognito_ClickAppMenuViaTouchScreen"),
                    new ParameterSet().value(false, true).name("NonIncognito_ClickAppMenuViaMouse"),
                    new ParameterSet()
                            .value(true, false)
                            .name("Incognito_ClickAppMenuViaTouchScreen"),
                    new ParameterSet().value(true, true).name("Incognito_ClickAppMenuViaMouse"));

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final boolean mIsIncognito;
    private final boolean mClickAppMenuViaMouse;
    private WebPageStation mInitialPage;

    public CloseAllTabsDialogTest(boolean isIncognito, boolean clickAppMenuViaMouse) {
        mIsIncognito = isIncognito;
        mClickAppMenuViaMouse = clickAppMenuViaMouse;
    }

    @Before
    public void setUp() {
        mInitialPage = mActivityTestRule.startOnBlankPage();
    }

    /** Tests that close all tabs works after modal dialog. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        if (mIsIncognito) mActivityTestRule.newIncognitoTabFromMenu();
        navigateToCloseAllTabsDialog(selector);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.positive_button), true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0, selector.getModel(mIsIncognito).getCount());
                    assertUndoSnackbar(/* wasCloseAllTabsConfirmed= */ true);
                });
    }

    /** Tests that close all tabs stops if dismissing modal dialog. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testCancelCloseAllTabs() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        if (mIsIncognito) mActivityTestRule.newIncognitoTabFromMenu();
        navigateToCloseAllTabsDialog(selector);

        onViewWaiting(withId(org.chromium.chrome.test.R.id.negative_button), true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(1, selector.getModel(mIsIncognito).getCount());
                    assertUndoSnackbar(/* wasCloseAllTabsConfirmed= */ false);
                });
    }

    /**
     * Tests the custom close all tabs animation will run and close tabs. This test does not test
     * the actual animation logic beyond verifying it runs and does not crash as testing the actual
     * animation data is not possible from here.
     */
    @Test
    @LargeTest
    @EnableAnimations
    @Restriction({DeviceFormFactor.PHONE})
    public void testCloseAllTabs_CustomAnimation() {
        TabModelSelector selector =
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get();

        TabUiTestHelper.createTabs(mActivityTestRule.getActivity(), mIsIncognito, 8);
        navigateToCloseAllTabsDialog(selector);
        onViewWaiting(withId(org.chromium.chrome.test.R.id.positive_button), true).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> assertUndoSnackbar(/* wasCloseAllTabsConfirmed= */ true));
        CriteriaHelper.pollUiThread(() -> 0 == selector.getModel(mIsIncognito).getCount());
    }

    private void navigateToCloseAllTabsDialog(TabModelSelector selector) {
        int tabCount = getTabCountOnUiThread(selector.getModel(mIsIncognito));
        assertThat(tabCount).isAtLeast(1);

        // Open the AppMenu in the Tab Switcher and ensure it shows.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });
        onViewWaiting(withId(org.chromium.chrome.test.R.id.app_menu_list))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        // Click close all tabs.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AppMenuTestSupport.callOnItemClick(
                                mActivityTestRule.getAppMenuCoordinator(),
                                mIsIncognito
                                        ? R.id.close_all_incognito_tabs_menu_id
                                        : R.id.close_all_tabs_menu_id,
                                mClickAppMenuViaMouse
                                        ? createClickTriggeringMotionFromMouse()
                                        : null));
    }

    /**
     * Creates a {@link MotionEventInfo} that matches one from a mouse and would trigger a click.
     */
    private static MotionEventInfo createClickTriggeringMotionFromMouse() {
        long downTime = SystemClock.uptimeMillis();
        return MotionEventTestUtils.createMouseMotionInfo(
                downTime, /* eventTime= */ downTime + 50, MotionEvent.ACTION_UP);
    }

    /**
     * Asserts presence of undo snackbar after "close all tabs" dialog is closed.
     *
     * @param wasCloseAllTabsConfirmed whether "close all tabs" was confirmed via the dialog, i.e.,
     *     whether the positive button was clicked.
     */
    private void assertUndoSnackbar(boolean wasCloseAllTabsConfirmed) {
        @Nullable Snackbar snackbar =
                mActivityTestRule.getActivity().getSnackbarManager().getCurrentSnackbarForTesting();
        if (!wasCloseAllTabsConfirmed) {
            assertNull("Cancelling the dialog should never show the undo snackbar", snackbar);
            return;
        }

        if (mIsIncognito) {
            assertNull("Incognito mode should never show the undo snackbar", snackbar);
            return;
        }

        if (mClickAppMenuViaMouse) {
            assertNull("Closing all tabs with a mouse shouldn't show the undo snackbar", snackbar);
        } else {
            assertNotNull("Non-incognito mode should show the undo snackbar", snackbar);
        }
    }
}
