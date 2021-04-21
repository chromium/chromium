// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunActivity.FirstRunActivityObserver;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the first run experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class FirstRunTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        public void startMainActivityForSyncTest() {
            FirstRunActivity.setObserverForTest(mTestObserver);

            // Starts up and waits for the FirstRunActivity to be ready.
            // This isn't exactly what startMainActivity is supposed to be doing, but short of a
            // refactoring of SyncTestBase to use something other than ChromeTabbedActivity,
            // it's the only way to reuse the rest of the setup and initialization code inside of
            // it.
            final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
            final Context context = instrumentation.getTargetContext();

            // Create an Intent that causes Chrome to run.
            final Intent intent = new Intent(TEST_ACTION);
            intent.setPackage(context.getPackageName());
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            // Start the FRE.
            final ActivityMonitor freMonitor =
                    new ActivityMonitor(FirstRunActivity.class.getName(), null, false);
            instrumentation.addMonitor(freMonitor);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                FirstRunFlowSequencer.launch(context, intent, false /* requiresBroadcast */,
                        false /* preferLightweightFre */);
            });

            // Wait for the FRE to be ready to use.
            Activity activity =
                    freMonitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
            instrumentation.removeMonitor(freMonitor);

            mActivity = (FirstRunActivity) activity;

            try {
                mTestObserver.createPostNativeAndPoliciesPageSequence.waitForCallback(0);
            } catch (TimeoutException e) {
                Assert.fail();
            }
            CriteriaHelper.pollUiThread((() -> mActivity.isNativeSideIsInitializedForTest()),
                    "native never initialized.");
        }
    };

    private static final String TEST_ACTION = "com.artificial.package.TEST_ACTION";

    private static final class TestObserver implements FirstRunActivityObserver {
        public final CallbackHelper createPostNativeAndPoliciesPageSequence = new CallbackHelper();

        @Override
        public void onCreatePostNativeAndPoliciesPageSequence(
                FirstRunActivity caller, Bundle freProperties) {
            createPostNativeAndPoliciesPageSequence.notifyCalled();
        }

        @Override
        public void onAcceptTermsOfService(FirstRunActivity caller) {}

        @Override
        public void onJumpToPage(FirstRunActivity caller, int position) {}

        @Override
        public void onUpdateCachedEngineName(FirstRunActivity caller) {}

        @Override
        public void onAbortFirstRunExperience(FirstRunActivity caller) {}

        @Override
        public void onExitFirstRun(FirstRunActivity caller) {}
    }

    private final TestObserver mTestObserver = new TestObserver();
    private FirstRunActivity mActivity;

    @Before
    public void setUp() {
        Assert.assertFalse(
                CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE));
    }

    @After
    public void tearDown() {
        if (mActivity != null) mActivity.finish();
    }

    // Test that signing in through FirstRun signs in and starts sync.
    /*
     * @SmallTest
     * @Feature({"Sync"})
     */
    @Test
    @FlakyTest(message = "https://crbug.com/616456")
    public void testSignIn() {
        CoreAccountInfo testAccountInfo = mSyncTestRule.addTestAccount();
        Assert.assertNull(mSyncTestRule.getCurrentSignedInAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        processFirstRun(testAccountInfo.getEmail(), false /* ShowSettings */);
        Assert.assertEquals(testAccountInfo, mSyncTestRule.getCurrentSignedInAccount());
        SyncTestUtil.waitForSyncFeatureActive();
    }

    // Test that signing in and opening settings through FirstRun signs in and doesn't fully start
    // sync until the settings page is closed.
    /*
     * @SmallTest
     * @Feature({"Sync"})
     */
    @Test
    @FlakyTest(message = "https://crbug.com/616456")
    public void testSignInWithOpenSettings() {
        CoreAccountInfo testAccountInfo = mSyncTestRule.addTestAccount();
        final SettingsActivity settingsActivity =
                processFirstRun(testAccountInfo.getEmail(), true /* ShowSettings */);

        // User should be signed in and the sync backend should initialize, but sync should not
        // become fully active until the settings page is closed.
        Assert.assertEquals(testAccountInfo, mSyncTestRule.getCurrentSignedInAccount());
        SyncTestUtil.waitForEngineInitialized();
        Assert.assertFalse(SyncTestUtil.isSyncFeatureActive());

        // Close the settings fragment.
        AccountManagementFragment fragment =
                (AccountManagementFragment) settingsActivity.getMainFragment();
        Assert.assertNotNull(fragment);
        settingsActivity.getSupportFragmentManager().beginTransaction().remove(fragment).commit();

        // Sync should immediately become active.
        Assert.assertTrue(SyncTestUtil.isSyncFeatureActive());
    }

    // Test that not signing in through FirstRun does not sign in sync.
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest // https://crbug.com/901488
    public void testNoSignIn() {
        mSyncTestRule.addTestAccount();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        processFirstRun(null, false /* ShowSettings */);
        Assert.assertNull(mSyncTestRule.getCurrentSignedInAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }

    /**
     * Execute the FirstRun code using the given parameters.
     *
     * @param account The account name to sign in, or null.
     * @param showSettings Whether to show the settings page.
     * @return The Settings activity if showSettings was YES; null otherwise.
     */
    private SettingsActivity processFirstRun(String account, boolean showSettings) {
        FirstRunSignInProcessor.setFirstRunFlowSignInComplete(false);
        FirstRunSignInProcessor.finalizeFirstRunFlowState(account, showSettings);

        SettingsActivity settingsActivity = null;
        if (showSettings) {
            settingsActivity =
                    ActivityUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                            SettingsActivity.class, new Runnable() {
                                @Override
                                public void run() {
                                    processFirstRunOnUiThread();
                                }
                            });
            Assert.assertNotNull("Could not find the settings activity", settingsActivity);
        } else {
            processFirstRunOnUiThread();
        }

        CriteriaHelper.pollInstrumentationThread(
                () -> FirstRunSignInProcessor.getFirstRunFlowSignInComplete());
        return settingsActivity;
    }

    private void processFirstRunOnUiThread() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { FirstRunSignInProcessor.start(mActivity); });
    }
}
