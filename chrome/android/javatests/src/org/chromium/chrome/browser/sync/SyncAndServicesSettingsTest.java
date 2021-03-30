// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import androidx.fragment.app.FragmentTransaction;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for SyncAndServicesSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncAndServicesSettingsTest {
    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<SyncAndServicesSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(SyncAndServicesSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule).around(mSettingsActivityTestRule);

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testSyncSwitch() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        final ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);

        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
    }

    /**
     * This is a regression test for http://crbug.com/454939.
     */
    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testOpeningSettingsDoesntEnableSync() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.stopSync();
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        closeFragment(fragment);
        Assert.assertFalse(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
    }

    /**
     * This is a regression test for http://crbug.com/467600.
     */
    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testOpeningSettingsDoesntStartEngine() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.stopSync();
        startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mSyncTestRule.getProfileSyncService().isSyncRequested());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testDefaultControlStatesWithSyncOffThenOn() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.stopSync();
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        assertSyncOffState(fragment);
        mSyncTestRule.togglePreference(getSyncSwitch(fragment));
        SyncTestUtil.waitForEngineInitialized();
        assertSyncOnState(fragment);
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testDefaultControlStatesWithSyncOnThenOff() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(getSyncSwitch(fragment));
        assertSyncOffState(fragment);
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    @DisabledTest(message = "https://crbug.com/991135")
    public void testSyncSwitchClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        Assert.assertFalse(
                "There should be no server cards", mSyncTestRule.hasServerAutofillCreditCards());
        mSyncTestRule.addServerAutofillCreditCard();
        Assert.assertTrue(
                "There should be server cards", mSyncTestRule.hasServerAutofillCreditCards());

        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());

        closeFragment(fragment);

        Assert.assertFalse("There should be no server cards remaining",
                mSyncTestRule.hasServerAutofillCreditCards());
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testDismissedSettingsDoesNotSetFirstSetupComplete() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // FirstSetupComplete should not be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(ProfileSyncService.get().isFirstSetupComplete()); });
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testDismissedSettingsShowsSyncSwitchOffByDefault() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        assertSyncOffState(fragment);
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testDismissedSettingsShowsSyncErrorCard() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        Assert.assertNotNull("Sync error card should be shown", getSyncErrorCard(fragment));
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testFirstSetupCompleteIsSetAfterSettingsOpenedAndBackPressed() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and leave sync off.
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        pressBackAndDismissActivity(fragment.getActivity());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        fragment = startSyncAndServicesPreferences();
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
        assertSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testFirstSetupCompleteIsSetAfterSettingsOpenedAndDismissed() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and leave sync off.
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        ApplicationTestUtils.finishActivity(fragment.getActivity());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        fragment = startSyncAndServicesPreferences();
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
        assertSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
    }

    @Test
    @LargeTest
    @Feature({"Sync", "Preferences"})
    public void testFirstSetupCompleteIsSetAfterSyncTurnedOn() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        startPreferencesForAdvancedSyncFlowAndInterruptIt();
        // Open Settings and turn sync on.
        SyncAndServicesSettings fragment = startSyncAndServicesPreferences();
        ChromeSwitchPreference syncSwitch = getSyncSwitch(fragment);
        mSyncTestRule.togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        // FirstSetupComplete should be set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(ProfileSyncService.get().isFirstSetupComplete()); });
        Assert.assertNull("Sync error card should not be shown", getSyncErrorCard(fragment));
    }

    /**
     * Test: if the onboarding was never shown, the AA chrome preference should not exist.
     *
     * Note:
     * - Presence of the {@link GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT}
     *   shared preference indicates whether onboarding was shown or not.
     * - There's a separate settings screen added if either AUTOFILL_ASSISTANT_PROACTIVE_HELP or
     *   OMNIBOX_ASSISTANT_VOICE_SEARCH is enabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantNoPreferenceIfOnboardingNeverShown() {
        final SyncAndServicesSettings syncPrefs = startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(
                    syncPrefs.findPreference(SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Test: if the onboarding was shown at least once, the AA chrome preference should also exist.
     *
     * Note:
     * - Presence of the {@link GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT}
     *   shared preference indicates whether onboarding was shown or not.
     * - There's a separate settings screen added if either AUTOFILL_ASSISTANT_PROACTIVE_HELP or
     *   OMNIBOX_ASSISTANT_VOICE_SEARCH is enabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantPreferenceShownIfOnboardingShown() {
        setAutofillAssistantSwitchValue(true);
        final SyncAndServicesSettings syncPrefs = startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull(
                    syncPrefs.findPreference(SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Ensure that the "Autofill Assistant" setting is not shown when the feature is disabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantNoPreferenceIfFeatureDisabled() {
        setAutofillAssistantSwitchValue(true);
        final SyncAndServicesSettings syncPrefs = startSyncAndServicesPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(
                    syncPrefs.findPreference(SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Ensure that the "Autofill Assistant" on/off switch works.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantSwitchOn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(true); });
        final SyncAndServicesSettings syncAndServicesSettings = startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) syncAndServicesSettings.findPreference(
                            SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT);
            Assert.assertTrue(autofillAssistantSwitch.isChecked());

            autofillAssistantSwitch.performClick();
            Assert.assertFalse(syncAndServicesSettings.isAutofillAssistantSwitchOn());
            autofillAssistantSwitch.performClick();
            Assert.assertTrue(syncAndServicesSettings.isAutofillAssistantSwitchOn());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantSwitchOff() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(false); });
        final SyncAndServicesSettings syncAndServicesSettings = startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) syncAndServicesSettings.findPreference(
                            SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT);
            Assert.assertFalse(autofillAssistantSwitch.isChecked());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testAutofillAssistantSubsection_ProactiveHelp() {
        final SyncAndServicesSettings syncAndServicesSettings = startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(syncAndServicesSettings.findPreference(
                    SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT));

            Assert.assertTrue(
                    syncAndServicesSettings
                            .findPreference(
                                    SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT_SUBSECTION)
                            .isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void testAutofillAssistantSubsection_AssistantVoiceSeach() {
        final SyncAndServicesSettings syncAndServicesSettings = startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(syncAndServicesSettings.findPreference(
                    SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT));

            Assert.assertTrue(
                    syncAndServicesSettings
                            .findPreference(
                                    SyncAndServicesSettings.PREF_AUTOFILL_ASSISTANT_SUBSECTION)
                            .isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @DisableFeatures(ChromeFeatureList.METRICS_SETTINGS_ANDROID)
    public void testMetricsSettingsHiddenFlagOff() {
        final SyncAndServicesSettings syncAndServicesSettings = startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Metrics settings should be null when the flag is off.",
                    syncAndServicesSettings.findPreference(
                            SyncAndServicesSettings.PREF_METRICS_SETTINGS));
        });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures(ChromeFeatureList.METRICS_SETTINGS_ANDROID)
    public void testMetricsSettingsShownFlagOn() {
        final SyncAndServicesSettings syncAndServicesSettings = startSyncAndServicesPreferences();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull("Metrics settings should exist when the flag is on.",
                    syncAndServicesSettings.findPreference(
                            SyncAndServicesSettings.PREF_METRICS_SETTINGS));
        });
    }

    private void setAutofillAssistantSwitchValue(boolean newValue) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, newValue);
    }

    /**
     * Start SyncAndServicesSettings signin screen and dissmiss it without pressing confirm or
     * cancel.
     */
    private void startPreferencesForAdvancedSyncFlowAndInterruptIt() throws Exception {
        Context context = InstrumentationRegistry.getTargetContext();
        String fragmentName = SyncAndServicesSettings.class.getName();
        final Bundle arguments = SyncAndServicesSettings.createArguments(true);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent =
                settingsLauncher.createSettingsActivityIntent(context, fragmentName, arguments);
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertTrue(activity instanceof SettingsActivity);
        ApplicationTestUtils.finishActivity(activity);
    }

    private SyncAndServicesSettings startSyncAndServicesPreferences() {
        mSettingsActivityTestRule.startSettingsActivity();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return mSettingsActivityTestRule.getFragment();
    }

    private void closeFragment(SyncAndServicesSettings fragment) {
        FragmentTransaction transaction = mSettingsActivityTestRule.getActivity()
                                                  .getSupportFragmentManager()
                                                  .beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void pressBackAndDismissActivity(Activity activity) throws Exception {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressBack();
        ApplicationTestUtils.finishActivity(activity);
    }

    private ChromeSwitchPreference getSyncSwitch(SyncAndServicesSettings fragment) {
        return (ChromeSwitchPreference) fragment.findPreference(
                SyncAndServicesSettings.PREF_SYNC_REQUESTED);
    }

    private Preference getSyncErrorCard(SyncAndServicesSettings fragment) {
        return ((PreferenceCategory) fragment.findPreference(
                        SyncAndServicesSettings.PREF_SYNC_CATEGORY))
                .findPreference(SyncAndServicesSettings.PREF_SYNC_ERROR_CARD);
    }

    private void assertSyncOnState(SyncAndServicesSettings fragment) {
        Assert.assertTrue("The sync switch should be on.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
    }

    private void assertSyncOffState(SyncAndServicesSettings fragment) {
        Assert.assertFalse("The sync switch should be off.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
    }
}
