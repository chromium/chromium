// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.UiDevice;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceCategory;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.sync.SyncAndServicesPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for SyncAndServicesPreferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncAndServicesPreferencesTest {
    private static final String TAG = "SyncAndServicesPreferencesTest";

    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private Preferences mPreferences;

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitch() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        final ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);

        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    /**
     * This is a regression test for http://crbug.com/454939.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testOpeningSettingsDoesntEnableSync() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        closeFragment(fragment);
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    /**
     * This is a regression test for http://crbug.com/467600.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testOpeningSettingsDoesntStartEngine() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mSyncTestRule.getProfileSyncService().isSyncRequested());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOffThenOn() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOffState(fragment);
        mSyncTestRule.togglePreference(getSyncSwitch(fragment));
        SyncTestUtil.waitForEngineInitialized();
        assertSyncOnState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOnThenOff() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(getSyncSwitch(fragment));
        assertSyncOffState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/991135")
    public void testSyncSwitchClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        Assert.assertFalse(
                "There should be no server cards", mSyncTestRule.hasServerAutofillCreditCards());
        mSyncTestRule.addServerAutofillCreditCard();
        Assert.assertTrue(
                "There should be server cards", mSyncTestRule.hasServerAutofillCreditCards());

        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());

        closeFragment(fragment);

        Assert.assertFalse("There should be no server cards remaining",
                mSyncTestRule.hasServerAutofillCreditCards());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDismissedSettingsDoesNotSetFirstSetupComplete() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // FirstSetupComplete should not be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(ProfileSyncService.get().isFirstSetupComplete()); });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDismissedSettingsShowsSyncSwitchOffByDefault() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        assertSyncOffState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDismissedSettingsShowsSyncErrorCard() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        Assert.assertNotNull("Sync error card should be shown", getSyncErrorCard(fragment));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFirstSetupCompleteIsSetAfterSettingsOpenedAndBackPressed() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and leave sync off.
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        pressBackAndDismissActivity(fragment.getActivity());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        fragment = startSyncAndServicesPreferences();
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
        assertSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFirstSetupCompleteIsSetAfterSettingsOpenedAndDismissed() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and leave sync off.
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        ApplicationTestUtils.finishActivity(fragment.getActivity());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        fragment = startSyncAndServicesPreferences();
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
        assertSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettings.get().isChromeSyncEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFirstSetupCompleteIsSetAfterSyncTurnedOn() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and turn sync on.
        SyncAndServicesPreferences fragment = startSyncAndServicesPreferences();
        ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.get().isChromeSyncEnabled());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
    }

    /**
     * Start SyncAndServicesPreferences signin screen and dissmiss it without pressing confirm or
     * cancel.
     */
    private void startPreferencesForAdvancedSyncFlowAndInterruptIt() throws Exception {
        Context context = InstrumentationRegistry.getTargetContext();
        String fragmentName = SyncAndServicesPreferences.class.getName();
        final Bundle arguments = SyncAndServicesPreferences.createArguments(true);
        Intent intent =
                PreferencesLauncher.createIntentForSettingsPage(context, fragmentName, arguments);
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertTrue(activity instanceof Preferences);
        ApplicationTestUtils.finishActivity(activity);
    }

    private SyncAndServicesPreferences startSyncAndServicesPreferences() {
        mPreferences = mSyncTestRule.startPreferences(SyncAndServicesPreferences.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (SyncAndServicesPreferences) mPreferences.getMainFragment();
    }

    private void closeFragment(SyncAndServicesPreferences fragment) {
        FragmentTransaction transaction =
                mPreferences.getSupportFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    // TODO(https://crbug.com/1015449): Move this function to ApplicationTestUtils.
    private void waitForActivityState(Activity activity, @ActivityState int state)
            throws Exception {
        final CallbackHelper callbackHelper = new CallbackHelper();
        final ApplicationStatus.ActivityStateListener activityStateListener =
                (activity1, newState) -> {
            if (newState == state) {
                callbackHelper.notifyCalled();
            }
        };
        try {
            boolean correctState = TestThreadUtils.runOnUiThreadBlocking(() -> {
                if (ApplicationStatus.getStateForActivity(activity) == state) {
                    return true;
                }
                ApplicationStatus.registerStateListenerForActivity(activityStateListener, activity);
                activity.finish();
                return false;
            });
            if (!correctState) {
                callbackHelper.waitForCallback(0);
            }
        } finally {
            ApplicationStatus.unregisterActivityStateListener(activityStateListener);
        }
    }

    private void pressBackAndDismissActivity(Activity activity) throws Exception {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressBack();
        waitForActivityState(activity, ActivityState.DESTROYED);
    }

    private ChromeSwitchPreference getSyncSwitch(SyncAndServicesPreferences fragment) {
        return (ChromeSwitchPreference) fragment.findPreference(
                SyncAndServicesPreferences.PREF_SYNC_REQUESTED);
    }

    private Preference getSyncErrorCard(SyncAndServicesPreferences fragment) {
        return ((PreferenceCategory) fragment.findPreference(
                        SyncAndServicesPreferences.PREF_SYNC_CATEGORY))
                .findPreference(SyncAndServicesPreferences.PREF_SYNC_ERROR_CARD);
    }

    private void assertSyncOnState(SyncAndServicesPreferences fragment) {
        Assert.assertTrue("The sync switch should be on.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
    }

    private void assertSyncOffState(SyncAndServicesPreferences fragment) {
        Assert.assertFalse("The sync switch should be off.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
    }
}