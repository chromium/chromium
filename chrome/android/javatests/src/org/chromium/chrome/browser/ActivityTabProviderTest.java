// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/** Tests for {@link ChromeActivity}'s {@link ActivityTabProvider}. */
@DoNotBatch(reason = "waitForActivityCompletelyLoaded is unhappy when batched - more work needed")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ActivityTabProviderTest {
    /** A test observer that provides access to the tab being observed. */
    private static class TestActivityTabTabObserver extends ActivityTabTabObserver {
        /** The tab currently being observed. */
        private Tab mObservedTab;

        public TestActivityTabTabObserver(ActivityTabProvider provider) {
            super(provider);
            TestThreadUtils.runOnUiThreadBlockingNoException(() -> mObservedTab = provider.get());
        }

        @Override
        public void onObservingDifferentTab(Tab tab, boolean hint) {
            mObservedTab = tab;
        }

        @Override
        protected void updateObservedTabToCurrent() {
            TestThreadUtils.runOnUiThreadBlocking(super::updateObservedTabToCurrent);
        }

        @Override
        protected void addObserverToTabSupplier() {
            TestThreadUtils.runOnUiThreadBlocking(super::addObserverToTabSupplier);
        }

        @Override
        protected void removeObserverFromTabSupplier() {
            TestThreadUtils.runOnUiThreadBlocking(super::removeObserverFromTabSupplier);
        }

        @Override
        public void destroy() {
            TestThreadUtils.runOnUiThreadBlocking(super::destroy);
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mActivity;
    private ActivityTabProvider mProvider;
    private Tab mActivityTab;
    private CallbackHelper mActivityTabChangedHelper = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity = mActivityTestRule.getActivity();
                    mProvider = mActivity.getActivityTabProvider();
                    mProvider.addObserver(
                            tab -> {
                                mActivityTab = tab;
                                mActivityTabChangedHelper.notifyCalled();
                            });
                });
        mActivityTabChangedHelper.waitForCallback(0);
        assertEquals(
                "Setup should have only triggered the event once.",
                1,
                mActivityTabChangedHelper.getCallCount());
    }

    /**
     * @return The {@link Tab} that the active model currently has selected.
     */
    private Tab getModelSelectedTab() {
        return mActivity.getTabModelSelector().getCurrentTab();
    }

    /**
     * Test that the onActivityTabChanged event is triggered when the observer is attached for only
     * that observer.
     */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testTriggerOnAddObserver() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProvider.addObserver(tab -> helper.notifyCalled());
                });
        helper.waitForCallback(0);

        assertEquals(
                "Only the added observer should have been triggered.",
                mActivityTabChangedHelper.getCallCount(),
                1);
        assertEquals(
                "The added observer should have only been triggered once.",
                1,
                helper.getCallCount());
    }

    /** Test that onActivityTabChanged is triggered when entering and exiting the tab switcher. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTriggerWithTabSwitcher() throws TimeoutException {
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.TAB_SWITCHER, false));
        mActivityTabChangedHelper.waitForCallback(1);
        assertEquals(
                "Entering the tab switcher should have triggered the event once.",
                2,
                mActivityTabChangedHelper.getCallCount());
        assertEquals("The activity tab should be null.", null, mActivityTab);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.BROWSING, false));
        mActivityTabChangedHelper.waitForCallback(2);
        assertEquals(
                "Exiting the tab switcher should have triggered the event once.",
                3,
                mActivityTabChangedHelper.getCallCount());
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);
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

        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                false);

        assertNotEquals(
                "A new tab should be in the foreground.", startingTab, getModelSelectedTab());
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);

        int callCount = mActivityTabChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Select the original tab without switching layouts.
                    mActivity
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(0, TabSelectionType.FROM_USER, false);
                });
        mActivityTabChangedHelper.waitForCallback(callCount);

        assertEquals(
                "Switching tabs should have triggered the event once.",
                callCount + 1,
                mActivityTabChangedHelper.getCallCount());
    }

    /** Test that onActivityTabChanged is triggered when the last tab is closed. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testTriggerOnLastTabClosed() throws TimeoutException {
        // Have a tab open in incognito model. This should not be in the way getting the event
        // triggered when closing the last tab in normal mode.
        TabModelSelector selector = mActivity.getTabModelSelector();
        TestThreadUtils.runOnUiThreadBlocking(() -> selector.selectModel(true));
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                true);
        TestThreadUtils.runOnUiThreadBlocking(() -> selector.selectModel(false));

        int callCount = mActivityTabChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.closeTab(getModelSelectedTab());
                });
        mActivityTabChangedHelper.waitForCallback(callCount);

        assertEquals(
                "Closing the last tab should have triggered the event once.",
                callCount + 1,
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

        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                false);

        assertNotEquals(
                "The starting tab should not be the selected tab.",
                getModelSelectedTab(),
                startingTab);
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);
        Tab activityTabBefore = mActivityTab;

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.getTabModelSelector().closeTab(startingTab);
                });

        assertEquals("The activity tab should not have changed.", activityTabBefore, mActivityTab);
    }

    /** Test that the {@link ActivityTabTabObserver} switches between tabs as the tab changes. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    public void testActivityTabTabObserver() throws TimeoutException {
        Tab startingTab = getModelSelectedTab();

        TestActivityTabTabObserver tabObserver = new TestActivityTabTabObserver(mProvider);

        assertEquals(
                "The observer should be attached to the starting tab.",
                startingTab,
                tabObserver.mObservedTab);

        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                false);

        assertNotEquals("The tab should have changed.", startingTab, getModelSelectedTab());
        assertEquals(
                "The observer should be attached to the new tab.",
                getModelSelectedTab(),
                tabObserver.mObservedTab);

        tabObserver.destroy();
    }

    /** Test activityTabProvider before layout state provider is available. */
    @Test
    @SmallTest
    @Feature({"ActivityTabObserver"})
    @Features.EnableFeatures({
        ChromeFeatureList.INSTANT_START,
        ChromeFeatureList.BACK_GESTURE_REFACTOR
    })
    public void testBeforeLayoutManagerAvailable() {
        ActivityTabProvider activityTabProvider =
                TestThreadUtils.runOnUiThreadBlockingNoException(ActivityTabProvider::new);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activityTabProvider.setTabModelSelector(mActivity.getTabModelSelector());
                });
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                false);
        assertEquals(
                "The activity tab should be the selected tab.",
                getModelSelectedTab(),
                activityTabProvider.get());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.getLayoutManager().showLayout(LayoutType.TAB_SWITCHER, false);
                    activityTabProvider.setLayoutStateProvider(mActivity.getLayoutManager());
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "The activity tab should be null on tab switcher.",
                            activityTabProvider.get(),
                            Matchers.equalTo(null));
                });
        TestThreadUtils.runOnUiThreadBlocking(activityTabProvider::destroy);
    }
}
