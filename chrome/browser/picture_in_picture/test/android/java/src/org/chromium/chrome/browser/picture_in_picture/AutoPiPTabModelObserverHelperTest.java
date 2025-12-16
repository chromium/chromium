// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.moveActivityToFront;
import static org.chromium.ui.test.util.DeviceRestriction.RESTRICTION_TYPE_NON_AUTO;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation test for the C++ AutoPictureInPictureTabModelObserverHelper logic. This test uses
 * a JNI bridge to verify the behavior of the C++ class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING
})
@Restriction({RESTRICTION_TYPE_NON_AUTO, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Batch(Batch.PER_CLASS)
public class AutoPiPTabModelObserverHelperTest {
    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static class TabActivationCallbackHelper extends CallbackHelper {
        private boolean mIsActivated;

        public void notifyCalled(boolean value) {
            mIsActivated = value;
            notifyCalled();
        }

        public boolean isActivated() {
            return mIsActivated;
        }
    }

    private TabActivationCallbackHelper mOnActivationChangedCallbackHelper;
    private ChromeTabbedActivity mInitialActivity;
    private ChromeTabbedActivity mSecondActivity;
    private WebPageStation mInitialPage;
    private Tab mInitialTab;
    private WebContents mObservedWebContents;

    private static final String BLANK_PAGE_URL = "about:blank";

    @Before
    public void setUp() {
        mOnActivationChangedCallbackHelper = new TabActivationCallbackHelper();
        mInitialPage = mActivityTestRule.startOnBlankPage();
        mInitialTab = mInitialPage.loadedTabElement.value();
        mInitialActivity = mInitialPage.getActivity();
        mObservedWebContents = mInitialPage.webContentsElement.value();

        // Initialize the C++ test utilities for the WebContents under observation,
        // passing it the callback to be invoked from C++.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutoPiPTabModelObserverHelperTestUtils.initialize(
                            mObservedWebContents,
                            (isActivated) -> {
                                mOnActivationChangedCallbackHelper.notifyCalled(isActivated);
                            });
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> AutoPiPTabModelObserverHelperTestUtils.destroy());

        if (mSecondActivity != null) {
            ApplicationTestUtils.finishActivity(mSecondActivity);
        }
        if (mInitialActivity != null) {
            ApplicationTestUtils.finishActivity(mInitialActivity);
        }
    }

    /** Tests that tab switching should activate/deactivate the observed tab. */
    @Test
    @MediumTest
    public void testTriggersOnTabActivationChanged() throws TimeoutException {
        int callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ true);

        CtaPageStation page = mInitialPage.openNewTabFast();
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertFalse(mOnActivationChangedCallbackHelper.isActivated());

        page.openRegularTabSwitcher().selectTabAtIndex(0, WebPageStation.newBuilder());
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertTrue(mOnActivationChangedCallbackHelper.isActivated());
    }

    /** Tests that stopping observation prevents callbacks, and starting it resumes them. */
    @Test
    @MediumTest
    public void testStopAndStartObserving() throws TimeoutException {
        int callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ true);

        AutoPiPTabModelObserverHelperTestUtils.stopObserving();
        CtaPageStation page = mInitialPage.openNewTabFast();
        assertEquals(
                "Callback should not have fired after stopping observation.",
                callCount,
                mOnActivationChangedCallbackHelper.getCallCount());

        // Start observing again. Since the second tab is currently selected, the originally
        // observed tab is not active.
        callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ false);

        // Tab switching after observing is resumed should trigger the callback.
        page.openRegularTabSwitcher().selectTabAtIndex(0, WebPageStation.newBuilder());
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertTrue(mOnActivationChangedCallbackHelper.isActivated());
    }

    /** Tests that creating a tab in the background does not trigger a callback. */
    @Test
    @MediumTest
    public void testOpenBackgroundTab() throws TimeoutException {
        int callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ true);

        // Open a tab in the background
        Tab backgroundTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mInitialActivity
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(BLANK_PAGE_URL),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                            mInitialTab);
                        });
        ChromeTabUtils.waitForTabPageLoaded(backgroundTab, BLANK_PAGE_URL);
        assertEquals(
                "Callback should not have fired after opening a background tab.",
                callCount,
                mOnActivationChangedCallbackHelper.getCallCount());

        // Select the background tab
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            mInitialActivity.getTabModelSelector(),
                            backgroundTab.getId(),
                            TabSelectionType.FROM_USER);
                });
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertFalse(mOnActivationChangedCallbackHelper.isActivated());
    }

    /** Tests that creating a tab in the background does not trigger a callback. */
    @Test
    @MediumTest
    public void testCloseTab() throws TimeoutException {
        int callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ true);

        // Open a second tab and switch to it
        CtaPageStation page = mInitialPage.openNewTabFast();
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertFalse(mOnActivationChangedCallbackHelper.isActivated());

        // Switch back to the original tab
        page = page.openRegularTabSwitcher().selectTabAtIndex(0, WebPageStation.newBuilder());
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertTrue(mOnActivationChangedCallbackHelper.isActivated());

        // Close the active tab should trigger a callback
        RegularTabSwitcherStation regularTabSwitcher = page.openRegularTabSwitcher();
        regularTabSwitcher = regularTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertFalse(mOnActivationChangedCallbackHelper.isActivated());
    }

    /* Tests that opening a second window doesn't change the tab model being observed. */
    @Test
    @MediumTest
    public void testOpenSecondWindow() throws TimeoutException {
        // TODO(crbug.com/425983853): use MultiWindowManagerApi31 methods to create windows.
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return;
        }
        int callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ true);

        // Create a second window
        createAndInitializeSecondWindow(mInitialActivity);
        assertEquals(
                "Callback should not have fired after opening a second window.",
                callCount,
                mOnActivationChangedCallbackHelper.getCallCount());

        // Switching away from the observed tab in the original window should still deactivate it
        moveActivityToFront(mInitialActivity);
        mInitialPage.openNewTabFast();
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertFalse(mOnActivationChangedCallbackHelper.isActivated());
    }

    /**
     * Tests moving an active observed tab to a new window, where it remains active. This should not
     * trigger a callback as its activation state doesn't change.
     */
    @Test
    @MediumTest
    public void testMoveActiveObservedTabToNewWindow() throws TimeoutException {
        // TODO(crbug.com/425983853): use MultiWindowManagerApi31 methods to create windows and
        // reparent tabs.
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return;
        }
        int callCount = startObservingAndAssertInitialCallback(/* expectedIsActivated= */ true);
        // Open a new tab
        CtaPageStation page = mInitialPage.openNewTabFast();
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertFalse(mOnActivationChangedCallbackHelper.isActivated());
        // Switch back to the original tab
        page = page.openRegularTabSwitcher().selectTabAtIndex(0, WebPageStation.newBuilder());
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
        assertTrue(mOnActivationChangedCallbackHelper.isActivated());

        // Create a second window
        createAndInitializeSecondWindow(mInitialActivity);
        // Move the original tab(active) to the new window
        moveActivityToFront(mInitialActivity);
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mSecondActivity,
                R.id.move_to_other_window_menu_id);

        assertEquals(
                "Moving an active tab should not trigger a callback.",
                callCount,
                mOnActivationChangedCallbackHelper.getCallCount());
    }

    /**
     * Tests moving an inactive observed tab to a new window and making it active. This should
     * trigger a callback.
     */
    @Test
    @MediumTest
    @DisabledTest(
            message = "Background tab reparenting test support not added. See crbug.com/425983853")
    public void testMoveInactiveObservedTabToNewWindow() {
        // TODO(crbug.com/421608904): test moving observed tab in the background to another window.
        // The tab should be activated.
    }

    /**
     * Creates and initializes a second ChromeTabbedActivity window. This is a blocking call that
     * waits until the new activity is fully ready for interaction.
     *
     * @param initialActivity The initial activity to create the second one from.
     */
    private void createAndInitializeSecondWindow(ChromeTabbedActivity initialActivity) {
        mSecondActivity = MultiWindowTestHelper.createSecondChromeTabbedActivity(initialActivity);

        CriteriaHelper.pollUiThread(
                () -> mSecondActivity.getTabModelSelector().isTabStateInitialized());
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mSecondActivity,
                BLANK_PAGE_URL,
                /* incognito= */ false);
    }

    /**
     * Helper method to start observing and assert the state of the initial callback.
     *
     * @param expectedIsActivated The expected activation state after starting to observe.
     * @return The updated call count.
     * @throws TimeoutException if the observer callback does not fire within the timeout period.
     */
    private int startObservingAndAssertInitialCallback(boolean expectedIsActivated)
            throws TimeoutException {
        int callCount = mOnActivationChangedCallbackHelper.getCallCount();

        AutoPiPTabModelObserverHelperTestUtils.startObserving();
        mOnActivationChangedCallbackHelper.waitForCallback(callCount);
        assertEquals(expectedIsActivated, mOnActivationChangedCallbackHelper.isActivated());

        return mOnActivationChangedCallbackHelper.getCallCount();
    }
}
