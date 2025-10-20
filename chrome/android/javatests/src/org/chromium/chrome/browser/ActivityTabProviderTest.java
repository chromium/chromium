// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests for {@link ChromeActivity}'s {@link ActivityTabProvider}. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ActivityTabProviderTest {
    private WebPageStation mPage;

    /** A test observer that provides access to the tab being observed. */
    private static class TestActivityTabTabObserver extends ActivityTabTabObserver {
        /** The tab currently being observed. */
        private Tab mObservedTab;

        public TestActivityTabTabObserver(ActivityTabProvider provider) {
            super(provider);
            ThreadUtils.runOnUiThreadBlocking(() -> mObservedTab = provider.get());
        }

        @Override
        public void onObservingDifferentTab(Tab tab, boolean hint) {
            mObservedTab = tab;
        }

        @Override
        protected void updateObservedTabToCurrent() {
            ThreadUtils.runOnUiThreadBlocking(super::updateObservedTabToCurrent);
        }

        @Override
        protected void addObserverToTabSupplier() {
            ThreadUtils.runOnUiThreadBlocking(super::addObserverToTabSupplier);
        }

        @Override
        protected void removeObserverFromTabSupplier() {
            ThreadUtils.runOnUiThreadBlocking(super::removeObserverFromTabSupplier);
        }

        @Override
        @SuppressWarnings("MissingSuperCall")
        public void destroy() {
            ThreadUtils.runOnUiThreadBlocking(super::destroy);
        }
    }

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private ChromeTabbedActivity mActivity;
    private ActivityTabProvider mProvider;
    private Tab mActivityTab;
    private final CallbackHelper mActivityTabChangedHelper = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
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
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getTabModelSelector().getCurrentTab());
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
        ThreadUtils.runOnUiThreadBlocking(
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
    @Restriction(DeviceFormFactor.PHONE)
    public void testTriggerWithTabSwitcher() throws TimeoutException {
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);

        RegularTabSwitcherStation tabSwitcher = mPage.openRegularTabSwitcher();
        mActivityTabChangedHelper.waitForCallback(1);
        assertEquals(
                "Entering the tab switcher should have triggered the event once.",
                2,
                mActivityTabChangedHelper.getCallCount());
        assertEquals("The activity tab should be null.", null, mActivityTab);
        mPage = tabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
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

        mPage = mPage.openFakeLinkToWebPage(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        assertNotEquals(
                "A new tab should be in the foreground.", startingTab, getModelSelectedTab());
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);

        int callCount = mActivityTabChangedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Select the original tab without switching layouts.
                    mActivity
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(0, TabSelectionType.FROM_USER);
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
    @Restriction(DeviceFormFactor.PHONE)
    public void testTriggerOnLastTabClosed() throws TimeoutException {
        // Have a tab open in incognito model. This should not be in the way getting the event
        // triggered when closing the last tab in normal mode.
        TabModelSelector selector = mActivity.getTabModelSelector();

        mPage =
                mPage.openNewIncognitoTabFast()
                        .loadWebPageProgrammatically(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        ThreadUtils.runOnUiThreadBlocking(() -> selector.selectModel(false));

        int callCount = mActivityTabChangedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.tryCloseTab(
                            TabClosureParams.closeTab(getModelSelectedTab())
                                    .allowUndo(false)
                                    .build(),
                            /* allowDialog= */ false);
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

        mPage = mPage.openFakeLinkToWebPage(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        assertNotEquals(
                "The starting tab should not be the selected tab.",
                getModelSelectedTab(),
                startingTab);
        assertEquals(
                "The activity tab should be the model's selected tab.",
                getModelSelectedTab(),
                mActivityTab);
        Tab activityTabBefore = mActivityTab;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getTabModelSelector()
                            .tryCloseTab(
                                    TabClosureParams.closeTab(startingTab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
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

        mPage = mPage.openFakeLinkToWebPage(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        assertNotEquals("The tab should have changed.", startingTab, getModelSelectedTab());
        assertEquals(
                "The observer should be attached to the new tab.",
                getModelSelectedTab(),
                tabObserver.mObservedTab);

        tabObserver.destroy();
    }
}
