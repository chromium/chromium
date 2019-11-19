// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link ChromeActivity}'s {@link ActivityTabProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ActivityTabProviderTest {
    /** A test observer that provides access to the tab being observed. */
    private static class TestActivityTabTabObserver extends ActivityTabTabObserver {
        /** Callback helper for notification that the observer is watching a different tab. */
        private CallbackHelper mObserverMoveHelper;

        /** The tab currently being observed. */
        private Tab mObservedTab;

        public TestActivityTabTabObserver(ActivityTabProvider provider) {
            super(provider);
            mObserverMoveHelper = new CallbackHelper();
            mObservedTab = provider.get();
        }

        @Override
        public void onObservingDifferentTab(Tab tab) {
            mObservedTab = tab;
            mObserverMoveHelper.notifyCalled();
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mActivity;
    private ActivityTabProvider mProvider;
    private Tab mActivityTab;
    private CallbackHelper mActivityTabChangedHelper = new CallbackHelper();
    private CallbackHelper mActivityTabChangedHintHelper = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mProvider = mActivity.getActivityTabProvider();
        mProvider.addObserverAndTrigger((tab, hint) -> {
            if (hint) {
                mActivityTabChangedHintHelper.notifyCalled();
            } else {
                mActivityTab = tab;
                mActivityTabChangedHelper.notifyCalled();
            }
        });
        assertEquals("Setup should have only triggered the event once.",
                mActivityTabChangedHelper.getCallCount(), 1);
    }

    /**
     * @return The {@link Tab} that the active model currently has selected.
     */
    private Tab getModelSelectedTab() {
        return mActivity.getTabModelSelector().getCurrentTab();
    }

    /**
     * Test that the onActivityTabChanged event is triggered when the observer is attached for
     * only that observer.
     */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testTriggerOnAddObserver() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        mProvider.addObserverAndTrigger((tab, hint) -> helper.notifyCalled());
        helper.waitForCallback(0);

        assertEquals("Only the added observer should have been triggered.",
                mActivityTabChangedHelper.getCallCount(), 1);
        assertEquals("The added observer should have only been triggered once.", 1,
                helper.getCallCount());
    }

    /** Test that onActivityTabChanged is triggered when entering and exiting the tab switcher. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTriggerWithTabSwitcher() throws TimeoutException {
        assertEquals("The activity tab should be the model's selected tab.", getModelSelectedTab(),
                mActivityTab);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showOverview(false));
        mActivityTabChangedHelper.waitForCallback(1);
        assertEquals("Entering the tab switcher should have triggered the event once.", 2,
                mActivityTabChangedHelper.getCallCount());
        assertEquals("The activity tab should be null.", null, mActivityTab);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().hideOverview(false));
        mActivityTabChangedHelper.waitForCallback(2);
        assertEquals("Exiting the tab switcher should have triggered the event once.", 3,
                mActivityTabChangedHelper.getCallCount());
        assertEquals("The activity tab should be the model's selected tab.", getModelSelectedTab(),
                mActivityTab);
    }

    /** Test that the hint event triggers when exiting the tab switcher. */
    @Test
    @LargeTest
    @Feature({"ActivityTabObserver"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTriggerHintWithTabSwitcher() throws TimeoutException {
        assertEquals("The hint should not yet have triggered.", 0,
                mActivityTabChangedHintHelper.getCallCount());

        setTabSwitcherModeAndWait(true);

        assertEquals("The hint should not yet have triggered.", 0,
                mActivityTabChangedHintHelper.getCallCount());

        setTabSwitcherModeAndWait(false);
        mActivityTabChangedHintHelper.waitForCallback(0);

        assertEquals("The hint should have triggerd once.", 1,
                mActivityTabChangedHintHelper.getCallCount());
    }

    /**
     * Test that onActivityTabChanged is triggered when switching to a new tab without switching
     * layouts.
     */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testTriggerWithTabSelection() throws TimeoutException {
        Tab startingTab = getModelSelectedTab();

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(), mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, false);

        assertNotEquals(
                "A new tab should be in the foreground.", startingTab, getModelSelectedTab());
        assertEquals("The activity tab should be the model's selected tab.", getModelSelectedTab(),
                mActivityTab);

        int callCount = mActivityTabChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Select the original tab without switching layouts.
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    0, TabSelectionType.FROM_USER);
        });
        mActivityTabChangedHelper.waitForCallback(callCount);

        assertEquals("Switching tabs should have triggered the event once.", callCount + 1,
                mActivityTabChangedHelper.getCallCount());
    }

    /** Test that onActivityTabChanged is triggered when the last tab is closed. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testTriggerOnLastTabClosed() throws TimeoutException {
        int callCount = mActivityTabChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTabModelSelector().closeTab(getModelSelectedTab()); });
        mActivityTabChangedHelper.waitForCallback(callCount);

        assertEquals("Closing the last tab should have triggered the event once.", callCount + 1,
                mActivityTabChangedHelper.getCallCount());
        assertEquals("The activity's tab should be null.", null, mActivityTab);
    }

    /**
     * Test that the correct tab is considered the activity tab when a different tab is closed on
     * phone.
     */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testCorrectTabAfterTabClosed() {
        Tab startingTab = getModelSelectedTab();

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(), mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, false);

        assertNotEquals("The starting tab should not be the selected tab.", getModelSelectedTab(),
                startingTab);
        assertEquals("The activity tab should be the model's selected tab.", getModelSelectedTab(),
                mActivityTab);
        Tab activityTabBefore = mActivityTab;

        int callCount = mActivityTabChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTabModelSelector().closeTab(startingTab); });

        assertEquals("The activity tab should not have changed.", activityTabBefore, mActivityTab);
    }

    /** Test that the {@link ActivityTabTabObserver} switches between tabs as the tab changes. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testActivityTabTabObserver() {
        Tab startingTab = getModelSelectedTab();

        TestActivityTabTabObserver tabObserver = new TestActivityTabTabObserver(mProvider);

        assertEquals("The observer should be attached to the starting tab.", startingTab,
                tabObserver.mObservedTab);

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(), mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, false);

        assertNotEquals("The tab should have changed.", startingTab, getModelSelectedTab());
        assertEquals("The observer should be attached to the new tab.", getModelSelectedTab(),
                tabObserver.mObservedTab);

        tabObserver.destroy();
    }

    /**
     * Enter or exit the tab switcher with animations and wait for the scene to change.
     * @param inSwitcher Whether to enter or exit the tab switcher.
     */
    private void setTabSwitcherModeAndWait(boolean inSwitcher) throws TimeoutException {
        final CallbackHelper sceneChangeHelper = new CallbackHelper();
        SceneChangeObserver observer = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {}

            @Override
            public void onSceneChange(Layout layout) {
                sceneChangeHelper.notifyCalled();
            }
        };
        mActivity.getCompositorViewHolder().getLayoutManager().addSceneChangeObserver(observer);

        int sceneChangeCount = sceneChangeHelper.getCallCount();
        if (inSwitcher) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivity.getLayoutManager().showOverview(true));
        } else {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivity.getLayoutManager().hideOverview(true));
        }
        sceneChangeHelper.waitForCallback(sceneChangeCount);

        mActivity.getCompositorViewHolder().getLayoutManager().removeSceneChangeObserver(observer);
    }
}
