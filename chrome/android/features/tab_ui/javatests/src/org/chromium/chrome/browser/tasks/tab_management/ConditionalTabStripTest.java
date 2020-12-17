// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.os.Build.VERSION_CODES.M;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;

import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.ListView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.TabsTest.SimulateClickOnMainThread;
import org.chromium.chrome.browser.TabsTest.SimulateTabSwipeOnMainThread;
import org.chromium.chrome.browser.accessibility_tab_switcher.AccessibilityTabModelListItem;
import org.chromium.chrome.browser.accessibility_tab_switcher.AccessibilityTabModelWrapper;
import org.chromium.chrome.browser.accessibility_tab_switcher.OverviewListLayout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;

/** End-to-end tests for conditional tab strip component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.DISABLE_STARTUP_PROMOS})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({CONDITIONAL_TAB_STRIP_ANDROID})
@Features.DisableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID, START_SURFACE_ANDROID})
@Batch(Batch.PER_CLASS)
public class ConditionalTabStripTest {
    // clang-format on
    private static final int TEST_SESSION_MS = 600000;
    private static final int SWIPE_TO_RIGHT_DIRECTION = 1;
    private static final int SWIPE_TO_LEFT_DIRECTION = -1;

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private SimulateTabSwipeOnMainThread mSwipeToNormal;
    private SimulateTabSwipeOnMainThread mSwipeToIncognito;
    private float mPxToDp = 1f;
    private float mTabsViewHeightDp;
    private float mTabsViewWidthDp;

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Before
    public void setUp() {
        // For this test suite, the session time is set to be 0 by default so that we can start a
        // new session by restarting Chrome. Also, the opt-out indicator and the dismiss counter
        // are reset to the initial state.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(0);
        ConditionalTabStripUtils.setOptOutIndicator(false);
        ConditionalTabStripUtils.setContinuousDismissCount(0);
        ConditionalTabStripUtils.setFeatureStatus(ConditionalTabStripUtils.FeatureStatus.DEFAULT);

        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);

        float dpToPx = InstrumentationRegistry.getInstrumentation()
                               .getContext()
                               .getResources()
                               .getDisplayMetrics()
                               .density;
        mPxToDp = 1.0f / dpToPx;
        View tabsView = cta.getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        mSwipeToIncognito =
                new SimulateTabSwipeOnMainThread(cta.getLayoutManager(), mTabsViewWidthDp - 20,
                        mTabsViewHeightDp / 2, SWIPE_TO_LEFT_DIRECTION * mTabsViewWidthDp, 0);
        mSwipeToNormal = new SimulateTabSwipeOnMainThread(cta.getLayoutManager(), 20,
                mTabsViewHeightDp / 2, SWIPE_TO_RIGHT_DIRECTION * mTabsViewWidthDp, 0);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getTabModelSelector().getModel(true).closeAllTabs();
            ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(null);
        });
    }

    private void enterTabSwitcher(ChromeTabbedActivity cta) {
        OverviewModeBehaviorWatcher showWatcher = TabUiTestHelper.createOverviewShowWatcher(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.findViewById(R.id.tab_switcher_button).performClick(); });
        showWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = M, message = "crbug.com/1081832")
    public void testStrip_updateWithAddition() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        // Unintentional tab creation will not trigger tab strip.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_EXTERNAL_APP);
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 2, 0);
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_LINK);
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 3, 0);

        // Intentional tab creation from toolbar menu will trigger tab strip.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, false);
        verifyShowingStrip(cta, false, 4);

        // Restart chrome to make the current tab strip session expire.
        cta = restartChrome();
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 4, 0);

        // Intentional tab creation from long-press context menu will trigger tab strip.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_LONGPRESS_BACKGROUND);
        verifyShowingStrip(cta, false, 5);

        // Restart chrome to make the current tab strip session expire.
        cta = restartChrome();
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 5, 0);

        // Intentional tab creation from long-press tab switcher action menu will trigger tab strip.
        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        onView(withText(R.string.menu_new_tab)).perform(click());
        verifyShowingStrip(cta, false, 6);

        // When tab strip is already showing, both intentional and unintentional tab creation should
        // trigger tab strip update.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_EXTERNAL_APP);
        verifyShowingStrip(cta, false, 7);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, false);
        verifyShowingStrip(cta, false, 8);
    }

    @Test
    @MediumTest
    public void testStrip_updateWithClosure() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);

        // Close one tab in tab switcher, check the update is reflected in tab strip.
        enterTabSwitcher(cta);
        closeStackTabAtIndex(cta, 2, false);
        clickOnStackTabAtIndex(1, false);
        verifyShowingStrip(cta, false, 2);

        // Close another tab in tab switcher. Since there is only tab left in current tab model,
        // the strip should be hidden.
        enterTabSwitcher(cta);
        closeStackTabAtIndex(cta, 1, false);
        clickOnStackTabAtIndex(0, false);
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_updateWithSelection() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Restart chrome to make the current tab strip session expire.
        cta = restartChrome();
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 4, 0);

        // Tab selection through tab switcher should trigger tab strip, and tab selection will be
        // reflected in tab strip.
        for (int i = 0; i < 4; i++) {
            enterTabSwitcher(cta);
            verifyHidingStrip();
            clickOnStackTabAtIndex(i, false);
            verifyShowingStrip(cta, false, 4);
            verifyStripSelectedPosition(cta, i);
        }
    }

    @Test
    @MediumTest
    public void testStrip_updateWithTabModelSwitch() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);
        createTabs(cta, true, 2);
        verifyShowingStrip(cta, true, 2);
        verifyTabModelTabCount(cta, 3, 2);
        assertTrue(cta.getTabModelSelector().isIncognitoSelected());

        // Switch tab model through tab switcher.
        enterTabSwitcher(cta);
        verifyHidingStrip();
        switchTabModel(cta, false);
        verifyHidingStrip();
        clickOnStackTabAtIndex(0, false);
        verifyShowingStrip(cta, false, 3);

        // Switch tab model through creating new tabs.
        createTabs(cta, true, 2);
        verifyTabModelTabCount(cta, 3, 4);
        verifyShowingStrip(cta, true, 4);
        createTabs(cta, false, 3);
        verifyTabModelTabCount(cta, 5, 4);
        verifyShowingStrip(cta, false, 5);
    }

    @Test
    @MediumTest
    public void testStrip_createTabWithStrip() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        // Test creating normal tabs by clicking the plus button in tab strip.
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);
        int normalTabCount = 3;
        for (int i = 0; i < 3; i++) {
            clickPlusButtonOnStrip();
            verifyTabModelTabCount(cta, ++normalTabCount, 0);
            verifyShowingStrip(cta, false, normalTabCount);
        }

        // Switch to incognito tab model.
        createTabs(cta, true, 1);
        // Strip should not be showing when there is only one tab in current model.
        verifyHidingStrip();
        createTabs(cta, true, 1);
        verifyShowingStrip(cta, true, 2);

        // Test creating incognito tabs by clicking the plus button in tab strip.
        int incognitoTabCount = 2;
        for (int i = 0; i < 3; i++) {
            clickPlusButtonOnStrip();
            verifyTabModelTabCount(cta, normalTabCount, ++incognitoTabCount);
            verifyShowingStrip(cta, true, incognitoTabCount);
        }
    }

    @Test
    @MediumTest
    public void testStrip_switchTabWithStrip() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 4);
        verifyShowingStrip(cta, false, 4);
        verifyStripSelectedPosition(cta, 3);

        // Switching between tabs by clicking on favicon.
        for (int i = 0; i < 4; i++) {
            clickNthItemInStrip(i);
            verifyStripSelectedPosition(cta, i);
        }
    }

    @Test
    @MediumTest
    public void testStrip_closeTabWithStrip() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        createTabs(cta, false, 2);
        verifyShowingStrip(cta, false, 2);
        verifyStripSelectedPosition(cta, 1);

        // Click the selected item to close the last tab, and the strip should be hidden
        // after closure.
        clickNthItemInStrip(1);
        verifyHidingStrip();
        verifyTabModelTabCount(cta, 1, 0);

        // Click undo to bring back the last tab, and should bring back the tab strip as
        // well. Also, the tab whose closure is undone should be selected.
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyShowingStrip(cta, false, 2);
        verifyTabModelTabCount(cta, 2, 0);
        verifyStripSelectedPosition(cta, 1);

        // Disable undo snackbar and test continuous closures.
        cta.getSnackbarManager().disableForTesting();
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 4);
        int tabCount = 4;
        for (int i = 3; i > 0; i--) {
            verifyStripSelectedPosition(cta, i);
            clickNthItemInStrip(i);
            verifyTabModelTabCount(cta, --tabCount, 0);
            if (i == 1) {
                // Tab strip will be hidden when there is only one tab in current model.
                verifyHidingStrip();
            } else {
                verifyShowingStrip(cta, false, tabCount);
            }
        }
    }

    @Test
    @MediumTest
    public void testStrip_dismiss() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        verifyHidingStrip();

        triggerStripAndDismiss(cta);

        // Tab strip should keep hidden throughout this session.
        enterTabSwitcher(cta);
        clickOnStackTabAtIndex(0, false);
        verifyHidingStrip();
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyHidingStrip();
        createTabs(cta, true, 2);
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_disabled_expired() throws Exception {
        triggerStripAndDismiss(sActivityTestRule.getActivity());

        ChromeTabbedActivity cta = restartChrome();
        verifyHidingStrip();

        createTabs(cta, false, 2);
        verifyShowingStrip(cta, false, cta.getCurrentTabModel().getCount());
    }

    @Test
    @MediumTest
    public void testStrip_disabled_notExpired() throws Exception {
        triggerStripAndDismiss(sActivityTestRule.getActivity());

        // Update the session time so that the disabled state is not expired for next restart.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(TEST_SESSION_MS);
        ChromeTabbedActivity cta = restartChrome();
        verifyHidingStrip();

        createTabs(cta, false, 2);
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_enabled_expired() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        restartChrome();
        verifyHidingStrip();
    }

    @Test
    @MediumTest
    public void testStrip_enabled_notExpired() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Update the session time so that the disabled state is not expired for next restart.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(TEST_SESSION_MS);
        cta = restartChrome();
        verifyShowingStrip(cta, false, 4);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1081697")
    public void testStrip_UndoDismiss() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Dismiss the strip, and then click on the undo snack bar to bring the strip back.
        clickDismissButtonInStrip();
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyShowingStrip(cta, false, 4);

        // Update the session time so that the enabled state is not expired for next restart. Verify
        // that the undo correctly updated the feature status to enabled.
        CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.setForTesting(TEST_SESSION_MS);
        cta = restartChrome();
        verifyShowingStrip(cta, false, 4);
    }

    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1094998")
    public void testStrip_InfoBarOptOut() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Initially, the continuous dismiss counter is 0, and dismiss the strip should show undo
        // snackbar.
        assertEquals(0, ConditionalTabStripUtils.getContinuousDismissCount());
        assertTrue(ConditionalTabStripUtils.shouldShowSnackbarForDismissal());
        clickDismissButtonInStrip();
        CriteriaHelper.pollUiThread(
                () -> sActivityTestRule.getActivity().getSnackbarManager().isShowing());

        // Update the dismiss counter so that the next dismissal should be the third continuous
        // dismissal, and we should show opt-out info bar.
        ConditionalTabStripUtils.setContinuousDismissCount(1);
        cta = restartChrome();
        assertEquals(2, ConditionalTabStripUtils.getContinuousDismissCount());
        assertFalse(ConditionalTabStripUtils.shouldShowSnackbarForDismissal());
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyShowingStrip(cta, false, 5);
        clickDismissButtonInStrip();
        verifyShowingInfoBar(cta);

        // Click the opt-out button should have the feature disabled.
        assertFalse(ConditionalTabStripUtils.getOptOutIndicator());
        onView(withId(R.id.button_secondary)).perform(click());
        assertTrue(ConditionalTabStripUtils.getOptOutIndicator());
        assertEquals(-1, ConditionalTabStripUtils.getContinuousDismissCount());
        int oldTabStripPermanentlyHiddenCount = RecordHistogram.getHistogramValueCountForTesting(
                ConditionalTabStripUtils.UMA_USER_STATUS_RESULT,
                ConditionalTabStripUtils.UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN);
        cta = restartChrome();
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(doesNotExist());
        int currentTabStripPermanentlyHiddenCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        ConditionalTabStripUtils.UMA_USER_STATUS_RESULT,
                        ConditionalTabStripUtils.UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN);
        assertEquals(1, currentTabStripPermanentlyHiddenCount - oldTabStripPermanentlyHiddenCount);
    }

    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1094998")
    public void testStrip_InfoBarOptIn() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        for (int i = 0; i < 3; i++) {
            createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        }
        verifyShowingStrip(cta, false, 4);

        // Initially, the continuous dismiss counter is 0, and dismiss the strip should show undo
        // snackbar.
        assertEquals(0, ConditionalTabStripUtils.getContinuousDismissCount());
        assertTrue(ConditionalTabStripUtils.shouldShowSnackbarForDismissal());
        clickDismissButtonInStrip();
        CriteriaHelper.pollUiThread(
                () -> sActivityTestRule.getActivity().getSnackbarManager().isShowing());

        // Update the dismiss counter so that the next dismissal should be the third continuous
        // dismissal, and we should show opt-out info bar.
        ConditionalTabStripUtils.setContinuousDismissCount(1);
        cta = restartChrome();
        assertEquals(2, ConditionalTabStripUtils.getContinuousDismissCount());
        assertFalse(ConditionalTabStripUtils.shouldShowSnackbarForDismissal());
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyShowingStrip(cta, false, 5);
        clickDismissButtonInStrip();
        verifyShowingInfoBar(cta);

        // Click the opt-in button should set the dismiss counter to -1.
        onView(withId(R.id.button_primary)).perform(click());
        assertEquals(-1, ConditionalTabStripUtils.getContinuousDismissCount());

        // Once the counter is set to -1, the counter should no longer be updated by later
        // dismissals.
        cta = restartChrome();
        assertEquals(-1, ConditionalTabStripUtils.getContinuousDismissCount());
        assertTrue(ConditionalTabStripUtils.shouldShowSnackbarForDismissal());
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyShowingStrip(cta, false, 6);
        clickDismissButtonInStrip();
        CriteriaHelper.pollUiThread(
                () -> sActivityTestRule.getActivity().getSnackbarManager().isShowing());
    }

    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1094998")
    public void testStrip_ContinuousDismissCounter() throws Exception {
        // Mock that the tab strip has been dismissed for two continuous sessions.
        ConditionalTabStripUtils.setContinuousDismissCount(2);

        // Since strip is not activated in current session, the counter will not be updated.
        assertEquals(ConditionalTabStripUtils.FeatureStatus.DEFAULT,
                ConditionalTabStripUtils.getFeatureStatus());
        ChromeTabbedActivity cta = restartChrome();
        assertEquals(2, ConditionalTabStripUtils.getContinuousDismissCount());

        // Since the strip was triggered in current session, the counter will be reset to 0 in the
        // next session.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyShowingStrip(cta, false, 2);
        assertEquals(ConditionalTabStripUtils.FeatureStatus.ACTIVATED,
                ConditionalTabStripUtils.getFeatureStatus());
        cta = restartChrome();
        assertEquals(0, ConditionalTabStripUtils.getContinuousDismissCount());

        // Dismiss the strip in current session.
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyShowingStrip(cta, false, 3);
        clickDismissButtonInStrip();
        CriteriaHelper.pollUiThread(
                () -> sActivityTestRule.getActivity().getSnackbarManager().isShowing());

        // Update the dismiss counter so that the next dismissal should be the sixth continuous
        // dismissal, and we should show opt-out info bar.
        ConditionalTabStripUtils.setContinuousDismissCount(4);
        cta = restartChrome();
        assertEquals(5, ConditionalTabStripUtils.getContinuousDismissCount());
        createBlankPageWithLaunchType(cta, false, TabLaunchType.FROM_CHROME_UI);
        verifyShowingStrip(cta, false, 4);
        clickDismissButtonInStrip();
        verifyShowingInfoBar(cta);

        // Click the info bar close button to dismiss the info bar.
        onView(withId(R.id.infobar_close_button)).perform(click());
        CriteriaHelper.pollUiThread(() -> {
            InfoBarContainer container = InfoBarContainer.get(
                    sActivityTestRule.getActivity().getTabModelSelector().getCurrentTab());
            return container.getInfoBarsForTesting().size() == 0;
        });

        // We no longer keep the dismiss counter when user has dismissed the strip continuously for
        // 6 sessions, i.e. dismiss the opt-out info bar twice.
        restartChrome();
        assertEquals(-1, ConditionalTabStripUtils.getContinuousDismissCount());
    }

    @Test
    @MediumTest
    public void testUndoClosure_AccessibilityMode() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        SnackbarManager snackbarManager = sActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, 3);
        verifyStripSelectedPosition(cta, 2);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());

        // Click the selected item in strip to close a tab. The undo snack bar should show, and
        // clicking on the snack bar button should undo the closure.
        clickNthItemInStrip(2);
        verifyShowingStrip(cta, false, 2);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoBarController);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyShowingStrip(cta, false, 3);
        verifyStripSelectedPosition(cta, 2);

        // The undo snack bar should still work after entering overview mode.
        clickNthItemInStrip(2);
        verifyShowingStrip(cta, false, 2);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoBarController);
        enterTabSwitcher(cta);
        verifyHidingStrip();
        assertNotNull(snackbarManager.getCurrentSnackbarForTesting());
        assertEquals(3, getAccessibilityOverviewList().getCount());
        verifyAccessibilityTabClosing(2, true);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyAccessibilityTabClosing(2, false);

        // The undo snack bar should not show when closure happens in accessibility tab switcher.
        AccessibilityTabModelListItem item = getAccessibilityOverviewListItem(0);
        verifyAccessibilityTabClosing(0, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> item.findViewById(R.id.end_button).performClick());
        assertNull(snackbarManager.getCurrentSnackbarForTesting());
        verifyAccessibilityTabClosing(0, true);
        assertEquals(3, getAccessibilityOverviewList().getCount());
    }

    private void verifyAccessibilityTabClosing(int index, boolean isClosing) {
        CriteriaHelper.pollUiThread(() -> {
            AccessibilityTabModelListItem item = getAccessibilityOverviewListItem(index);
            Criteria.checkThat(item.findViewById(R.id.undo_contents).getVisibility(),
                    Matchers.is(isClosing ? View.VISIBLE : View.INVISIBLE));
        });
    }

    private ChromeTabbedActivity restartChrome() throws Exception {
        TabUiTestHelper.finishActivity(sActivityTestRule.getActivity());
        sActivityTestRule.startMainActivityFromLauncher();
        // Wait for bottom controls to stabilize.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(sActivityTestRule.getActivity()
                                       .getBrowserControlsManager()
                                       .getBottomControlOffset(),
                    Matchers.is(0));
        });
        return sActivityTestRule.getActivity();
    }

    private void createBlankPageWithLaunchType(ChromeTabbedActivity cta, boolean isIncognito,
            @TabLaunchType int type) throws ExecutionException {
        TabCreator tabCreator = cta.getTabCreator(isIncognito);
        LoadUrlParams loadUrlParams = new LoadUrlParams(UrlConstants.CHROME_BLANK_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tabCreator.createNewTab(loadUrlParams, type, null));
    }

    private void verifyHidingStrip() {
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(not(isDisplayed())));
    }

    private void verifyShowingStrip(ChromeTabbedActivity cta, boolean isIncognito, int tabCount) {
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()));
        verifyTabStripFaviconCount(cta, tabCount);
        TabModel tabModel = cta.getTabModelSelector().getModel(isIncognito);
        assertEquals(isIncognito, cta.getTabModelSelector().isIncognitoSelected());
        assertEquals(tabCount, tabModel.getCount());
    }

    private void switchTabModel(ChromeTabbedActivity cta, boolean isIncognito) {
        assertTrue(cta.getOverviewModeBehavior().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(isIncognito ? mSwipeToIncognito : mSwipeToNormal);
        // Wait until the target stack is visible.
        Stack stack = getStack(cta.getLayoutManager(), isIncognito);
        LayoutTab layoutTab = stack.getTabs()[0].getLayoutTab();
        CriteriaHelper.pollUiThread(
                () -> layoutTab.getX() > 0 && layoutTab.getX() < mTabsViewWidthDp);
    }

    private void clickOnStackTabAtIndex(int index, boolean isIncognito) {
        LayoutManagerChrome layoutManager = updateTabsViewSize();
        float[] coordinates = getStackTabClickTarget(index, isIncognito);
        float clickX = coordinates[0];
        float clickY = coordinates[1];

        OverviewModeBehaviorWatcher overviewModeWatcher =
                new OverviewModeBehaviorWatcher(layoutManager, false, true);

        TestThreadUtils.runOnUiThreadBlocking(
                new SimulateClickOnMainThread(layoutManager, (int) clickX, (int) clickY));
        overviewModeWatcher.waitForBehavior();
    }

    private void closeStackTabAtIndex(ChromeTabbedActivity cta, int index, boolean isIncognito) {
        LayoutManagerChrome layoutManager = updateTabsViewSize();
        StackLayout layout = (StackLayout) layoutManager.getOverviewLayout();
        Stack stack = layout.getTabStackAtIndex(
                isIncognito ? StackLayout.INCOGNITO_STACK_INDEX : StackLayout.NORMAL_STACK_INDEX);
        assertTrue(
                "try to close tab at invalid index", index < stack.getTabs().length && index >= 0);
        LayoutTab layoutTab = stack.getTabs()[index].getLayoutTab();
        float x = stack.getCloseBoundsOnLayoutTab(layoutTab).centerX();
        float y = stack.getCloseBoundsOnLayoutTab(layoutTab).centerY();
        ChromeTabUtils.closeTabWithAction(InstrumentationRegistry.getInstrumentation(), cta,
                ()
                        -> TestThreadUtils.runOnUiThreadBlocking(
                                new SimulateClickOnMainThread(layoutManager, x, y)));
    }

    private void verifyStripSelectedPosition(ChromeTabbedActivity cta, int index) {
        assertEquals(cta.getCurrentTabModel().index(), index);
        // Since View.getForeground() is not supported in 23-, there is not good way for us to
        // check the selected item from the perspective of Android View. Therefore, skip this check
        // for API below 23.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            return;
        }
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()))
                .check((v, e) -> {
                    RecyclerView recyclerView = (RecyclerView) v;
                    RecyclerView.Adapter adapter = recyclerView.getAdapter();
                    for (int i = 0; i < adapter.getItemCount(); i++) {
                        View itemView = recyclerView.findViewHolderForAdapterPosition(i).itemView;
                        if (itemView.getForeground() != null) {
                            assertEquals(index, i);
                        }
                    }
                });
    }

    private void clickPlusButtonOnStrip() {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.toolbar_right_button)))
                .perform(click());
    }

    private void clickNthItemInStrip(int index) {
        onView(allOf(withParent(withId(R.id.toolbar_container_view)), withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()))
                .perform(RecyclerViewActions.actionOnItemAtPosition(index, click()));
    }

    private void clickDismissButtonInStrip() {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.toolbar_left_button)))
                .perform(click());
    }

    private void triggerStripAndDismiss(ChromeTabbedActivity cta) {
        int normalTabCount = cta.getTabModelSelector().getModel(false).getCount();
        createTabs(cta, false, 3);
        verifyShowingStrip(cta, false, normalTabCount + 2);

        // Click the left button should dismiss the tab strip.
        clickDismissButtonInStrip();
        verifyHidingStrip();
    }

    private void verifyShowingInfoBar(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(() -> {
            InfoBarContainer container =
                    InfoBarContainer.get(cta.getTabModelSelector().getCurrentTab());
            return container.getVisibility() == View.VISIBLE && !container.isAnimating();
        });
        Context context = (Context) cta;
        onView(withId(R.id.infobar_message))
                .check(matches(withText(
                        containsString(context.getString(R.string.tab_strip_info_bar_question)))));
        onView(withId(R.id.button_primary))
                .check(matches(withText(
                        containsString(context.getString(R.string.tab_strip_info_bar_reshow)))));
        onView(withId(R.id.button_secondary))
                .check(matches(withText(
                        containsString(context.getString(R.string.tab_strip_info_bar_no_reshow)))));
    }

    // Utility methods copied from TabsTest.java.
    // TODO(yuezhanggg): Pull out these methods into a separate utility class and share them with
    // TabsTest.
    private float[] getStackTabClickTarget(final int tabIndexToSelect, final boolean isIncognito) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final float[] target = new float[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Stack stack = getStack(layoutManager, isIncognito);
            StackTab[] tabs = stack.getTabs();
            // The position of the click is expressed from the top left corner of the content.
            // The aim is to find an offset that is inside the content but not on the close
            // button.  For this, we calculate the center of the visible tab area.
            LayoutTab layoutTab = tabs[tabIndexToSelect].getLayoutTab();
            LayoutTab nextLayoutTab = (tabIndexToSelect + 1) < tabs.length
                    ? tabs[tabIndexToSelect + 1].getLayoutTab()
                    : null;

            float tabOffsetX = layoutTab.getX();
            float tabOffsetY = layoutTab.getY();
            float tabRightX = tabOffsetX + layoutTab.getScaledContentWidth();
            float tabBottomY = nextLayoutTab != null
                    ? nextLayoutTab.getY()
                    : tabOffsetY + layoutTab.getScaledContentHeight();
            tabRightX = Math.min(tabRightX, mTabsViewWidthDp);
            tabBottomY = Math.min(tabBottomY, mTabsViewHeightDp);

            target[0] = (tabOffsetX + tabRightX) / 2.0f;
            target[1] = (tabOffsetY + tabBottomY) / 2.0f;
        });
        return target;
    }

    private Stack getStack(final LayoutManagerChrome layoutManager, boolean isIncognito) {
        LayoutManagerChromePhone layoutManagerPhone = (LayoutManagerChromePhone) layoutManager;
        StackLayout layout = (StackLayout) layoutManagerPhone.getOverviewLayout();
        return (layout).getTabStackAtIndex(
                isIncognito ? StackLayout.INCOGNITO_STACK_INDEX : StackLayout.NORMAL_STACK_INDEX);
    }

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = sActivityTestRule.getActivity().getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return sActivityTestRule.getActivity().getLayoutManager();
    }

    // Utility methods from OverviewListLayoutTest.java.
    private ListView getAccessibilityOverviewList() {
        AccessibilityTabModelWrapper container =
                ((OverviewListLayout) sActivityTestRule.getActivity().getOverviewListLayout())
                        .getContainer();
        return (ListView) container.findViewById(R.id.list_view);
    }

    private AccessibilityTabModelListItem getAccessibilityOverviewListItem(int index) {
        return (AccessibilityTabModelListItem) getAccessibilityOverviewList().getChildAt(index);
    }
}
