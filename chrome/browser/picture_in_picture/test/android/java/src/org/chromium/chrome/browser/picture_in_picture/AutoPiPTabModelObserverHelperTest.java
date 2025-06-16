// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation test for the C++ AutoPictureInPictureTabModelObserverHelper logic. This test uses
 * a JNI bridge to verify the behavior of the C++ class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AutoPiPTabModelObserverHelperTest {
    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private WebPageStation mPage;
    private WebContents mObservedWebContents;
    private final CallbackHelper mOnActivationChangedCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mObservedWebContents = mPage.webContentsElement.get();

        // Initialize the C++ test utilities for the WebContents under observation,
        // passing it the callback to be invoked from C++.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutoPiPTabModelObserverHelperTestUtils.initialize(
                            mObservedWebContents,
                            (isActivated) -> {
                                mOnActivationChangedCallbackHelper.notifyCalled();
                            });
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AutoPiPTabModelObserverHelperTestUtils.destroy(mObservedWebContents));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Implementation not yet landed. See crbug.com/421608904")
    public void testTriggersOnTabActivationChanged() throws TimeoutException {
        int callCount = mOnActivationChangedCallbackHelper.getCallCount();

        // Start observing.
        AutoPiPTabModelObserverHelperTestUtils.startObserving(mObservedWebContents);

        // Open and switch to a new tab. This should deactivate our observed tab.
        PageStation page = mPage.openNewTabFast();
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);

        // Switch back to the original tab. This should activate it.
        page.openRegularTabSwitcher().selectTabAtIndex(0, WebPageStation.newBuilder());
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Implementation not yet landed. See crbug.com/421608904")
    public void testStopAndStartObserving() throws TimeoutException {
        int callCount = mOnActivationChangedCallbackHelper.getCallCount();

        // Start observing, then immediately stop.
        AutoPiPTabModelObserverHelperTestUtils.startObserving(mObservedWebContents);
        AutoPiPTabModelObserverHelperTestUtils.stopObserving(mObservedWebContents);

        // Open a new tab. Since we are not observing, no additional callback
        // should fire.
        PageStation page = mPage.openNewTabFast();
        assertEquals(
                "Callback should not have fired.",
                callCount,
                mOnActivationChangedCallbackHelper.getCallCount());

        // Start observing again.
        AutoPiPTabModelObserverHelperTestUtils.startObserving(mObservedWebContents);

        // Switch back to the original tab. It should now trigger the callback.
        page.openRegularTabSwitcher().selectTabAtIndex(0, WebPageStation.newBuilder());
        mOnActivationChangedCallbackHelper.waitForCallback(callCount++);
    }

    // TODO(crbug.com/421608904): add additional multi window tests.
}
