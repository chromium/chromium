// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_tab_switcher;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.ListView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.android.material.tabs.TabLayout;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests accessibility UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OverviewListLayoutTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String PAGE_1_HTML = "data:text/html;charset=utf-8,"
            + "<html><head><title>Page%201<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";
    private static final String PAGE_2_HTML = "data:text/html;charset=utf-8,"
            + "<html><head><title>Page%202<%2Ftitle><%2Fhead><body><%2Fbody><%2Fhtml>";

    private static final int SWIPE_START_X_OFFSET = 10;
    private static final int SWIPE_START_Y_OFFSET = 10;
    private static final int SWIPE_END_X = 20;

    private void verifyChildCount(int count) {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getList().getChildCount(), Matchers.is(count)));
    }

    private void verifyTabModelCount(boolean incognito, int count) {
        CriteriaHelper.pollUiThread(() -> {
            int actualCount = mActivityTestRule.getActivity()
                                      .getTabModelSelector()
                                      .getModel(incognito)
                                      .getCount();
            Criteria.checkThat(actualCount, Matchers.is(count));
        });
    }

    @Before
    public void setUp() {
        CommandLine.getInstance().appendSwitch(ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER);
        mActivityTestRule.startMainActivityFromLauncher();
    }

    private AccessibilityTabModelWrapper getContainer() {
        return ((OverviewListLayout) mActivityTestRule.getActivity().getOverviewListLayout())
                .getContainer();
    }

    private ListView getList() {
        return (ListView) getContainer().findViewById(R.id.list_view);
    }

    private void setupTabs() {
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 3);

        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));

        verifyChildCount(4);
    }

    private AccessibilityTabModelListItem getListItemAndDisableAnimations(int index) {
        AccessibilityTabModelListItem item = getListItem(index);
        item.disableAnimations();
        return item;
    }

    private AccessibilityTabModelListItem getListItem(int index) {
        AccessibilityTabModelListItem item =
                (AccessibilityTabModelListItem) getList().getChildAt(index);
        return item;
    }

    private CharSequence getTabTitleOfListItem(int index) {
        View childView = getListItem(index);
        TextView childTextView;
        childTextView = (TextView) childView.findViewById(org.chromium.chrome.R.id.title);
        return childTextView.getText();
    }

    private void toggleTabSwitcher(final boolean expectVisible) {
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        CriteriaHelper.pollUiThread(() -> {
            if (expectVisible) {
                Criteria.checkThat(getContainer(), Matchers.notNullValue());
                Criteria.checkThat(getContainer().getParent(), Matchers.notNullValue());
            } else {
                if (getContainer() == null) return;
                Criteria.checkThat(getContainer().getParent(), Matchers.nullValue());
            }
        });
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanEnterSwitcher() {
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));

        Assert.assertNotNull("Accessibility container was not initialized", getContainer());
        Assert.assertNotNull("Accessibility container was not visible", getContainer().getParent());
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanLeaveSwitcher() {
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));

        Assert.assertNotNull("Accessibility container was not initialized", getContainer());
        Assert.assertNotNull("Accessibility container was not visible", getContainer().getParent());

        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        Assert.assertNull("Accessibility container was not visible", getContainer().getParent());
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanCloseWithCloseButton() throws TimeoutException {
        setupTabs();

        AccessibilityTabModelListItem item = getListItemAndDisableAnimations(0);

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        final TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), item.getCloseButtonForTests());

        didReceiveClosureCommittedHelper.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertEquals("Tab not closed", 3, getList().getChildCount()); });
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCanSwipeClosed() throws TimeoutException {
        setupTabs();

        AccessibilityTabModelListItem item = getListItemAndDisableAnimations(1);

        int[] location = TestTouchUtils.getAbsoluteLocationFromRelative(
                item, SWIPE_START_X_OFFSET, SWIPE_START_Y_OFFSET);

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        final TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(mActivityTestRule.getActivity(), location[0], location[1], downTime);
        TouchCommon.dragTo(mActivityTestRule.getActivity(), location[0],
                (int) (item.getWidth() / 1.5), location[1], location[1], 20, downTime);
        TouchCommon.dragEnd(mActivityTestRule.getActivity(), 400, location[1], downTime);

        didReceiveClosureCommittedHelper.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertEquals("Tab not closed", 3, getList().getChildCount()); });
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testResetSwipe() {
        setupTabs();

        AccessibilityTabModelListItem item = getListItemAndDisableAnimations(0);

        int[] location = TestTouchUtils.getAbsoluteLocationFromRelative(
                item, SWIPE_START_X_OFFSET, SWIPE_START_Y_OFFSET);

        TouchCommon.dragTo(mActivityTestRule.getActivity(), location[0], SWIPE_END_X, location[1],
                location[1], 20, SystemClock.uptimeMillis());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Tab not reset as expected", 4, getList().getChildCount());
        });
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseAndUndo() throws TimeoutException {
        setupTabs();

        final AccessibilityTabModelListItem item = getListItem(0);

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        final TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabPendingClosure(Tab tab) {
                didReceivePendingClosureHelper.notifyCalled();
            }
        });

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), item.getCloseButtonForTests());

        didReceivePendingClosureHelper.waitForCallback(0);

        TestTouchUtils.performClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), item.findViewById(R.id.undo_button));

        Assert.assertFalse("Close not undone", item.hasPendingClosure());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Tab closure not undone", 4, getList().getChildCount());
        });
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseAll() {
        setupTabs();

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.close_all_tabs_menu_id);

        verifyChildCount(0);
        verifyTabModelCount(false, 0);
        Assert.assertFalse(mActivityTestRule.getActivity()
                                   .findViewById(R.id.tab_switcher_mode_tab_switcher_button)
                                   .isEnabled());
        Assert.assertTrue(
                mActivityTestRule.getActivity().findViewById(R.id.new_tab_button).isEnabled());
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseAllIncognito() {
        setupTabs();
        mActivityTestRule.newIncognitoTabsFromMenu(2);
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));
        verifyChildCount(2);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.close_all_incognito_tabs_menu_id);
        verifyTabModelCount(true, 0);

        verifyChildCount(4);
        Assert.assertTrue(mActivityTestRule.getActivity()
                                  .findViewById(R.id.tab_switcher_mode_tab_switcher_button)
                                  .isEnabled());

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.close_all_tabs_menu_id);

        verifyChildCount(0);
        verifyTabModelCount(false, 0);
        Assert.assertFalse(mActivityTestRule.getActivity()
                                   .findViewById(R.id.tab_switcher_mode_tab_switcher_button)
                                   .isEnabled());
        Assert.assertTrue(
                mActivityTestRule.getActivity().findViewById(R.id.new_tab_button).isEnabled());
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testModelSwitcherVisibility() {
        setupTabs();

        View switcherButtons = getContainer().findViewById(R.id.tab_wrapper);

        Assert.assertEquals(
                "Tab Model Switcher buttons visible", View.GONE, switcherButtons.getVisibility());

        mActivityTestRule.newIncognitoTabsFromMenu(2);

        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));

        Assert.assertEquals("Tab Model switcher buttons not visible", View.VISIBLE,
                switcherButtons.getVisibility());
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @MediumTest
    @Feature({"Accessibility"})
    public void testModelSwitcherFunctionality() {
        mActivityTestRule.newIncognitoTabsFromMenu(2);

        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 3);

        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button));

        View switcherButtons = getContainer().findViewById(R.id.tab_wrapper);

        Assert.assertEquals("Tab Model Switcher buttons visible", View.VISIBLE,
                switcherButtons.getVisibility());

        TabLayout.Tab incognitoButton = getContainer().getIncognitoTabsButton();

        Assert.assertNotNull("IncognitoButton is null", incognitoButton);

        TestThreadUtils.runOnUiThreadBlocking(() -> incognitoButton.select());

        verifyChildCount(2);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> getContainer().getStandardTabsButton().select());

        verifyChildCount(4);
    }

    /**
     * Tests that the TabObserver of the {@link AccessibilityTabModelListItem} is added back
     * to the Tab after the View is hidden.  This requires bringing the tab switcher back twice
     * because the TabObserver is removed/added when the tab switcher's View is detached from/
     * attached to the window.
     */
    @Test
    @MediumTest
    @Feature({"Accessibility"})
    public void testObservesTitleChanges() throws Exception {
        mActivityTestRule.loadUrl(PAGE_1_HTML);

        // Bring the tab switcher forward and send it away twice.
        toggleTabSwitcher(true);
        Assert.assertEquals("Page 1", getTabTitleOfListItem(0));
        toggleTabSwitcher(false);
        toggleTabSwitcher(true);
        toggleTabSwitcher(false);

        // Load another URL.
        new TabLoadObserver(mActivityTestRule.getActivity().getActivityTab())
                .fullyLoadUrl(PAGE_2_HTML);

        // Bring the tab switcher forward and check the title.
        toggleTabSwitcher(true);
        Assert.assertEquals("Page 2", getTabTitleOfListItem(0));
    }

    @Test
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @MediumTest
    @Feature({"Accessibility"})
    public void testCloseTabThroughTabStrip() throws TimeoutException {
        setupTabs();

        getListItemAndDisableAnimations(0);
        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        final TabModel model = mActivityTestRule.getActivity().getCurrentTabModel();
        model.addObserver(new TabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                mActivityTestRule.getActivity(), false, model.getTabAt(0).getId());
        TabStripUtils.clickCompositorButton(tab.getCloseButton(),
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        didReceiveClosureCommittedHelper.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertEquals("Tab not closed", 3, getList().getChildCount()); });
    }
}
