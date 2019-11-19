// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunActivity.FirstRunActivityObserver;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.sync.AccountManagementFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the first run experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
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
                mTestObserver.flowIsKnownCallback.waitForCallback(0);
            } catch (TimeoutException e) {
                Assert.fail();
            }
            CriteriaHelper.pollUiThread((() -> mActivity.isNativeSideIsInitializedForTest()),
                    "native never initialized.");
        }
    };

    private static final String TEST_ACTION = "com.artificial.package.TEST_ACTION";

    private static final class TestObserver implements FirstRunActivityObserver {
        public final CallbackHelper flowIsKnownCallback = new CallbackHelper();

        @Override
        public void onFlowIsKnown(Bundle freProperties) {
            flowIsKnownCallback.notifyCalled();
        }

        @Override
        public void onAcceptTermsOfService() {}

        @Override
        public void onJumpToPage(int position) {}

        @Override
        public void onUpdateCachedEngineName() {}

        @Override
        public void onAbortFirstRunExperience() {}
    }

    private final TestObserver mTestObserver = new TestObserver();
    private FirstRunActivity mActivity;

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
        Account testAccount = SigninTestUtil.addTestAccount();
        Assert.assertNull(SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        processFirstRun(testAccount.name, false /* ShowSettings */);
        Assert.assertEquals(testAccount, SigninTestUtil.getCurrentAccount());
        SyncTestUtil.waitForSyncActive();
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
        final Account testAccount = SigninTestUtil.addTestAccount();
        final Preferences prefActivity = processFirstRun(testAccount.name, true /* ShowSettings */);

        // User should be signed in and the sync backend should initialize, but sync should not
        // become fully active until the settings page is closed.
        Assert.assertEquals(testAccount, SigninTestUtil.getCurrentAccount());
        SyncTestUtil.waitForEngineInitialized();
        Assert.assertFalse(SyncTestUtil.isSyncActive());

        // Close the settings fragment.
        AccountManagementFragment fragment =
                (AccountManagementFragment) prefActivity.getMainFragment();
        Assert.assertNotNull(fragment);
        prefActivity.getSupportFragmentManager().beginTransaction().remove(fragment).commit();

        // Sync should immediately become active.
        Assert.assertTrue(SyncTestUtil.isSyncActive());
    }

    // Test that not signing in through FirstRun does not sign in sync.
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest // https://crbug.com/901488
    public void testNoSignIn() {
        SigninTestUtil.addTestAccount();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        processFirstRun(null, false /* ShowSettings */);
        Assert.assertNull(SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }

    /**
     * Execute the FirstRun code using the given parameters.
     *
     * @param account The account name to sign in, or null.
     * @param showSettings Whether to show the settings page.
     * @return The Preferences activity if showSettings was YES; null otherwise.
     */
    private Preferences processFirstRun(String account, boolean showSettings) {
        FirstRunSignInProcessor.setFirstRunFlowSignInComplete(false);
        FirstRunSignInProcessor.finalizeFirstRunFlowState(account, showSettings);

        Preferences prefActivity = null;
        if (showSettings) {
            prefActivity =
                    ActivityUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                            Preferences.class, new Runnable() {
                                @Override
                                public void run() {
                                    processFirstRunOnUiThread();
                                }
                            });
            Assert.assertNotNull("Could not find the preferences activity", prefActivity);
        } else {
            processFirstRunOnUiThread();
        }

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return FirstRunSignInProcessor.getFirstRunFlowSignInComplete();
            }
        });
        return prefActivity;
    }

    private void processFirstRunOnUiThread() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { FirstRunSignInProcessor.start(mActivity); });
    }
}
