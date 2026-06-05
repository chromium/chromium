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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableLeakChecks;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests for TabObserver. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableLeakChecks("crbug.com/512492636 (TabObserverTest)")
public class TabObserverTest {
    /** A {@Link TabObserver} that has callback helpers for each event. */
    private static class TestTabObserver extends EmptyTabObserver {
        private final CallbackHelper mInteractabilityHelper = new CallbackHelper();
        private final CallbackHelper mUrlUpdatedHelper = new CallbackHelper();

        @Override
        public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
            mInteractabilityHelper.notifyCalled();
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            mUrlUpdatedHelper.notifyCalled();
        }
    }

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private ChromeTabbedActivity mActivity;
    private Tab mTab;
    private TestTabObserver mTabObserver;

    @Before
    public void setUp() throws Exception {
        mTabObserver = new TestTabObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab = mActivityTestRule.getActivity().getActivityTab();
                    mTab.addObserver(mTabObserver);
                    mActivity = mActivityTestRule.getActivity();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab.removeObserver(mTabObserver);
                });
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testTabInteractable_tabSwitcher() throws TimeoutException {
        final LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        CallbackHelper interactabilityHelper = mTabObserver.mInteractabilityHelper;

        assertTrue("Tab should be interactable.", mTab.isUserInteractable());

        int interactableCallCount = interactabilityHelper.getCallCount();

        // Enter tab switcher mode and make sure the event is triggered.
        TabUiTestHelper.enterTabSwitcher(mActivity);

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab.updateAttachment(null, null);
                    assertFalse(mTab.hasObserverForTesting(mTabObserver));
                });
    }

    private void doTestNavigationStateChanged() throws TimeoutException {
        CallbackHelper urlUpdatedHelper = mTabObserver.mUrlUpdatedHelper;
        int callCount = urlUpdatedHelper.getCallCount();

        final String url = "about:blank";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab.loadUrl(new LoadUrlParams(url));
                });

        urlUpdatedHelper.waitForCallback(callCount);
        org.junit.Assert.assertEquals(url, mTab.getUrl().getSpec());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.DEFER_NAVIGATION_STATE_CHANGED)
    public void testNavigationStateChanged_DeferEnabled() throws TimeoutException {
        doTestNavigationStateChanged();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.DEFER_NAVIGATION_STATE_CHANGED)
    public void testNavigationStateChanged_DeferDisabled() throws TimeoutException {
        doTestNavigationStateChanged();
    }
}
