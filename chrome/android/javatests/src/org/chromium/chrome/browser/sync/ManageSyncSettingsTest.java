// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Dialog;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Tests for ManageSyncSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSyncSettingsTest {
    private static final int RENDER_TEST_REVISION = 5;

    /**
     * Maps selected types to their UI element IDs.
     */
    private static final Map<Integer, String> UI_DATATYPES = new HashMap<>();

    static {
        UI_DATATYPES.put(UserSelectableType.AUTOFILL, ManageSyncSettings.PREF_SYNC_AUTOFILL);
        UI_DATATYPES.put(UserSelectableType.BOOKMARKS, ManageSyncSettings.PREF_SYNC_BOOKMARKS);
        UI_DATATYPES.put(
                UserSelectableType.PAYMENTS, ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
        UI_DATATYPES.put(UserSelectableType.HISTORY, ManageSyncSettings.PREF_SYNC_HISTORY);
        UI_DATATYPES.put(UserSelectableType.PASSWORDS, ManageSyncSettings.PREF_SYNC_PASSWORDS);
        UI_DATATYPES.put(
                UserSelectableType.READING_LIST, ManageSyncSettings.PREF_SYNC_READING_LIST);
        UI_DATATYPES.put(UserSelectableType.TABS, ManageSyncSettings.PREF_SYNC_RECENT_TABS);
        UI_DATATYPES.put(UserSelectableType.PREFERENCES, ManageSyncSettings.PREF_SYNC_SETTINGS);
    }

    private SettingsActivity mSettingsActivity;

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private UnifiedConsentServiceBridge.Natives mUnifiedConsentServiceBridgeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                UnifiedConsentServiceBridgeJni.TEST_HOOKS, mUnifiedConsentServiceBridgeMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Mockito.when(mUnifiedConsentServiceBridgeMock.isUrlKeyedAnonymizedDataCollectionEnabled(
                                 Profile.getLastUsedRegularProfile()))
                    .thenReturn(true);
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncEverythingAndDataTypes() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
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
    @MediumTest
    @Feature({"Sync"})
    @Policies.
    Add({ @Policies.Item(key = "SyncTypesListDisabled", string = "[\"passwords\", \"autofill\"]") })
    public void testSyncWithManagedDataTypes() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        Collection<CheckBoxPreference> dataTypes = getDataTypes(fragment).values();

        // When a sync type is disabled by policy, the `Sync everything` toggle should be checked.
        // and the user can still check it off and choose to sync everything that is not managed.
        Assert.assertTrue(syncEverything.isEnabled());
        Assert.assertTrue(syncEverything.isChecked());

        // When one or more sync types are managed, the respective preference should be disabled and
        // not checked, while all other preferences should be user selectable.
        for (CheckBoxPreference dataType : dataTypes) {
            if (dataType.getKey().equals(ManageSyncSettings.PREF_SYNC_PASSWORDS)
                    || dataType.getKey().equals(ManageSyncSettings.PREF_SYNC_AUTOFILL)
                    || dataType.getKey().equals(
                            ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION)) {
                Assert.assertFalse(dataType.isChecked());
                Assert.assertFalse(dataType.isEnabled());
            } else {
                Assert.assertTrue(dataType.isChecked());
                Assert.assertFalse(dataType.isEnabled());
            }
        }

        // Toggle the Sync everything button, and only non-managed types should be enabled.
        mSyncTestRule.togglePreference(syncEverything);

        Assert.assertTrue(syncEverything.isEnabled());
        Assert.assertFalse(syncEverything.isChecked());

        for (CheckBoxPreference dataType : dataTypes) {
            if (dataType.getKey().equals(ManageSyncSettings.PREF_SYNC_PASSWORDS)
                    || dataType.getKey().equals(ManageSyncSettings.PREF_SYNC_AUTOFILL)
                    || dataType.getKey().equals(
                            ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION)) {
                Assert.assertFalse(dataType.isChecked());
                Assert.assertFalse(dataType.isEnabled());
            } else {
                Assert.assertTrue(dataType.isChecked());
                Assert.assertTrue(dataType.isEnabled());
            }
        }

        // Check that the preference shows the managed text.
        onView(withText("Passwords"))
                .check(matches(hasSibling(withText(R.string.managed_by_your_organization))));
        onView(withText("Addresses and more"))
                .check(matches(hasSibling(withText(R.string.managed_by_your_organization))));
        onView(withText("Payment methods, offers, and addresses using Google Pay"))
                .check(matches(hasSibling(withText(R.string.managed_by_your_organization))));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSettingDataTypes() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        Map<Integer, CheckBoxPreference> dataTypes = getDataTypes(fragment);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertFalse(syncEverything.isChecked());
        for (CheckBoxPreference dataType : dataTypes.values()) {
            Assert.assertTrue(dataType.isChecked());
            Assert.assertTrue(dataType.isEnabled());
        }

        Set<Integer> expectedTypes = new HashSet<>(dataTypes.keySet());
        assertSelectedTypesAre(expectedTypes);
        mSyncTestRule.togglePreference(dataTypes.get(UserSelectableType.AUTOFILL));
        mSyncTestRule.togglePreference(dataTypes.get(UserSelectableType.PASSWORDS));
        expectedTypes.remove(UserSelectableType.AUTOFILL);
        expectedTypes.remove(UserSelectableType.PAYMENTS);
        expectedTypes.remove(UserSelectableType.PASSWORDS);

        closeFragment(fragment);
        assertSelectedTypesAre(expectedTypes);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testUnsettingAllDataTypesDoesNotStopSync() {
        // See crbug.com/1291946: The original MICE implementation stopped sync
        // (by setting SyncRequested to false) when the user disabled all data
        // types, for migration / backwards compatibility reasons. As of M104,
        // that's no longer the case.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);
        mSyncTestRule.togglePreference(getSyncEverything(fragment));

        for (CheckBoxPreference dataType : getDataTypes(fragment).values()) {
            mSyncTestRule.togglePreference(dataType);
        }
        // All data types have been unchecked, but Sync itself should still be
        // enabled.
        Assert.assertTrue(SyncTestUtil.isSyncFeatureEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testTogglingSyncEverythingDoesNotStopSync() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.setSelectedTypes(false, new HashSet<>());
        ManageSyncSettings fragment = startManageSyncPreferences();

        // Sync is requested to start. Toggling SyncEverything will call setSelectedTypes with
        // empty set in the backend. But sync stop request should not be called.
        mSyncTestRule.togglePreference(getSyncEverything(fragment));
        Assert.assertTrue(SyncTestUtil.isSyncFeatureEnabled());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testPressingSignOutAndTurnOffSyncShowsSignOutDialog() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();

        Preference turnOffSyncPreference =
                fragment.findPreference(ManageSyncSettings.PREF_TURN_OFF_SYNC);
        Assert.assertTrue("Sign out and turn off sync button should be shown",
                turnOffSyncPreference.isVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                fragment.findPreference(ManageSyncSettings.PREF_TURN_OFF_SYNC)::performClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        onView(withText(R.string.turn_off_sync_and_signout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPressingTurnOffSyncForChildUser() {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();

        // Payments integration should be disabled even though Sync Everything is on
        Set<Integer> forcedUncheckedDataTypes = new HashSet<>();
        forcedUncheckedDataTypes.add(UserSelectableType.PAYMENTS);
        assertSyncOnState(fragment, forcedUncheckedDataTypes);

        Preference turnOffSyncPreference =
                fragment.findPreference(ManageSyncSettings.PREF_TURN_OFF_SYNC);
        Assert.assertTrue(
                "Turn off sync button should be shown", turnOffSyncPreference.isVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                fragment.findPreference(ManageSyncSettings.PREF_TURN_OFF_SYNC)::performClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        onView(withText(R.string.turn_off_sync_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationChecked() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationUnchecked() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        Set<Integer> allDataTypesExceptPayments = new HashSet<>(UI_DATATYPES.keySet());
        allDataTypesExceptPayments.remove(UserSelectableType.PAYMENTS);

        mSyncTestRule.setSelectedTypes(false, allDataTypesExceptPayments);
        ManageSyncSettings fragment = startManageSyncPreferences();

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxDisablesPaymentsIntegration() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncTestRule.togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/988622")
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxEnablesPaymentsIntegration() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.disableDataType(UserSelectableType.PAYMENTS);

        mSyncTestRule.setSelectedTypes(false, UI_DATATYPES.keySet());
        ManageSyncSettings fragment = startManageSyncPreferences();

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncTestRule.togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    @DisabledTest(message = "crbug.com/994726")
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationCheckboxClearsServerAutofillCreditCards() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        Assert.assertFalse(
                "There should be no server cards", mSyncTestRule.hasServerAutofillCreditCards());
        mSyncTestRule.addServerAutofillCreditCard();
        Assert.assertTrue(
                "There should be server cards", mSyncTestRule.hasServerAutofillCreditCards());

        ManageSyncSettings fragment = startManageSyncPreferences();
        assertSyncOnState(fragment);
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
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
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Get the UI elements.
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        CheckBoxPreference syncAutofill =
                (CheckBoxPreference) fragment.findPreference(ManageSyncSettings.PREF_SYNC_AUTOFILL);
        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

        assertSyncOnState(fragment);
        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());

        mSyncTestRule.togglePreference(syncEverything);

        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());

        mSyncTestRule.togglePreference(syncAutofill);

        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertFalse(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationEnabledBySyncEverything() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.disableDataType(UserSelectableType.PAYMENTS);
        mSyncTestRule.disableDataType(UserSelectableType.AUTOFILL);

        // Get the UI elements.
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        CheckBoxPreference syncAutofill =
                (CheckBoxPreference) fragment.findPreference(ManageSyncSettings.PREF_SYNC_AUTOFILL);
        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

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
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        final PassphraseTypeDialogFragment typeFragment = getPassphraseTypeDialogFragment();
        mSyncTestRule.signOut();

        // Mimic the user clicking on the explicit passphrase checkbox immediately after signing
        // out.
        TestThreadUtils.runOnUiThreadBlocking(fragment::onChooseCustomPassphraseRequested);

        // No crash means we passed.
    }

    /**
     * Test that entering a passphrase while sync is off doesn't crash.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testEnterPassphraseWhenSyncIsOff() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        mSyncTestRule.signOut();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> fragment.onPassphraseEntered("passphrase"));
        // No crash means we passed.
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/1188548")
    public void testPassphraseCreation() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        TestThreadUtils.runOnUiThreadBlocking(fragment::onChooseCustomPassphraseRequested);
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

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentIntegrationDisabledForChildUser() {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        CheckBoxPreference paymentsIntegration = (CheckBoxPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

        // Payments integration should be disabled even though Sync Everything is on
        Set<Integer> forcedUncheckedDataTypes = new HashSet<>();
        forcedUncheckedDataTypes.add(UserSelectableType.PAYMENTS);
        assertSyncOnState(fragment, forcedUncheckedDataTypes);

        assertPaymentsIntegrationEnabled(false);
        Assert.assertFalse(paymentsIntegration.isChecked());
        Assert.assertFalse(paymentsIntegration.isEnabled());

        // Turn off Sync Everything
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        mSyncTestRule.togglePreference(syncEverything);

        // Payments integration should stay off
        assertPaymentsIntegrationEnabled(false);
        Assert.assertFalse(paymentsIntegration.isChecked());
        Assert.assertFalse(paymentsIntegration.isEnabled());
    }

    /**
     * Test the trusted vault key retrieval flow, which involves launching an intent and finally
     * calling TrustedVaultClient.notifyKeysChanged().
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTrustedVaultKeyRetrieval() {
        final byte[] trustedVaultKey = new byte[] {1, 2, 3, 4};

        mSyncTestRule.getFakeServerHelper().setTrustedVaultNigori(trustedVaultKey);

        // Keys won't be populated by FakeTrustedVaultClientBackend unless corresponding key
        // retrieval activity is about to be completed.
        SyncTestRule.FakeTrustedVaultClientBackend.get().setKeys(
                Collections.singletonList(trustedVaultKey));

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Initially FakeTrustedVaultClientBackend doesn't provide any keys, so PSS should remain
        // in TrustedVaultKeyRequired state.
        SyncTestUtil.waitForTrustedVaultKeyRequired(true);

        final ManageSyncSettings fragment = startManageSyncPreferences();
        // Mimic the user tapping on Encryption. This should start DummyKeyRetrievalActivity and
        // notify native client that keys were changed. Right before DummyKeyRetrievalActivity
        // completion FakeTrustedVaultClientBackend will start populate keys.
        Preference encryption = fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);
        clickPreference(encryption);

        // Native client should fetch new keys and get out of TrustedVaultKeyRequired state.
        SyncTestUtil.waitForTrustedVaultKeyRequired(false);
    }

    /**
     * Test the trusted vault recoverability fix flow, which involves launching an intent and
     * finally calling TrustedVaultClient.notifyRecoverabilityChanged().
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTrustedVaultRecoverabilityFix() {
        final byte[] trustedVaultKey = new byte[] {1, 2, 3, 4};

        mSyncTestRule.getFakeServerHelper().setTrustedVaultNigori(trustedVaultKey);

        // Mimic retrieval having completed earlier.
        SyncTestRule.FakeTrustedVaultClientBackend.get().setKeys(
                Collections.singletonList(trustedVaultKey));
        SyncTestRule.FakeTrustedVaultClientBackend.get().startPopulateKeys();

        SyncTestRule.FakeTrustedVaultClientBackend.get().setRecoverabilityDegraded(true);

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Initially recoverability should be reported as degraded.
        SyncTestUtil.waitForTrustedVaultRecoverabilityDegraded(true);

        // Mimic the user tapping on the error card's button. This should start
        // DummyRecoverabilityDegradedFixActivity and notify native client that recoverability has
        // changed. Right before DummyRecoverabilityDegradedFixActivity completion
        // FakeTrustedVaultClientBackend will exit the recoverability degraded state.
        final ManageSyncSettings fragment = startManageSyncPreferences();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { fragment.onSyncErrorCardPrimaryButtonClicked(); });

        // Native client should fetch the new recoverability state and get out of the
        // degraded-recoverability state.
        SyncTestUtil.waitForTrustedVaultRecoverabilityDegraded(false);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowPreferencesAndBottomBarShown() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        Assert.assertTrue(
                fragment.findPreference(ManageSyncSettings.PREF_SYNCING_CATEGORY).isVisible());
        Assert.assertTrue(
                fragment.findPreference(ManageSyncSettings.PREF_SEARCH_AND_BROWSE_CATEGORY)
                        .isVisible());
        Assert.assertNotNull(fragment.getView().findViewById(R.id.bottom_bar_shadow));
        Assert.assertNotNull(fragment.getView().findViewById(R.id.bottom_bar_button_container));
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowTopView() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        render(fragment, "advanced_sync_flow_top_view");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowBottomView() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            // Sometimes the rendered image may not contain the scrollbar and cause flakiness.
            // Hide the scrollbar altogether to reduce flakiness.
            recyclerView.setVerticalScrollBarEnabled(false);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        render(fragment, "advanced_sync_flow_bottom_view");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowFromSyncConsentTopView() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        render(fragment, "advanced_sync_flow_top_view_from_sync_consent");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowFromSyncConsentBottomView() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            // Sometimes the rendered image may not contain the scrollbar and cause flakiness.
            // Hide the scrollbar altogether to reduce flakiness.
            recyclerView.setVerticalScrollBarEnabled(false);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        render(fragment, "advanced_sync_flow_bottom_view_from_sync_consent");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowTopViewForChildUser() throws Exception {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        render(fragment, "advanced_sync_flow_top_view_child");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowBottomViewForChildUser() throws Exception {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            // Sometimes the rendered image may not contain the scrollbar and cause flakiness.
            // Hide the scrollbar altogether to reduce flakiness.
            recyclerView.setVerticalScrollBarEnabled(false);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        render(fragment, "advanced_sync_flow_bottom_view_child");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowFromSyncConsentTopViewForChildUser() throws Exception {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        render(fragment, "advanced_sync_flow_top_view_from_sync_consent_child");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testAdvancedSyncFlowFromSyncConsentBottomViewForChildUser() throws Exception {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            // Sometimes the rendered image may not contain the scrollbar and cause flakiness.
            // Hide the scrollbar altogether to reduce flakiness.
            recyclerView.setVerticalScrollBarEnabled(false);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        render(fragment, "advanced_sync_flow_bottom_view_from_sync_consent_child");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    @Policies.Add({
        @Policies.Item(key = "SyncTypesListDisabled",
                string =
                        "[\"bookmarks\", \"readingList\", \"preferences\", \"passwords\", \"autofill\", \"typedUrls\", \"tabs\"]")
    })
    public void
    testSyncSettingsTopViewWithSyncTypesManagedByPolicy() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        render(fragment, "sync_settings_top_view_with_sync_types_disabled_by_policy");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    @Policies.Add({
        @Policies.Item(key = "SyncTypesListDisabled",
                string =
                        "[\"bookmarks\", \"readingList\", \"preferences\", \"passwords\", \"autofill\", \"typedUrls\", \"tabs\"]")
    })
    public void
    testSyncSettingsBottomViewWithSyncTypesManagedByPolicy() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        render(fragment, "sync_settings_bottom_view_with_sync_types_disabled_by_policy");
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentDoesNotEnableUKM() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        ChromeSwitchPreference urlKeyedAnonymizedData = getUrlKeyedAnonymizedData(fragment);

        Assert.assertTrue(urlKeyedAnonymizedData.isChecked());
        verifyUrlKeyedAnonymizedDataCollectionNotSet();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentForSupervisedUserWithUKMEnabled()
            throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Mockito.when(mUnifiedConsentServiceBridgeMock.isUrlKeyedAnonymizedDataCollectionManaged(
                                 Profile.getLastUsedRegularProfile()))
                    .thenReturn(true);
            Mockito.when(mUnifiedConsentServiceBridgeMock.isUrlKeyedAnonymizedDataCollectionEnabled(
                                 Profile.getLastUsedRegularProfile()))
                    .thenReturn(true);
        });

        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        ChromeSwitchPreference urlKeyedAnonymizedData = getUrlKeyedAnonymizedData(fragment);

        Assert.assertTrue(urlKeyedAnonymizedData.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentForSupervisedUserWithUKMDisabled()
            throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Mockito.when(mUnifiedConsentServiceBridgeMock.isUrlKeyedAnonymizedDataCollectionManaged(
                                 Profile.getLastUsedRegularProfile()))
                    .thenReturn(true);
            Mockito.when(mUnifiedConsentServiceBridgeMock.isUrlKeyedAnonymizedDataCollectionEnabled(
                                 Profile.getLastUsedRegularProfile()))
                    .thenReturn(false);
        });

        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        ChromeSwitchPreference urlKeyedAnonymizedData = getUrlKeyedAnonymizedData(fragment);

        Assert.assertFalse(urlKeyedAnonymizedData.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentConfirmEnablesUKM() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        Button confirmButton = fragment.getView().findViewById(R.id.confirm_button);
        clickButton(confirmButton);

        verifyUrlKeyedAnonymizedDataCollectionSet();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentMSBBToggledOffDoesNotEnableUKM()
            throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        ChromeSwitchPreference urlKeyedAnonymizedData = getUrlKeyedAnonymizedData(fragment);
        Button confirmButton = fragment.getView().findViewById(R.id.confirm_button);

        Assert.assertTrue(urlKeyedAnonymizedData.isChecked());
        mSyncTestRule.togglePreference(urlKeyedAnonymizedData);
        Assert.assertFalse(urlKeyedAnonymizedData.isChecked());

        clickButton(confirmButton);

        verifyUrlKeyedAnonymizedDataCollectionNotSet();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentBackPressDoesNotEnableUKM() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();

        TestThreadUtils.runOnUiThreadBlocking(() -> { fragment.onBackPressed(); });

        verifyUrlKeyedAnonymizedDataCollectionNotSet();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentCancelDoesNotEnableUKM() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();
        Button cancelButton = fragment.getView().findViewById(R.id.cancel_button);

        clickButton(cancelButton);

        verifyUrlKeyedAnonymizedDataCollectionNotSet();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAdvancedSyncFlowFromSyncConsentFragmentCloseDoesNotEnableUKM()
            throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();

        closeFragment(fragment);

        verifyUrlKeyedAnonymizedDataCollectionNotSet();
    }

    private ManageSyncSettings startManageSyncPreferences() {
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }

    private ManageSyncSettings startManageSyncPreferencesFromSyncConsentFlow() {
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity(
                ManageSyncSettings.createArguments(true));
        return mSettingsActivityTestRule.getFragment();
    }

    private void closeFragment(ManageSyncSettings fragment) {
        FragmentTransaction transaction =
                mSettingsActivity.getSupportFragmentManager().beginTransaction();
        transaction.remove(fragment);
        transaction.commit();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private ChromeSwitchPreference getSyncEverything(ManageSyncSettings fragment) {
        return (ChromeSwitchPreference) fragment.findPreference(
                ManageSyncSettings.PREF_SYNC_EVERYTHING);
    }

    private ChromeSwitchPreference getUrlKeyedAnonymizedData(ManageSyncSettings fragment) {
        return (ChromeSwitchPreference) fragment.findPreference(
                ManageSyncSettings.PREF_URL_KEYED_ANONYMIZED_DATA);
    }

    private Map<Integer, CheckBoxPreference> getDataTypes(ManageSyncSettings fragment) {
        Map<Integer, CheckBoxPreference> dataTypes = new HashMap<>();
        for (Map.Entry<Integer, String> uiDataType : UI_DATATYPES.entrySet()) {
            Integer selectedType = uiDataType.getKey();
            String prefId = uiDataType.getValue();
            dataTypes.put(selectedType, (CheckBoxPreference) fragment.findPreference(prefId));
        }
        return dataTypes;
    }

    private Preference getGoogleActivityControls(ManageSyncSettings fragment) {
        return fragment.findPreference(ManageSyncSettings.PREF_GOOGLE_ACTIVITY_CONTROLS);
    }

    private Preference getEncryption(ManageSyncSettings fragment) {
        return fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);
    }

    private Preference getReviewData(ManageSyncSettings fragment) {
        return fragment.findPreference(ManageSyncSettings.PREF_SYNC_REVIEW_DATA);
    }

    private PassphraseTypeDialogFragment getPassphraseTypeDialogFragment() {
        return ActivityTestUtils.waitForFragment(
                mSettingsActivity, ManageSyncSettings.FRAGMENT_PASSPHRASE_TYPE);
    }

    private PassphraseCreationDialogFragment getPassphraseCreationDialogFragment() {
        return ActivityTestUtils.waitForFragment(
                mSettingsActivity, ManageSyncSettings.FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void assertSyncOnState(ManageSyncSettings fragment) {
        assertSyncOnState(fragment, new HashSet<Integer>());
    }

    private void assertSyncOnState(
            ManageSyncSettings fragment, Set<Integer> forcedUncheckedDataTypes) {
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        Assert.assertTrue("The sync everything switch should be on.", syncEverything.isChecked());
        Assert.assertTrue(
                "The sync everything switch should be enabled.", syncEverything.isEnabled());
        for (Map.Entry<Integer, CheckBoxPreference> dataType : getDataTypes(fragment).entrySet()) {
            CheckBoxPreference checkBox = dataType.getValue();
            String key = checkBox.getKey();
            Assert.assertFalse("Data type " + key + " should be disabled.", checkBox.isEnabled());
            if (forcedUncheckedDataTypes.contains(dataType.getKey())) {
                Assert.assertFalse(
                        "Data type " + key + " should be unchecked.", checkBox.isChecked());
            } else {
                Assert.assertTrue("Data type " + key + " should be checked.", checkBox.isChecked());
            }
        }
        Assert.assertTrue("The google activity controls button should always be enabled.",
                getGoogleActivityControls(fragment).isEnabled());
        Assert.assertTrue("The encryption button should always be enabled.",
                getEncryption(fragment).isEnabled());
        Assert.assertTrue("The review your synced data button should be always enabled.",
                getReviewData(fragment).isEnabled());
    }

    private void assertSelectedTypesAre(final Set<Integer> enabledDataTypes) {
        final Set<Integer> disabledDataTypes = new HashSet<>(UI_DATATYPES.keySet());
        disabledDataTypes.removeAll(enabledDataTypes);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> actualDataTypes = mSyncTestRule.getSyncService().getSelectedTypes();
            Assert.assertTrue(actualDataTypes.containsAll(enabledDataTypes));
            Assert.assertTrue(Collections.disjoint(disabledDataTypes, actualDataTypes));
        });
    }

    private void assertPaymentsIntegrationEnabled(final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> actualDataTypes = mSyncTestRule.getSyncService().getSelectedTypes();
            if (enabled) {
                Assert.assertTrue(actualDataTypes.contains(UserSelectableType.PAYMENTS));
            } else {
                Assert.assertFalse(actualDataTypes.contains(UserSelectableType.PAYMENTS));
            }
        });
    }

    private void verifyUrlKeyedAnonymizedDataCollectionSet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            Mockito.verify(mUnifiedConsentServiceBridgeMock, Mockito.atLeastOnce())
                    .setUrlKeyedAnonymizedDataCollectionEnabled(profile, true);
        });
    }

    private void verifyUrlKeyedAnonymizedDataCollectionNotSet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            Mockito.verify(mUnifiedConsentServiceBridgeMock, Mockito.never())
                    .setUrlKeyedAnonymizedDataCollectionEnabled(profile, true);
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

    private void render(ManageSyncSettings fragment, String skiaGoldId) throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        // Sanitize the view, in particular to ensure the presence of scroll bars do not cause
        // image diffs.
        ChromeRenderTestRule.sanitize(fragment.getView());
        mRenderTestRule.render(fragment.getView(), skiaGoldId);
    }
}
