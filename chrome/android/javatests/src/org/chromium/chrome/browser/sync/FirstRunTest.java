// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunActivity.FirstRunActivityObserver;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the first run experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class FirstRunTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

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

        @Override
        public void onExitFirstRun() {}
    }

    private final TestObserver mTestObserver = new TestObserver();
    private FirstRunActivity mActivity;

    @Before
    public void setUp() {
        Assert.assertFalse(
                CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE));

        FirstRunActivity.setObserverForTest(mTestObserver);

        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(context.getPackageName());
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Runnable activityTrigger = () -> TestThreadUtils.runOnUiThreadBlocking(() -> {
            FirstRunFlowSequencer.launch(context, intent, false /* requiresBroadcast */,
                    false /* preferLightweightFre */);
        });
        mActivity = ActivityUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                FirstRunActivity.class, activityTrigger);

        try {
            mTestObserver.flowIsKnownCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail();
        }
        CriteriaHelper.pollUiThread(
                (() -> mActivity.isNativeSideIsInitializedForTest()), "native never initialized.");
    }

    @After
    public void tearDown() {
        if (mActivity != null) mActivity.finish();
    }

    // Test that signing in through FirstRun signs in and starts sync.
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/616456")
    public void testSignIn() {
        Account testAccount = mSyncTestRule.addTestAccount();
        Assert.assertNull(mSyncTestRule.getCurrentSignedInAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        processFirstRun(testAccount.name, false /* ShowSettings */);
        Assert.assertEquals(testAccount, mSyncTestRule.getCurrentSignedInAccount());
        SyncTestUtil.waitForSyncActive();
    }

    // Test that signing in and opening settings through FirstRun signs in and doesn't fully start
    // sync until the settings page is closed.
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/616456")
    public void testSignInWithOpenSettings() {
        final Account testAccount = mSyncTestRule.addTestAccount();
        final SettingsActivity settingsActivity =
                processFirstRun(testAccount.name, true /* ShowSettings */);

        // User should be signed in and the sync backend should initialize, but sync should not
        // become fully active until the settings page is closed.
        Assert.assertEquals(testAccount, mSyncTestRule.getCurrentSignedInAccount());
        SyncTestUtil.waitForEngineInitialized();
        Assert.assertFalse(SyncTestUtil.isSyncActive());

        // Close the settings fragment.
        SyncAndServicesSettings fragment =
                (SyncAndServicesSettings) settingsActivity.getMainFragment();
        Assert.assertNotNull(fragment);
        settingsActivity.getSupportFragmentManager().beginTransaction().remove(fragment).commit();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // First setup should not be marked as complete just by closing the fragment.
            Assert.assertFalse(mSyncTestRule.getProfileSyncService().isFirstSetupComplete());

            // Marking the first setup as complete should make sync active.
            mSyncTestRule.getProfileSyncService().setFirstSetupComplete(
                    SyncFirstSetupCompleteSource.BASIC_FLOW);
        });
        Assert.assertTrue(SyncTestUtil.isSyncActive());
    }

    // Test that not signing in through FirstRun does not sign in sync.
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/616456")
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
