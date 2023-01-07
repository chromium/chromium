// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests for TabObserver.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabObserverTest {
    /** A {@Link TabObserver} that has callback helpers for each event. */
    private static class TestTabObserver extends EmptyTabObserver {
        private CallbackHelper mInteractabilityHelper = new CallbackHelper();

        @Override
        public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
            mInteractabilityHelper.notifyCalled();
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mActivity;
    private Tab mTab;
    private TestTabObserver mTabObserver;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTabObserver = new TestTabObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTab = mActivityTestRule.getActivity().getActivityTab();
            mTab.addObserver(mTabObserver);
            mActivity = mActivityTestRule.getActivity();
        });
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTabInteractable_tabSwitcher() throws TimeoutException {
        final LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        CallbackHelper interactabilityHelper = mTabObserver.mInteractabilityHelper;

        assertTrue("Tab should be interactable.", mTab.isUserInteractable());

        int interactableCallCount = interactabilityHelper.getCallCount();

        // Enter tab switcher mode and make sure the event is triggered.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> layoutManager.showLayout(LayoutType.TAB_SWITCHER, false));

        interactabilityHelper.waitForCallback(interactableCallCount);
        interactableCallCount = interactabilityHelper.getCallCount();
        assertFalse("Tab should not be interactable.", mTab.isUserInteractable());

        // Exit tab switcher and wait for event again.
        LayoutTestUtils.startShowingAndWaitForLayout(layoutManager, LayoutType.BROWSING, false);

        interactabilityHelper.waitForCallback(interactableCallCount);
        assertTrue("Tab should be interactable.", mTab.isUserInteractable());
    }

    @Test
    @SmallTest
    public void testTabInteractable_multipleTabs() throws TimeoutException {
        CallbackHelper interactabilityHelper = mTabObserver.mInteractabilityHelper;

        assertTrue("Tab should be interactable.", mTab.isUserInteractable());

        int interactableCallCount = interactabilityHelper.getCallCount();

        // Launch a new tab in the foreground.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), mActivity);

        // The original tab should be hidden.
        interactabilityHelper.waitForCallback(interactableCallCount);
        assertFalse("Tab should not be interactable.", mTab.isUserInteractable());
    }

    @Test
    @SmallTest
    public void testTabDetach_observerUnregistered() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTab.updateAttachment(null, null);
            assertFalse(mTab.hasObserver(mTabObserver));
        });
    }
}
