// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests for TabObserver. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabObserverTest {
    /** A {@Link TabObserver} that has callback helpers for each event. */
    private static class TestTabObserver extends EmptyTabObserver {
        private CallbackHelper mInteractabilityHelper = new CallbackHelper();

        @Override
        public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
            mInteractabilityHelper.notifyCalled();
        }
    }

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static ChromeTabbedActivity sActivity;
    private static Tab sTab;
    private static TestTabObserver sTabObserver;

    @Before
    public void setUp() throws Exception {
        sTabObserver = new TestTabObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTab = sActivityTestRule.getActivity().getActivityTab();
                    sTab.addObserver(sTabObserver);
                    sActivity = sActivityTestRule.getActivity();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTab.removeObserver(sTabObserver);
                });
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testTabInteractable_tabSwitcher() throws TimeoutException {
        final LayoutManagerChrome layoutManager = sActivity.getLayoutManager();
        CallbackHelper interactabilityHelper = sTabObserver.mInteractabilityHelper;

        assertTrue("Tab should be interactable.", sTab.isUserInteractable());

        int interactableCallCount = interactabilityHelper.getCallCount();

        // Enter tab switcher mode and make sure the event is triggered.
        TabUiTestHelper.enterTabSwitcher(sActivity);

        interactabilityHelper.waitForCallback(interactableCallCount);
        interactableCallCount = interactabilityHelper.getCallCount();
        assertFalse("Tab should not be interactable.", sTab.isUserInteractable());

        // Exit tab switcher and wait for event again.
        LayoutTestUtils.startShowingAndWaitForLayout(layoutManager, LayoutType.BROWSING, false);

        interactabilityHelper.waitForCallback(interactableCallCount);
        assertTrue("Tab should be interactable.", sTab.isUserInteractable());
    }

    @Test
    @SmallTest
    public void testTabInteractable_multipleTabs() throws TimeoutException {
        CallbackHelper interactabilityHelper = sTabObserver.mInteractabilityHelper;

        assertTrue("Tab should be interactable.", sTab.isUserInteractable());

        int interactableCallCount = interactabilityHelper.getCallCount();

        // Launch a new tab in the foreground.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), sActivity);

        // The original tab should be hidden.
        interactabilityHelper.waitForCallback(interactableCallCount);
        assertFalse("Tab should not be interactable.", sTab.isUserInteractable());
    }

    @Test
    @SmallTest
    public void testTabDetach_observerUnregistered() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTab.updateAttachment(null, null);
                    assertFalse(sTab.hasObserver(sTabObserver));
                });
    }
}
