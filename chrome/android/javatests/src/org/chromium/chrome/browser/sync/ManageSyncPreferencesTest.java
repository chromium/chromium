// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Dialog;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AlertDialog;
import android.support.v7.preference.CheckBoxPreference;
import android.support.v7.preference.Preference;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.sync.ManageSyncPreferences;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.Passphrase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Tests for ManageSyncPreferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSyncPreferencesTest {
    private static final String TAG = "ManageSyncPreferencesTest";

    /**
     * Maps ModelTypes to their UI element IDs.
     */
    private static final Map<Integer, String> UI_DATATYPES = new HashMap<>();

    static {
        UI_DATATYPES.put(ModelType.AUTOFILL, ManageSyncPreferences.PREF_SYNC_AUTOFILL);
        UI_DATATYPES.put(ModelType.BOOKMARKS, ManageSyncPreferences.PREF_SYNC_BOOKMARKS);
        UI_DATATYPES.put(ModelType.TYPED_URLS, ManageSyncPreferences.PREF_SYNC_HISTORY);
        UI_DATATYPES.put(ModelType.PASSWORDS, ManageSyncPreferences.PREF_SYNC_PASSWORDS);
        UI_DATATYPES.put(ModelType.PROXY_TABS, ManageSyncPreferences.PREF_SYNC_RECENT_TABS);
        UI_DATATYPES.put(ModelType.PREFERENCES, ManageSyncPreferences.PREF_SYNC_SETTINGS);
    }

    private Preferences mPreferences;

    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncEverythingAndDataTypes() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        ManageSyncPreferences fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        Collection<CheckBoxPreference> dataTypes = getDataTypes(fragment).values();

        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(syncEverything);
        // When syncEverything is toggled off all data types are checked and enabled by default.
        // User needs to manually uncheck to toggle sync off for data types.
        for (CheckBoxPreference dataType : dataTypes) {
            Assert.assertTrue(dataType.isChecked());
            Assert.assertTrue(dataType.isEnabled());
        }
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSettingDataTypes() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        ManageSyncPreferences fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        Map<Integer, CheckBoxPreference> dataTypes = getDataTypes(fragment);

        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(syncEverything);
        for (CheckBoxPreference dataType : dataTypes.values()) {
            Assert.assertTrue(dataType.isChecked());
            Assert.assertTrue(dataType.isEnabled());
        }

        Set<Integer> expectedTypes = new HashSet<>(dataTypes.keySet());
        assertChosenDataTypesAre(expectedTypes);
        mSyncTestRule.togglePreference(dataTypes.get(ModelType.AUTOFILL));
        mSyncTestRule.togglePreference(dataTypes.get(ModelType.PASSWORDS));
        expectedTypes.remove(ModelType.AUTOFILL);
        expectedTypes.remove(ModelType.PASSWORDS);

        closeFragment(fragment);
        assertChosenDataTypesAre(expectedTypes);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationChecked() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        ManageSyncPreferences fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);

        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationUnchecked() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(false);

        mSyncTestRule.setChosenDataTypes(false, UI_DATATYPES.keySet());
        ManageSyncPreferences fragment = startManageSyncPreferences();

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);

        // All data types are enabled by default as syncEverything is toggled off.
        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxDisablesPaymentsIntegration() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        ManageSyncPreferences fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncTestRule.togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/988622")
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxEnablesPaymentsIntegration() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(false);

        mSyncTestRule.setChosenDataTypes(false, UI_DATATYPES.keySet());
        ManageSyncPreferences fragment = startManageSyncPreferences();

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncTestRule.togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    @DisabledTest(message = "crbug.com/994726")
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        Assert.assertFalse(
                "There should be no server cards", mSyncTestRule.hasServerAutofillCreditCards());
        mSyncTestRule.addServerAutofillCreditCard();
        Assert.assertTrue(
                "There should be server cards", mSyncTestRule.hasServerAutofillCreditCards());

        ManageSyncPreferences fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncTestRule.togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);

        Assert.assertFalse("There should be no server cards remaining",
                mSyncTestRule.hasServerAutofillCreditCards());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationDisabledByAutofillSyncCheckbox() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(true);

        ManageSyncPreferences fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_AUTOFILL);
        mSyncTestRule.togglePreference(syncAutofill);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);
        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationEnabledBySyncEverything() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.setPaymentsIntegrationEnabled(false);
        mSyncTestRule.disableDataType(ModelType.AUTOFILL);

        // Get the UI elements.
        ManageSyncPreferences fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        CheckBoxPreference syncAutofill = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_AUTOFILL);
        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_PAYMENTS_INTEGRATION);

        // All three are unchecked and payments is disabled.
        Assert.assertFalse(syncEverything.isChecked());
        Assert.assertFalse(syncAutofill.isChecked());
        Assert.assertTrue(syncAutofill.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());
        Assert.assertFalse(paymentsIntegration.isEnabled());

        // All three are checked after toggling sync everything.
        mSyncTestRule.togglePreference(syncEverything);
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
        ManageSyncPreferences fragment = startManageSyncPreferences();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseTypeDialogFragment typeFragment = getPassphraseTypeDialogFragment();
        mSyncTestRule.stopSync();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { typeFragment.onItemClick(null, null, 0, Passphrase.Type.CUSTOM); });
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
        final ManageSyncPreferences fragment = startManageSyncPreferences();
        mSyncTestRule.stopSync();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> fragment.onPassphraseEntered("passphrase"));
        // No crash means we passed.
    }

    /**
     * Test that triggering OnPassphraseAccepted dismisses PassphraseDialogFragment.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/986243")
    public void testPassphraseDialogDismissed() {
        final FakeProfileSyncService pss = overrideProfileSyncService();

        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        // Trigger PassphraseDialogFragment to be shown when taping on Encryption.
        pss.setPassphraseRequiredForPreferredDataTypes(true);

        final ManageSyncPreferences fragment = startManageSyncPreferences();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseDialogFragment passphraseFragment = getPassphraseDialogFragment();
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting PassphraseRequired to false
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        pss.setPassphraseRequiredForPreferredDataTypes(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            pss.syncStateChanged();
            fragment.getFragmentManager().executePendingTransactions();
            Assert.assertNull("PassphraseDialogFragment should be dismissed.",
                    mPreferences.getFragmentManager().findFragmentByTag(
                            ManageSyncPreferences.FRAGMENT_ENTER_PASSPHRASE));
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPassphraseCreation() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        SyncTestUtil.waitForSyncActive();
        final ManageSyncPreferences fragment = startManageSyncPreferences();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> fragment.onPassphraseTypeSelected(Passphrase.Type.CUSTOM));
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
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            // PSS has to be constructed on the UI thread.
            FakeProfileSyncService fakeProfileSyncService = new FakeProfileSyncService();
            ProfileSyncService.overrideForTests(fakeProfileSyncService);
            return fakeProfileSyncService;
        });
    }

    private ManageSyncPreferences startManageSyncPreferences() {
        mPreferences = mSyncTestRule.startPreferences(ManageSyncPreferences.class.getName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (ManageSyncPreferences) mPreferences.getMainFragment();
    }

    private void closeFragment(ManageSyncPreferences fragment) {
        FragmentTransaction transaction =
                mPreferences.getSupportFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private ChromeSwitchPreference getSyncEverything(ManageSyncPreferences fragment) {
        return (ChromeSwitchPreference) fragment.findPreference(
                ManageSyncPreferences.PREF_SYNC_EVERYTHING);
    }

    private Map<Integer, CheckBoxPreference> getDataTypes(ManageSyncPreferences fragment) {
        Map<Integer, CheckBoxPreference> dataTypes = new HashMap<>();
        for (Map.Entry<Integer, String> uiDataType : UI_DATATYPES.entrySet()) {
            Integer modelType = uiDataType.getKey();
            String prefId = uiDataType.getValue();
            dataTypes.put(modelType, (CheckBoxPreference) fragment.findPreference(prefId));
        }
        return dataTypes;
    }

    private Preference getGoogleActivityControls(ManageSyncPreferences fragment) {
        return fragment.findPreference(ManageSyncPreferences.PREF_GOOGLE_ACTIVITY_CONTROLS);
    }

    private Preference getEncryption(ManageSyncPreferences fragment) {
        return fragment.findPreference(ManageSyncPreferences.PREF_ENCRYPTION);
    }

    private Preference getManageData(ManageSyncPreferences fragment) {
        return fragment.findPreference(ManageSyncPreferences.PREF_SYNC_MANAGE_DATA);
    }

    private PassphraseDialogFragment getPassphraseDialogFragment() {
        return ActivityUtils.waitForFragment(
                mPreferences, ManageSyncPreferences.FRAGMENT_ENTER_PASSPHRASE);
    }

    private PassphraseTypeDialogFragment getPassphraseTypeDialogFragment() {
        return ActivityUtils.waitForFragment(
                mPreferences, ManageSyncPreferences.FRAGMENT_PASSPHRASE_TYPE);
    }

    private PassphraseCreationDialogFragment getPassphraseCreationDialogFragment() {
        return ActivityUtils.waitForFragment(
                mPreferences, ManageSyncPreferences.FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void assertSyncOnState(ManageSyncPreferences fragment) {
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        Assert.assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        Assert.assertTrue(
                "The sync everything switch should be enabled.", syncEverything.isEnabled());
        for (CheckBoxPreference dataType : getDataTypes(fragment).values()) {
            String key = dataType.getKey();
            Assert.assertTrue("Data type " + key + " should be checked.", dataType.isChecked());
            Assert.assertFalse("Data type " + key + " should be disabled.", dataType.isEnabled());
        }
        Assert.assertTrue("The google activity controls button should always be enabled.",
                getGoogleActivityControls(fragment).isEnabled());
        Assert.assertTrue("The encryption button should always be enabled.",
                getEncryption(fragment).isEnabled());
        Assert.assertTrue("The manage sync data button should be always enabled.",
                getManageData(fragment).isEnabled());
    }

    private void assertChosenDataTypesAre(final Set<Integer> enabledDataTypes) {
        final Set<Integer> disabledDataTypes = new HashSet<>(UI_DATATYPES.keySet());
        disabledDataTypes.removeAll(enabledDataTypes);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> actualDataTypes =
                    mSyncTestRule.getProfileSyncService().getChosenDataTypes();
            Assert.assertTrue(actualDataTypes.containsAll(enabledDataTypes));
            Assert.assertTrue(Collections.disjoint(disabledDataTypes, actualDataTypes));
        });
    }

    private void assertPaymentsIntegrationEnabled(final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(enabled, PersonalDataManager.isPaymentsIntegrationEnabled());
        });
    }

    private void clickPreference(final Preference pref) {
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> pref.getOnPreferenceClickListener().onPreferenceClick(pref));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void clickButton(final Button button) {
        TestThreadUtils.runOnUiThreadBlocking((Runnable) button::performClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void setText(final TextView textView, final String text) {
        TestThreadUtils.runOnUiThreadBlocking(() -> textView.setText(text));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void clearError(final TextView textView) {
        TestThreadUtils.runOnUiThreadBlocking(() -> textView.setError(null));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
