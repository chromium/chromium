// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.app.FragmentTransaction;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.SwitchPreference;
import android.preference.TwoStatePreference;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.sync.protocol.AutofillWalletSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.sync.protocol.WalletMaskedCreditCard;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Tests for SyncCustomizationFragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@SuppressLint("UseSparseArrays")
public class SyncCustomizationFragmentTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String TAG = "SyncCustomizationFragmentTest";

    /**
     * Fake ProfileSyncService for test to control the value returned from
     * isPassphraseRequiredForDecryption.
     */
    private class FakeProfileSyncService extends ProfileSyncService {
        private boolean mPassphraseRequiredForDecryption;

        FakeProfileSyncService() {
            super();
            setMasterSyncEnabledProvider(
                    () -> { return AndroidSyncSettings.isMasterSyncEnabled(); });
        }

        @Override
        public boolean isPassphraseRequiredForDecryption() {
            return mPassphraseRequiredForDecryption;
        }

        void setPassphraseRequiredForDecryption(boolean passphraseRequiredForDecryption) {
            mPassphraseRequiredForDecryption = passphraseRequiredForDecryption;
        }
    }

    /**
     * Maps ModelTypes to their UI element IDs.
     */
    private static final Map<Integer, String> UI_DATATYPES;

    static {
        UI_DATATYPES = new HashMap<>();
        UI_DATATYPES.put(ModelType.AUTOFILL, SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        UI_DATATYPES.put(ModelType.BOOKMARKS, SyncCustomizationFragment.PREFERENCE_SYNC_BOOKMARKS);
        UI_DATATYPES.put(ModelType.TYPED_URLS, SyncCustomizationFragment.PREFERENCE_SYNC_OMNIBOX);
        UI_DATATYPES.put(ModelType.PASSWORDS, SyncCustomizationFragment.PREFERENCE_SYNC_PASSWORDS);
        UI_DATATYPES.put(ModelType.PROXY_TABS,
                SyncCustomizationFragment.PREFERENCE_SYNC_RECENT_TABS);
        UI_DATATYPES.put(ModelType.PREFERENCES,
                SyncCustomizationFragment.PREFERENCE_SYNC_SETTINGS);
    }

    private Preferences mPreferences;

    @Before
    public void setUp() throws Exception {
        mPreferences = null;
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    /**
     * Minimal test fixture to debug flakiness in the actual tests.
     * TODO(crbug.com/879246): Remove after investigation.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSetupOnly() {
        mSyncTestRule.setUpTestAccountAndSignIn();
    }

    /**
     * Minimal test fixture to debug flakiness in the actual tests.
     * TODO(crbug.com/879246): Remove after investigation.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSetupAndWaitForSyncActive() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitch() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        final SwitchPreference syncSwitch = getSyncSwitch(fragment);

        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.isChromeSyncEnabled());
        togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.isChromeSyncEnabled());
        togglePreference(syncSwitch);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.isChromeSyncEnabled());
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
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        closeFragment(fragment);
        Assert.assertFalse(AndroidSyncSettings.isChromeSyncEnabled());
    }

    /**
     * This is a regression test for http://crbug.com/467600.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest
    public void testOpeningSettingsDoesntStartEngine() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        startSyncCustomizationFragment();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mSyncTestRule.getProfileSyncService().isSyncRequested());
            Assert.assertFalse(mSyncTestRule.getProfileSyncService().isEngineInitialized());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest
    public void testDefaultControlStatesWithSyncOffThenOn() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOffState(fragment);
        togglePreference(getSyncSwitch(fragment));
        SyncTestUtil.waitForEngineInitialized();
        assertDefaultSyncOnState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDefaultControlStatesWithSyncOnThenOff() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        togglePreference(getSyncSwitch(fragment));
        assertDefaultSyncOffState(fragment);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncEverythingAndDataTypes() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        SwitchPreference syncEverything = getSyncEverything(fragment);
        Collection<CheckBoxPreference> dataTypes = getDataTypes(fragment).values();

        assertDefaultSyncOnState(fragment);
        togglePreference(syncEverything);
        for (CheckBoxPreference dataType : dataTypes) {
            Assert.assertTrue(dataType.isChecked());
            Assert.assertTrue(dataType.isEnabled());
        }

        // If all data types are unchecked, sync should turn off.
        for (CheckBoxPreference dataType : dataTypes) {
            togglePreference(dataType);
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertDefaultSyncOffState(fragment);
        Assert.assertFalse(AndroidSyncSettings.isChromeSyncEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSettingDataTypes() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        SwitchPreference syncEverything = getSyncEverything(fragment);
        Map<Integer, CheckBoxPreference> dataTypes = getDataTypes(fragment);

        assertDefaultSyncOnState(fragment);
        togglePreference(syncEverything);
        for (CheckBoxPreference dataType : dataTypes.values()) {
            Assert.assertTrue(dataType.isChecked());
            Assert.assertTrue(dataType.isEnabled());
        }

        Set<Integer> expectedTypes = new HashSet<>(dataTypes.keySet());
        expectedTypes.add(ModelType.PREFERENCES);
        expectedTypes.add(ModelType.PRIORITY_PREFERENCES);
        assertDataTypesAre(expectedTypes);
        togglePreference(dataTypes.get(ModelType.AUTOFILL));
        togglePreference(dataTypes.get(ModelType.PASSWORDS));
        // Nothing should have changed before the fragment closes.
        assertDataTypesAre(expectedTypes);

        closeFragment(fragment);
        expectedTypes.remove(ModelType.AUTOFILL);
        expectedTypes.remove(ModelType.PASSWORDS);
        assertDataTypesAre(expectedTypes);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationChecked() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(true);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);

        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationUnchecked() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(false);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);

        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxDisablesPaymentsIntegration() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(true);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxEnablesPaymentsIntegration() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(false);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(true);

        Assert.assertFalse("There should be no server cards", hasServerAutofillCreditCards());
        addServerAutofillCreditCard();
        Assert.assertTrue("There should be server cards", hasServerAutofillCreditCards());

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);

        Assert.assertFalse(
                "There should be no server cards remaining", hasServerAutofillCreditCards());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSwitchClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(true);

        Assert.assertFalse("There should be no server cards", hasServerAutofillCreditCards());
        addServerAutofillCreditCard();
        Assert.assertTrue("There should be server cards", hasServerAutofillCreditCards());

        Assert.assertTrue(AndroidSyncSettings.isChromeSyncEnabled());
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncSwitch = getSyncSwitch(fragment);
        Assert.assertTrue(syncSwitch.isChecked());
        Assert.assertTrue(AndroidSyncSettings.isChromeSyncEnabled());
        togglePreference(syncSwitch);
        Assert.assertFalse(syncSwitch.isChecked());
        Assert.assertFalse(AndroidSyncSettings.isChromeSyncEnabled());

        closeFragment(fragment);

        Assert.assertFalse(
                "There should be no server cards remaining", hasServerAutofillCreditCards());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationDisabledByAutofillSyncCheckbox() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(true);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        togglePreference(syncAutofill);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationEnabledByAutofillSyncCheckbox() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        setPaymentsIntegrationEnabled(false);

        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        assertDefaultSyncOnState(fragment);
        SwitchPreference syncEverything = getSyncEverything(fragment);
        togglePreference(syncEverything);

        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        togglePreference(syncAutofill);  // Disable autofill sync.
        togglePreference(syncAutofill);  // Re-enable autofill sync again.

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);
        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationEnabledBySyncEverything() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        setPaymentsIntegrationEnabled(false);
        mSyncTestRule.disableDataType(ModelType.AUTOFILL);

        // Get the UI elements.
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        SwitchPreference syncEverything = getSyncEverything(fragment);
        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_AUTOFILL);
        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_PAYMENTS_INTEGRATION);

        // All three are unchecked and payments is disabled.
        Assert.assertFalse(syncEverything.isChecked());
        Assert.assertFalse(syncAutofill.isChecked());
        Assert.assertTrue(syncAutofill.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());
        Assert.assertFalse(paymentsIntegration.isEnabled());

        // All three are checked after toggling sync everything.
        togglePreference(syncEverything);
        Assert.assertTrue(syncEverything.isChecked());
        Assert.assertTrue(syncAutofill.isChecked());
        Assert.assertFalse(syncAutofill.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());
        Assert.assertFalse(paymentsIntegration.isEnabled());

        // Closing the fragment enabled payments integration.
        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    /**
     * Test that choosing a passphrase type while sync is off doesn't crash.
     *
     * This is a regression test for http://crbug.com/507557.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testChoosePassphraseTypeWhenSyncIsOff() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseTypeDialogFragment typeFragment = getPassphraseTypeDialogFragment();
        mSyncTestRule.stopSync();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            typeFragment.onItemClick(
                    null, null, 0, PassphraseType.CUSTOM_PASSPHRASE.internalValue());
        });
        // No crash means we passed.
    }

    /**
     * Test that entering a passphrase while sync is off doesn't crash.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testEnterPassphraseWhenSyncIsOff() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        final SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        mSyncTestRule.stopSync();
        ThreadUtils.runOnUiThreadBlockingNoException(
                () -> fragment.onPassphraseEntered("passphrase"));
        // No crash means we passed.
    }

    /**
     * Test that triggering OnPassphraseAccepted dismisses PassphraseDialogFragment.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest
    public void testPassphraseDialogDismissed() {
        final FakeProfileSyncService pss = overrideProfileSyncService();

        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        // Trigger PassphraseDialogFragment to be shown when taping on Encryption.
        pss.setPassphraseRequiredForDecryption(true);

        final SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseDialogFragment passphraseFragment = getPassphraseDialogFragment();
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting PassphraseRequired to false
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        pss.setPassphraseRequiredForDecryption(false);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            pss.syncStateChanged();
            fragment.getFragmentManager().executePendingTransactions();
            Assert.assertNull("PassphraseDialogFragment should be dismissed.",
                    mPreferences.getFragmentManager().findFragmentByTag(
                            SyncCustomizationFragment.FRAGMENT_ENTER_PASSPHRASE));
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPassphraseCreation() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        final SyncCustomizationFragment fragment = startSyncCustomizationFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> fragment.onPassphraseTypeSelected(PassphraseType.CUSTOM_PASSPHRASE));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        PassphraseCreationDialogFragment pcdf = getPassphraseCreationDialogFragment();
        AlertDialog dialog = (AlertDialog) pcdf.getDialog();
        Button okButton = dialog.getButton(Dialog.BUTTON_POSITIVE);
        EditText enterPassphrase = (EditText) dialog.findViewById(R.id.passphrase);
        EditText confirmPassphrase = (EditText) dialog.findViewById(R.id.confirm_passphrase);

        // Error if you try to submit empty passphrase.
        Assert.assertNull(confirmPassphrase.getError());
        clickButton(okButton);
        Assert.assertTrue(pcdf.isResumed());
        Assert.assertNotNull(enterPassphrase.getError());
        Assert.assertNull(confirmPassphrase.getError());

        // Error if you try to submit with only the first box filled.
        clearError(confirmPassphrase);
        setText(enterPassphrase, "foo");
        clickButton(okButton);
        Assert.assertTrue(pcdf.isResumed());
        Assert.assertNull(enterPassphrase.getError());
        Assert.assertNotNull(confirmPassphrase.getError());

        // Remove first box should only show empty error message
        setText(enterPassphrase, "");
        clickButton(okButton);
        Assert.assertNotNull(enterPassphrase.getError());
        Assert.assertNull(confirmPassphrase.getError());

        // Error if you try to submit with only the second box filled.
        clearError(confirmPassphrase);
        setText(confirmPassphrase, "foo");
        clickButton(okButton);
        Assert.assertTrue(pcdf.isResumed());
        Assert.assertNull(enterPassphrase.getError());
        Assert.assertNotNull(confirmPassphrase.getError());

        // No error if text doesn't match without button press.
        setText(enterPassphrase, "foo");
        clearError(confirmPassphrase);
        setText(confirmPassphrase, "bar");
        Assert.assertNull(enterPassphrase.getError());
        Assert.assertNull(confirmPassphrase.getError());

        // Error if you try to submit unmatching text.
        clearError(confirmPassphrase);
        clickButton(okButton);
        Assert.assertTrue(pcdf.isResumed());
        Assert.assertNull(enterPassphrase.getError());
        Assert.assertNotNull(confirmPassphrase.getError());

        // Success if text matches.
        setText(confirmPassphrase, "foo");
        clickButton(okButton);
        Assert.assertFalse(pcdf.isResumed());
    }

    private FakeProfileSyncService overrideProfileSyncService() {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            // PSS has to be constructed on the UI thread.
            FakeProfileSyncService fakeProfileSyncService = new FakeProfileSyncService();
            ProfileSyncService.overrideForTests(fakeProfileSyncService);
            return fakeProfileSyncService;
        });
    }

    private SyncCustomizationFragment startSyncCustomizationFragment() {
        mPreferences = mSyncTestRule.startPreferences(SyncCustomizationFragment.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (SyncCustomizationFragment) mPreferences.getFragmentForTest();
    }

    private void closeFragment(SyncCustomizationFragment fragment) {
        FragmentTransaction transaction = mPreferences.getFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private SwitchPreference getSyncSwitch(SyncCustomizationFragment fragment) {
        return (SwitchPreference) fragment.findPreference(
                SyncCustomizationFragment.PREF_SYNC_SWITCH);
    }

    private SwitchPreference getSyncEverything(SyncCustomizationFragment fragment) {
        return (SwitchPreference) fragment.findPreference(
                SyncCustomizationFragment.PREFERENCE_SYNC_EVERYTHING);
    }

    private Map<Integer, CheckBoxPreference> getDataTypes(SyncCustomizationFragment fragment) {
        Map<Integer, CheckBoxPreference> dataTypes = new HashMap<>();
        for (Map.Entry<Integer, String> uiDataType : UI_DATATYPES.entrySet()) {
            Integer modelType = uiDataType.getKey();
            String prefId = uiDataType.getValue();
            dataTypes.put(modelType, (CheckBoxPreference) fragment.findPreference(prefId));
        }
        return dataTypes;
    }

    private Preference getEncryption(SyncCustomizationFragment fragment) {
        return fragment.findPreference(SyncCustomizationFragment.PREFERENCE_ENCRYPTION);
    }

    private Preference getManageData(SyncCustomizationFragment fragment) {
        return fragment.findPreference(SyncCustomizationFragment.PREFERENCE_SYNC_MANAGE_DATA);
    }

    private PassphraseDialogFragment getPassphraseDialogFragment() {
        return ActivityUtils.waitForFragment(
                mPreferences, SyncCustomizationFragment.FRAGMENT_ENTER_PASSPHRASE);
    }

    private PassphraseTypeDialogFragment getPassphraseTypeDialogFragment() {
        return ActivityUtils.waitForFragment(
                mPreferences, SyncCustomizationFragment.FRAGMENT_PASSPHRASE_TYPE);
    }

    private PassphraseCreationDialogFragment getPassphraseCreationDialogFragment() {
        return ActivityUtils.waitForFragment(
                mPreferences, SyncCustomizationFragment.FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void assertDefaultSyncOnState(SyncCustomizationFragment fragment) {
        Assert.assertTrue("The sync switch should be on.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
        SwitchPreference syncEverything = getSyncEverything(fragment);
        Assert.assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        Assert.assertTrue(
                "The sync everything switch should be enabled.", syncEverything.isEnabled());
        for (CheckBoxPreference dataType : getDataTypes(fragment).values()) {
            String key = dataType.getKey();
            Assert.assertTrue("Data type " + key + " should be checked.", dataType.isChecked());
            Assert.assertFalse("Data type " + key + " should be disabled.", dataType.isEnabled());
        }
        Assert.assertTrue(
                "The encryption button should be enabled.", getEncryption(fragment).isEnabled());
        Assert.assertTrue("The manage sync data button should be always enabled.",
                getManageData(fragment).isEnabled());
    }

    private void assertDefaultSyncOffState(SyncCustomizationFragment fragment) {
        Assert.assertFalse("The sync switch should be off.", getSyncSwitch(fragment).isChecked());
        Assert.assertTrue(
                "The sync switch should be enabled.", getSyncSwitch(fragment).isEnabled());
        SwitchPreference syncEverything = getSyncEverything(fragment);
        Assert.assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        Assert.assertFalse(
                "The sync everything switch should be disabled.", syncEverything.isEnabled());
        for (CheckBoxPreference dataType : getDataTypes(fragment).values()) {
            String key = dataType.getKey();
            Assert.assertTrue("Data type " + key + " should be checked.", dataType.isChecked());
            Assert.assertFalse("Data type " + key + " should be disabled.", dataType.isEnabled());
        }
        Assert.assertFalse(
                "The encryption button should be disabled.", getEncryption(fragment).isEnabled());
        Assert.assertTrue("The manage sync data button should be always enabled.",
                getManageData(fragment).isEnabled());
    }

    private void assertDataTypesAre(final Set<Integer> enabledDataTypes) {
        final Set<Integer> disabledDataTypes = new HashSet<>(UI_DATATYPES.keySet());
        disabledDataTypes.removeAll(enabledDataTypes);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> actualDataTypes =
                    mSyncTestRule.getProfileSyncService().getPreferredDataTypes();
            Assert.assertTrue(actualDataTypes.containsAll(enabledDataTypes));
            Assert.assertTrue(Collections.disjoint(disabledDataTypes, actualDataTypes));
        });
    }

    private void setPaymentsIntegrationEnabled(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.setPaymentsIntegrationEnabled(enabled));
    }

    private void assertPaymentsIntegrationEnabled(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(enabled, PersonalDataManager.isPaymentsIntegrationEnabled());
        });
    }

    private void addServerAutofillCreditCard() {
        final String serverId = "025eb937c022489eb8dc78cbaa969218";
        WalletMaskedCreditCard card =
                WalletMaskedCreditCard.newBuilder()
                        .setId(serverId)
                        .setStatus(WalletMaskedCreditCard.WalletCardStatus.VALID)
                        .setNameOnCard("Jon Doe")
                        .setType(WalletMaskedCreditCard.WalletCardType.UNKNOWN)
                        .setLastFour("1111")
                        .setExpMonth(11)
                        .setExpYear(2020)
                        .build();
        AutofillWalletSpecifics wallet_specifics =
                AutofillWalletSpecifics.newBuilder()
                        .setType(AutofillWalletSpecifics.WalletInfoType.MASKED_CREDIT_CARD)
                        .setMaskedCard(card)
                        .build();
        EntitySpecifics specifics =
                EntitySpecifics.newBuilder().setAutofillWallet(wallet_specifics).build();
        SyncEntity entity = SyncEntity.newBuilder()
                                    .setName(serverId)
                                    .setIdString(serverId)
                                    .setSpecifics(specifics)
                                    .build();
        mSyncTestRule.getFakeServerHelper().setWalletData(entity);
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    private boolean hasServerAutofillCreditCards() {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            List<CreditCard> cards = PersonalDataManager.getInstance().getCreditCardsForSettings();
            for (int i = 0; i < cards.size(); i++) {
                if (!cards.get(i).getIsLocal()) return true;
            }
            return false;
        });
    }

    // UI interaction convenience methods.
    private void togglePreference(final TwoStatePreference pref) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            boolean newValue = !pref.isChecked();
            pref.getOnPreferenceChangeListener().onPreferenceChange(pref, newValue);
            pref.setChecked(newValue);
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void clickPreference(final Preference pref) {
        ThreadUtils.runOnUiThreadBlockingNoException(
                () -> pref.getOnPreferenceClickListener().onPreferenceClick(pref));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void clickButton(final Button button) {
        ThreadUtils.runOnUiThreadBlocking((Runnable) button::performClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void setText(final TextView textView, final String text) {
        ThreadUtils.runOnUiThreadBlocking(() -> textView.setText(text));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void clearError(final TextView textView) {
        ThreadUtils.runOnUiThreadBlocking(() -> textView.setError(null));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
