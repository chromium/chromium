// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the PowerBroadcastReceiver.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class PowerBroadcastReceiverTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final long MS_INTERVAL = 1000;
    private static final long MS_RUNNABLE_DELAY = 2500;
    private static final long MS_TIMEOUT = 5000;

    private static final MockPowerManagerHelper sScreenOff = new MockPowerManagerHelper(false);
    private static final MockPowerManagerHelper sScreenOn = new MockPowerManagerHelper(true);

    /** Mocks out PowerBroadcastReceiver.ServiceRunnable. */
    private static class MockServiceRunnable extends PowerBroadcastReceiver.ServiceRunnable {
        public CallbackHelper postHelper = new CallbackHelper();
        public CallbackHelper cancelHelper = new CallbackHelper();
        public CallbackHelper runHelper = new CallbackHelper();
        public CallbackHelper runActionsHelper = new CallbackHelper();

        @Override
        public void setState(@State int state) {
            super.setState(state);
            if (state == State.POSTED) {
                postHelper.notifyCalled();
            } else if (state == State.CANCELED) {
                cancelHelper.notifyCalled();
            } else if (state == State.COMPLETED) {
                runHelper.notifyCalled();
            }
        }

        @Override
        public long getDelayToRun() {
            return MS_RUNNABLE_DELAY;
        }

        @Override
        public void runActions() {
            // Don't let the real services start.
            runActionsHelper.notifyCalled();
        }
    }

    /** Mocks out PowerBroadcastReceiver.PowerManagerHelper. */
    private static class MockPowerManagerHelper extends PowerBroadcastReceiver.PowerManagerHelper {
        private final boolean mScreenIsOn;

        public MockPowerManagerHelper(boolean screenIsOn) {
            mScreenIsOn = screenIsOn;
        }

        @Override
        public boolean isScreenOn(Context context) {
            return mScreenIsOn;
        }
    }

    private MockServiceRunnable mRunnable;
    private PowerBroadcastReceiver mReceiver;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();
        mReceiver = TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeActivitySessionTracker.getInstance()
                                   .getPowerBroadcastReceiverForTesting());

        // Set up our mock runnable.
        mRunnable = new MockServiceRunnable();
        mReceiver.setServiceRunnableForTests(mRunnable);

        // Initially claim that the screen is on.
        mReceiver.setPowerManagerHelperForTests(sScreenOn);
    }

    /**
     * Check if the runnable is posted and run while the screen is on.
     */
    @Test
    @MediumTest
    @Feature({"Omaha", "Sync"})
    public void testRunnableRunsWithScreenOn() throws Exception {
        // Pause & resume to post the runnable.
        ApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());
        int postCount = mRunnable.postHelper.getCallCount();
        int runCount = mRunnable.runHelper.getCallCount();
        ApplicationTestUtils.launchChrome(InstrumentationRegistry.getTargetContext());

        // Relaunching Chrome should cause the runnable to trigger.
        mRunnable.postHelper.waitForCallback(postCount, 1);
        mRunnable.runHelper.waitForCallback(runCount, 1);
        Assert.assertEquals(0, mRunnable.cancelHelper.getCallCount());
        Assert.assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }

    /**
     * Check that the runnable gets posted and canceled when Main is sent to the background.
     */
    @Test
    @FlakyTest(message = "https://crbug.com/579363")
    @MediumTest
    @Feature({"Omaha", "Sync"})
    public void testRunnableGetsCanceled() throws Exception {
        // Pause & resume to post the runnable.
        ApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());
        int postCount = mRunnable.postHelper.getCallCount();
        ApplicationTestUtils.launchChrome(InstrumentationRegistry.getTargetContext());
        mRunnable.postHelper.waitForCallback(postCount, 1);
        Assert.assertEquals(0, mRunnable.runHelper.getCallCount());
        Assert.assertEquals(0, mRunnable.cancelHelper.getCallCount());

        // Pause before the runnable has a chance to run.  Should cause the runnable to be canceled.
        int cancelCount = mRunnable.cancelHelper.getCallCount();
        ApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());
        mRunnable.cancelHelper.waitForCallback(cancelCount, 1);
        Assert.assertEquals(0, mRunnable.runHelper.getCallCount());
        Assert.assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }

    /**
     * Check that the runnable gets run only while the screen is on.
     */
    @Test
    @MediumTest
    @Feature({"Omaha", "Sync"})
    @FlakyTest(message = "https://crbug.com/587138")
    public void testRunnableGetsRunWhenScreenIsOn() throws Exception {
        // Claim the screen is off.
        mReceiver.setPowerManagerHelperForTests(sScreenOff);

        // Pause & resume.  Because the screen is off, nothing should happen.
        ApplicationTestUtils.fireHomeScreenIntent(InstrumentationRegistry.getTargetContext());
        ApplicationTestUtils.launchChrome(InstrumentationRegistry.getTargetContext());
        Assert.assertTrue("Isn't waiting for power broadcasts.", mReceiver.isRegistered());
        Assert.assertEquals(0, mRunnable.postHelper.getCallCount());
        Assert.assertEquals(0, mRunnable.runHelper.getCallCount());
        Assert.assertEquals(0, mRunnable.cancelHelper.getCallCount());

        // Pretend to turn the screen on.
        int postCount = mRunnable.postHelper.getCallCount();
        int runCount = mRunnable.runHelper.getCallCount();
        mReceiver.setPowerManagerHelperForTests(sScreenOn);
        Intent intent = new Intent(Intent.ACTION_SCREEN_ON);
        mReceiver.onReceive(InstrumentationRegistry.getTargetContext(), intent);

        // The runnable should run now that the screen is on.
        mRunnable.postHelper.waitForCallback(postCount, 1);
        mRunnable.runHelper.waitForCallback(runCount, 1);
        Assert.assertEquals(0, mRunnable.cancelHelper.getCallCount());
        Assert.assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }
}
