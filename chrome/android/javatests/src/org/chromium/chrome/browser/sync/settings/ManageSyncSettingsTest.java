// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import static java.util.Map.entry;

import android.app.Dialog;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.extensions.ExtensionsBuildflags;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionUiBackendRule;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.TransportState;
import org.chromium.components.sync.UserActionableError;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync.internal.SyncPrefNames;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Tests for ManageSyncSettings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "TODO(crbug.com/40743432): SyncTestRule doesn't support batching.")
// Avoids UserActionableError.NEEDS_UPM_BACKEND_UPGRADE for most tests. Specific tests can still
// trigger the error by overriding getUserActionableError()
@Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class ManageSyncSettingsTest {
    private static final int RENDER_TEST_REVISION = 9;

    /** Maps selected types to their Account UI element IDs. */
    private static final Map<Integer, String> ACCOUNT_UI_DATATYPES =
            Map.ofEntries(
                    entry(
                            UserSelectableType.AUTOFILL,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_ADDRESSES_TOGGLE),
                    entry(
                            UserSelectableType.BOOKMARKS,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_BOOKMARKS_TOGGLE),
                    // NOTE: EXTENSIONS are only available in the desktop Android build.
                    entry(
                            UserSelectableType.EXTENSIONS,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_EXTENSIONS_TOGGLE),
                    entry(
                            UserSelectableType.PAYMENTS,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_PAYMENTS_TOGGLE),
                    // HISTORY and TABS are bundled in the same switch in the new settings panel.
                    entry(
                            UserSelectableType.HISTORY,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE),
                    entry(
                            UserSelectableType.TABS,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE),
                    entry(
                            UserSelectableType.PASSWORDS,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE),
                    entry(
                            UserSelectableType.READING_LIST,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_READING_LIST_TOGGLE),
                    entry(
                            UserSelectableType.PREFERENCES,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_SETTINGS_TOGGLE));

    private SettingsActivity mSettingsActivity;

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    public final FakeExtensionUiBackendRule mFakeExtensionUiBackendRule =
            new FakeExtensionUiBackendRule();

    @Mock private UnifiedConsentServiceBridge.Natives mUnifiedConsentServiceBridgeMock;
    @Mock private RegionalCapabilitiesService mRegionalCapabilities;
    @Mock private GoogleActivityController mGoogleActivityController;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private HistorySyncHelper mHistorySyncHelperMock;
    @Mock private SyncService mSyncService;
    @Mock private ReauthenticatorBridge mReauthenticatorMock;

    @Before
    public void setUp() {
        UnifiedConsentServiceBridgeJni.setInstanceForTesting(mUnifiedConsentServiceBridgeMock);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Mockito.when(
                                    mUnifiedConsentServiceBridgeMock
                                            .isUrlKeyedAnonymizedDataCollectionEnabled(
                                                    ProfileManager.getLastUsedRegularProfile()))
                            .thenReturn(true);
                });
        ServiceLoaderUtil.setInstanceForTesting(
                GoogleActivityController.class, mGoogleActivityController);

        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);
        when(mRegionalCapabilities.isInEeaCountry()).thenReturn(false);

        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);

        // Disable the extension UI by default because most tests are for mobile Android where
        // extensions are not supported. This prevents having to maintain two sets of screenshots
        // for render tests. Individual tests may override it.
        ThreadUtils.runOnUiThreadBlocking(() -> mFakeExtensionUiBackendRule.setEnabled(false));

        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
    }

    /**
     * Test opening sync settings without sync consent when `mIsFromSigninScreen` is true doesn't
     * crash.
     *
     * <p>This is a regression test for crbug.com/362220452.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testOpenSyncSettingsIsFromSigninScreenIsTrueWithoutSyncConsent() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity(ManageSyncSettings.createArguments(true));
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING})
    public void testAccountSettingsView() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mFakeExtensionUiBackendRule.setEnabled(
                                ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS));

        // The types that should be default-enabled in transport mode depend on various flags.
        Set<String> expectedEnabledTypes =
                new HashSet<>(
                        Arrays.asList(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PAYMENTS_TOGGLE,
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE,
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_SETTINGS_TOGGLE,
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_BOOKMARKS_TOGGLE,
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_READING_LIST_TOGGLE,
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_ADDRESSES_TOGGLE));
        if (shouldShowExtensionsItem()) {
            expectedEnabledTypes.add(ManageSyncSettings.PREF_ACCOUNT_SECTION_EXTENSIONS_TOGGLE);
        }
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();

        Collection<ChromeSwitchPreference> dataTypes = getAccountDataTypes(fragment).values();
        for (ChromeSwitchPreference dataType : dataTypes) {
            Assert.assertEquals(
                    "Wrong checked state for toggle " + dataType.getKey(),
                    expectedEnabledTypes.contains(dataType.getKey()),
                    dataType.isChecked());
            Assert.assertTrue(dataType.isEnabled());
        }

        onView(withText(R.string.account_section_header)).check(matches(isDisplayed()));

        scrollToAndVerifyPresence(R.string.account_section_history_toggle);

        scrollToAndVerifyPresence(R.string.account_section_bookmarks_toggle);

        if (shouldShowExtensionsItem()) {
            scrollToAndVerifyPresence(R.string.account_section_extensions_toggle);
        } else {
            onView(withText(R.string.account_section_extensions_toggle)).check(doesNotExist());
        }

        scrollToAndVerifyPresence(R.string.account_section_reading_list_toggle);

        scrollToAndVerifyPresence(R.string.account_section_addresses_toggle);

        scrollToAndVerifyPresence(R.string.account_section_passwords_toggle);

        scrollToAndVerifyPresence(R.string.account_section_payments_and_info_toggle);

        scrollToAndVerifyPresence(R.string.account_section_settings_toggle);

        scrollToAndVerifyPresence(R.string.account_section_footer);

        scrollToAndVerifyPresence(R.string.sign_in_personalize_google_services_title);

        onView(withText(R.string.account_advanced_header)).check(matches(isDisplayed()));
        onView(withText(R.string.sign_in_personalize_google_services_summary))
                .check(matches(isDisplayed()));

        scrollToAndVerifyPresence(R.string.sync_encryption);

        scrollToAndVerifyPresence(R.string.account_data_dashboard_title);
        onView(withText(R.string.account_data_dashboard_subtitle)).check(matches(isDisplayed()));

        scrollToAndVerifyPresence(R.string.manage_your_google_account);

        scrollToAndVerifyPresence(R.string.account_android_device_accounts);
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    @Policies.Add({
        @Policies.Item(key = "SyncTypesListDisabled", string = "[\"bookmarks\", \"passwords\"]")
    })
    @DisableIf.Build(
            sdk_equals = 29,
            supported_abis_includes = "x86_64",
            message = "crbug.com/444011887")
    public void testSignInWithManagedDataTypes() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        Map<Integer, ChromeSwitchPreference> dataTypes = getAccountDataTypes(fragment);
        // When one or more sync types are managed, the respective preference should be disabled and
        // not checked, while all other preferences should be user selectable.
        for (ChromeSwitchPreference dataType : dataTypes.values()) {
            // Filter the history switch, since it's currently not enabled by default.
            if (dataType.getKey().equals(ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE)) {
                continue;
            }
            boolean shouldBeEnabled =
                    !dataType.getKey()
                                    .equals(
                                            ManageSyncSettings
                                                    .PREF_ACCOUNT_SECTION_BOOKMARKS_TOGGLE)
                            && !dataType.getKey()
                                    .equals(
                                            ManageSyncSettings
                                                    .PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE);
            Assert.assertEquals(dataType.isChecked(), shouldBeEnabled);
            Assert.assertEquals(dataType.isEnabled(), shouldBeEnabled);
        }

        // Check that the preference shows the managed text.
        onView(withText(R.string.account_section_bookmarks_toggle))
                .check(matches(hasSibling(withText(R.string.managed_by_your_organization))));
        onView(withText(R.string.account_section_passwords_toggle))
                .check(matches(hasSibling(withText(R.string.managed_by_your_organization))));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testPressingSignOut() {
        mSyncTestRule.setUpAccountAndSignInForTesting();

        Assert.assertNotNull(
                mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));

        startManageSyncPreferences();

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withId(R.id.sign_out_button)).perform(click());
        Assert.assertNull(mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testHistoryOptInDoNotCarryOverFromOneUserToAnother() {
        mSyncTestRule.getSigninTestRule().addAccountThenSignin(TestAccounts.ACCOUNT1);

        ManageSyncSettings fragment = startManageSyncPreferences();

        ChromeSwitchPreference history_and_tabs_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE);
        mSyncTestRule.togglePreference(history_and_tabs_toggle);
        Assert.assertTrue(history_and_tabs_toggle.isChecked());

        mSyncTestRule.signOut();

        // Add a different account, and open the sync settings to check that history opt-in did not
        // carry over from one user to another.
        mSyncTestRule.getSigninTestRule().addAccountThenSignin(TestAccounts.ACCOUNT2);

        fragment = startManageSyncPreferences();

        history_and_tabs_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE);
        Assert.assertFalse(history_and_tabs_toggle.isChecked());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testRemoveAccountFromDeviceShouldClearSyncPrefs() {
        SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        signinTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ManageSyncSettings fragment = startManageSyncPreferences();

        ChromeSwitchPreference passwords_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE);
        mSyncTestRule.togglePreference(passwords_toggle);
        Assert.assertFalse(passwords_toggle.isChecked());

        mSyncTestRule.signOut();
        signinTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // Add the same account again, and open the sync settings to check that prefs was cleared
        // upon the account removal.
        signinTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        fragment = startManageSyncPreferences();

        passwords_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE);
        Assert.assertTrue(passwords_toggle.isChecked());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testHistoryOptInCarriesOverThroughSignoutSignin() {
        mSyncTestRule.getSigninTestRule().addAccountThenSignin(TestAccounts.ACCOUNT1);

        ManageSyncSettings fragment = startManageSyncPreferences();

        ChromeSwitchPreference history_and_tabs_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE);
        mSyncTestRule.togglePreference(history_and_tabs_toggle);
        Assert.assertTrue(history_and_tabs_toggle.isChecked());

        mSyncTestRule.signOut();

        // Sign-in again with the same account, and open the sync settings to check that history
        // opt-in did carry over through sign-out & sign-in.
        SigninTestUtil.signin(TestAccounts.ACCOUNT1);

        fragment = startManageSyncPreferences();

        history_and_tabs_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE);
        Assert.assertTrue(history_and_tabs_toggle.isChecked());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSyncAddressesWithCustomPasspharaseShowsWarningDialog() {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();

        ChromeSwitchPreference addresses_toggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_ADDRESSES_TOGGLE);
        mSyncTestRule.togglePreference(addresses_toggle);
        onView(withText(R.string.sync_addresses_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSyncHistoryAndTabsToggle() {
        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference historyToggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE);

        SyncService syncService = mSyncTestRule.getSyncService();

        // Switching history sync on from settings clears history sync declined prefs.
        mSyncTestRule.togglePreference(historyToggle);
        verify(mHistorySyncHelperMock).clearHistorySyncDeclinedPrefs();

        Set<Integer> activeDataTypes =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return syncService.getActiveDataTypes();
                        });
        Assert.assertTrue(activeDataTypes.contains(DataType.HISTORY));
        Assert.assertTrue(activeDataTypes.contains(DataType.SESSIONS));

        // Switching history sync off from settings records history sync declined prefs.
        mSyncTestRule.togglePreference(historyToggle);
        verify(mHistorySyncHelperMock).recordHistorySyncDeclinedPrefs();

        activeDataTypes =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return syncService.getActiveDataTypes();
                        });
        Assert.assertFalse(activeDataTypes.contains(DataType.HISTORY));
        Assert.assertFalse(activeDataTypes.contains(DataType.SESSIONS));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/1188548")
    public void testPassphraseCreation() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(fragment::onChooseCustomPassphraseRequested);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        PassphraseCreationDialogFragment pcdf = getPassphraseCreationDialogFragment();
        AlertDialog dialog = (AlertDialog) pcdf.getDialog();
        Button okButton = dialog.getButton(Dialog.BUTTON_POSITIVE);
        EditText enterPassphrase = dialog.findViewById(R.id.passphrase);
        EditText confirmPassphrase = dialog.findViewById(R.id.confirm_passphrase);

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
        // mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        mSyncTestRule.getSigninTestRule().addChildTestAccountThenWaitForSignin();
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference paymentsIntegration =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PAYMENTS_TOGGLE);

        assertPaymentsIntegrationEnabled(false);
        Assert.assertFalse(paymentsIntegration.isChecked());
        Assert.assertFalse(paymentsIntegration.isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING})
    public void testPaymentSettingsStringUpdated() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference paymentsIntegration =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PAYMENTS_TOGGLE);
        Assert.assertEquals(
                paymentsIntegration.getTitle(),
                fragment.getActivity()
                        .getString(R.string.account_section_payments_and_info_toggle));
    }

    /**
     * Test the trusted vault key retrieval flow, which involves launching an intent and finally
     * calling TrustedVaultClient.notifyKeysChanged().
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "crbug.com/386744084")
    public void testTrustedVaultKeyRetrieval() {
        final byte[] trustedVaultKey = new byte[] {1, 2, 3, 4};

        mSyncTestRule.getFakeServerHelper().setTrustedVaultNigori(trustedVaultKey);

        // Keys won't be populated by FakeTrustedVaultClientBackend unless corresponding key
        // retrieval activity is about to be completed.
        SyncTestRule.FakeTrustedVaultClientBackend.get()
                .setKeys(Collections.singletonList(trustedVaultKey));

        mSyncTestRule.setUpAccountAndSignInForTesting();

        // Initially FakeTrustedVaultClientBackend doesn't provide any keys, so PSS should remain
        // in TrustedVaultKeyRequired state.
        SyncTestUtil.waitForTrustedVaultKeyRequired(true);

        final ManageSyncSettings fragment = startManageSyncPreferences();
        // Mimic the user tapping on Encryption. This should start FakeKeyRetrievalActivity and
        // notify native client that keys were changed. Right before FakeKeyRetrievalActivity
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
    @DisabledTest(message = "crbug.com/386744084")
    public void testTrustedVaultRecoverabilityFix() {
        final byte[] trustedVaultKey = new byte[] {1, 2, 3, 4};

        mSyncTestRule.getFakeServerHelper().setTrustedVaultNigori(trustedVaultKey);

        // Mimic retrieval having completed earlier.
        SyncTestRule.FakeTrustedVaultClientBackend.get()
                .setKeys(Collections.singletonList(trustedVaultKey));
        SyncTestRule.FakeTrustedVaultClientBackend.get().startPopulateKeys();

        SyncTestRule.FakeTrustedVaultClientBackend.get().setRecoverabilityDegraded(true);

        mSyncTestRule.setUpAccountAndSignInForTesting();

        // Initially recoverability should be reported as degraded.
        SyncTestUtil.waitForTrustedVaultRecoverabilityDegraded(true);

        // Mimic the user tapping on the error card's button. This should start
        // FakeRecoverabilityDegradedFixActivity and notify native client that recoverability has
        // changed. Right before FakeRecoverabilityDegradedFixActivity completion
        // FakeTrustedVaultClientBackend will exit the recoverability degraded state.
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fragment.onSyncErrorCardPrimaryButtonClicked();
                });

        // Native client should fetch the new recoverability state and get out of the
        // degraded-recoverability state.
        SyncTestUtil.waitForTrustedVaultRecoverabilityDegraded(false);
    }

    @Test
    @LargeTest
    public void testSigninSettingsBatchUploadCardVisibilityWhenSyncIsConfiguring()
            throws Exception {
        setupMockSyncService(BiometricStatus.ONLY_LSKF_AVAILABLE, TransportState.CONFIGURING);
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInWithoutWaitingForTesting();

        final ManageSyncSettings fragment = startManageSyncPreferences();
        Assert.assertFalse(
                fragment.findPreference(ManageSyncSettings.PREF_BATCH_UPLOAD_CARD_PREFERENCE)
                        .isVisible());
        Assert.assertNull(fragment.getView().findViewById(R.id.signin_settings_card));
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsTopAvatar() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.central_account_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.central_account_card);
                        });
        mRenderTestRule.render(view, "sign_in_settings_top_avatar");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsTopAvatarWithNoName() throws Exception {
        mSyncTestRule.getSigninTestRule().addAccountThenSignin(TestAccounts.TEST_ACCOUNT_NO_NAME);
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.central_account_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.central_account_card);
                        });
        mRenderTestRule.render(view, "sign_in_settings_top_avatar_with_no_name");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsTopAvatarWithNonDisplayableEmail() throws Exception {
        SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        var childAccount = TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL;
        signinTestRule.addAccount(childAccount);
        // Child accounts are automatically signed-in in the background.
        signinTestRule.waitForSignin(childAccount);
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.central_account_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.central_account_card);
                        });
        mRenderTestRule.render(view, "sign_in_settings_top_avatar_with_non_displayable_email");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsTopAvatarWithNonDisplayableEmailAndNoName() throws Exception {
        SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        var childAccount = TestAccounts.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME;
        signinTestRule.addAccount(childAccount);
        // Child accounts are automatically signed-in in the background.
        signinTestRule.waitForSignin(childAccount);
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.central_account_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.central_account_card);
                        });
        mRenderTestRule.render(
                view, "sign_in_settings_top_avatar_with_non_displayable_email_and_no_name");
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testTitleOfAccountSyncSettingsPage() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        Assert.assertEquals(
                fragment.getActivity().getTitle(),
                fragment.getActivity().getString(R.string.account_settings_title));
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testBottomOfAccountSyncSettingsPage() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        render(fragment, "bottom_of_account_sync_settings_page");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSignoutButton() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        ViewUtils.waitForVisibleView(withId(R.id.sign_out_button));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.sign_out_button);
                        });
        mRenderTestRule.render(view, "sign_out_button");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsBatchUploadEntryDescriptionPassword() throws Exception {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_passwords");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsBatchUploadEntryDescriptionOther() throws Exception {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_other");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsBatchUploadEntryDescriptionPasswordAndOther() throws Exception {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return fragment.getActivity().findViewById(R.id.signin_settings_card);
                        });
        mRenderTestRule.render(view, "batch_upload_entry_description_password_and_other");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsBatchUploadDialogShouldShowPasswordsToggle() throws Exception {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        onView(withText(R.string.batch_upload_card_save_button)).perform(click());
        ViewUtils.waitForVisibleView(withId(R.id.batch_upload_dialog));

        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AppModalPresenter presenter =
                                    (AppModalPresenter)
                                            ((ModalDialogManagerHolder) fragment.getActivity())
                                                    .getModalDialogManager()
                                                    .getCurrentPresenterForTest();
                            return presenter.getDialogViewForTesting();
                        });
        mRenderTestRule.render(view, "batch_upload_passwords_dialog");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsBatchUploadDialogShouldShowBookmarksAndReadingListToggles()
            throws Exception {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        onView(withText(R.string.batch_upload_card_save_button)).perform(click());
        ViewUtils.waitForVisibleView(withId(R.id.batch_upload_dialog));

        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AppModalPresenter presenter =
                                    (AppModalPresenter)
                                            ((ModalDialogManagerHolder) fragment.getActivity())
                                                    .getModalDialogManager()
                                                    .getCurrentPresenterForTest();
                            return presenter.getDialogViewForTesting();
                        });
        mRenderTestRule.render(view, "batch_upload_bookmarks_and_reading_list_dialog");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    public void testSigninSettingsBatchUploadDialogShouldShowAllToggles() throws Exception {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();

        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        onView(withText(R.string.batch_upload_card_save_button)).perform(click());
        ViewUtils.waitForVisibleView(withId(R.id.batch_upload_dialog));

        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            AppModalPresenter presenter =
                                    (AppModalPresenter)
                                            ((ModalDialogManagerHolder) fragment.getActivity())
                                                    .getModalDialogManager()
                                                    .getCurrentPresenterForTest();
                            return presenter.getDialogViewForTesting();
                        });
        mRenderTestRule.render(view, "batch_upload_all_toggles_dialog");
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void
            testSigninSettingsBatchUploadEntryDescriptionForPasswordsNotRequestedWhenAuthUnavailable()
                    throws Exception {
        setupMockSyncService(BiometricStatus.UNAVAILABLE, TransportState.ACTIVE);

        mSyncTestRule.setUpAccountAndSignInForTesting();
        startManageSyncPreferences();
        verify(mSyncService, atLeast(1))
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.READING_LIST)), any(Callback.class));
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices", "RenderTest"})
    public void testGoogleActivityControls() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        render(fragment, "sync_settings_google_activity_controls");
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices", "RenderTest"})
    public void testLinkedServicesSetting() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        render(fragment, "sync_settings_linked_services_setting");
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices", "RenderTest"})
    public void testLinkedServicesSettingEea() throws Exception {
        when(mRegionalCapabilities.isInEeaCountry()).thenReturn(true);
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        render(fragment, "sync_settings_linked_services_setting_eea");
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices"})
    public void testClickPersonalizeGoogleServicesNonEEA() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        // Click the Google ActivityControls pref
        onView(withText(R.string.sign_in_personalize_google_services_title)).perform(click());
        verify(mGoogleActivityController).openWebAndAppActivitySettings(any(), any());
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices"})
    public void testClickPersonalizeGoogleServicesEEA() {
        when(mRegionalCapabilities.isInEeaCountry()).thenReturn(true);
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        // Click the Personalize Google services
        onView(withText(R.string.sign_in_personalize_google_services_title_eea)).perform(click());
        onView(withText(R.string.personalized_google_services_summary))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/450272307")
    public void testKeyboardNavigationToSignOutButton() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
        // There are 4 non-selectable preferences in the preference screen: central_account_card,
        // account_section_header, account_section_footer, and account_advanced_header.
        for (int i = 0; i < recyclerView.getAdapter().getItemCount() - 4; ++i) {
            onView(withId(R.id.recycler_view)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
        }
        onView(withId(R.id.sign_out_button)).check(matches(hasFocus()));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/450272307")
    public void testCentralAccountCardNotReceivingFocus() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        startManageSyncPreferences();
        // Focus on the first element that can receive focus in the settings page.
        onView(withId(R.id.recycler_view)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
        onView(withId(R.id.history_and_tabs_toggle)).check(matches(hasFocus()));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/450272307")
    public void testBatchUploadCardNotReceivingFocus() {
        setupMockSyncService();
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        any(Callback.class));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        startManageSyncPreferences();
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));

        // Focus on the first element that can receive focus in the settings page.
        onView(withId(R.id.recycler_view)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
        onView(withId(R.id.signin_settings_card_button)).check(matches(hasFocus()));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/450272307")
    public void testIdentityErrorCardNotReceivingFocus() {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();

        CriteriaHelper.pollUiThread(
                () -> mSyncTestRule.getSyncService().isPassphraseRequiredForPreferredDataTypes());

        startManageSyncPreferences();
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));

        // Focus on the first element that can receive focus in the settings page.
        onView(withId(R.id.recycler_view)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
        onView(withId(R.id.signin_settings_card_button)).check(matches(hasFocus()));
    }

    @Test
    @SmallTest
    public void testFirstTextViewInPassphraseDialogNotReceivingFocus() {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();

        CriteriaHelper.pollUiThread(
                () -> mSyncTestRule.getSyncService().isPassphraseRequiredForPreferredDataTypes());

        startManageSyncPreferences();
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        // Passphrase dialog should open.
        final PassphraseDialogFragment passphraseFragment =
                ActivityTestUtils.waitForFragment(
                        mSettingsActivity, ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Focus on the first element that can receive focus in the passphrase dialog.
        onView(withText(R.string.sync_enter_passphrase_title))
                .perform(pressKey(KeyEvent.KEYCODE_DPAD_LEFT));
        onView(withId(R.id.passphrase)).check(matches(hasFocus()));
    }

    @Test
    @LargeTest
    public void testWrongPassphraseShowsIncorrectPassphraseError() throws Exception {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();

        CriteriaHelper.pollUiThread(
                () -> mSyncTestRule.getSyncService().isPassphraseRequiredForPreferredDataTypes());

        startManageSyncPreferences();
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        // Passphrase dialog should open.
        final PassphraseDialogFragment passphraseFragment =
                ActivityTestUtils.waitForFragment(
                        mSettingsActivity, ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Mimic the user tapping on the positive(submit) button with an empty(wrong) passphrase.
        onView(withText(R.string.submit)).perform(click());
        onView(withId(R.id.verifying)).check(matches(withText(R.string.sync_passphrase_incorrect)));
    }

    // TODO(crbug.com/330438265): Extend this test for the identity error card.
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "crbug.com/386744084")
    public void testSyncErrorCardForUpmBackendOutdatedUpdatedDynamically() {
        setupMockSyncService();
        when(mSyncService.getUserActionableError())
                .thenReturn(UserActionableError.NEEDS_UPM_BACKEND_UPGRADE);

        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));
        SyncErrorCardPreference preference =
                (SyncErrorCardPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_ERROR_CARD_PREFERENCE);

        // The error card exists.
        Assert.assertTrue(preference.isShown());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mSyncService.getUserActionableError())
                            .thenReturn(UserActionableError.NONE);
                    // TODO(crbug.com/327623232): Observe such changes instead.
                    preference.syncStateChanged();
                });
        // The error card is now hidden.
        Assert.assertFalse(preference.isShown());
    }

    @Test
    @LargeTest
    public void testIdentityErrorCardActionForPassphraseRequired() throws Exception {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();

        SyncService syncService = mSyncTestRule.getSyncService();
        CriteriaHelper.pollUiThread(() -> syncService.isPassphraseRequiredForPreferredDataTypes());

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));
        IdentityErrorCardPreference preference =
                (IdentityErrorCardPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_IDENTITY_ERROR_CARD_PREFERENCE);

        // The error card exists.
        Assert.assertTrue(preference.isShown());

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        // Passphrase dialog should open.
        final PassphraseDialogFragment passphraseFragment =
                ActivityTestUtils.waitForFragment(
                        mSettingsActivity, ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting the passphrase
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        ThreadUtils.runOnUiThreadBlocking(() -> syncService.setDecryptionPassphrase("passphrase"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fragment.getFragmentManager().executePendingTransactions();
                    Assert.assertNull(
                            "PassphraseDialogFragment should be dismissed.",
                            mSettingsActivity
                                    .getFragmentManager()
                                    .findFragmentByTag(
                                            ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE));
                });

        // The error card is now hidden.
        Assert.assertFalse(preference.isShown());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncDisabledByPolicy() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncService syncService = mSyncTestRule.getSyncService();
        // Mark sync disabled by policy.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setBoolean(SyncPrefNames.SYNC_MANAGED, true);
                    Assert.assertTrue(syncService.isSyncDisabledByEnterprisePolicy());
                });

        ManageSyncSettings fragment = startManageSyncPreferences();

        onView(withText(R.string.settings_sync_disabled_by_administrator))
                .check(matches(isDisplayed()));

        // All datatype toggles should be unchecked and disabled.
        Map<Integer, ChromeSwitchPreference> dataTypes = getAccountDataTypes(fragment);
        for (ChromeSwitchPreference dataType : dataTypes.values()) {
            Assert.assertFalse(dataType.isChecked());
            Assert.assertFalse(dataType.isEnabled());
        }
    }

    private void setupMockSyncService(
            @BiometricStatus int biometricAvailabilityStatus, @TransportState int transportState) {
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(biometricAvailabilityStatus);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.getTransportState()).thenReturn(transportState);
        when(mSyncService.getAuthError())
                .thenReturn(new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
    }

    private void setupMockSyncService() {
        setupMockSyncService(BiometricStatus.BIOMETRICS_AVAILABLE, TransportState.ACTIVE);
    }

    private ManageSyncSettings startManageSyncPreferences() {
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }

    private Map<Integer, ChromeSwitchPreference> getAccountDataTypes(ManageSyncSettings fragment) {
        Map<Integer, ChromeSwitchPreference> dataTypes = new HashMap<>();
        for (Map.Entry<Integer, String> accountUiDataType : ACCOUNT_UI_DATATYPES.entrySet()) {
            if (accountUiDataType.getKey() == UserSelectableType.TABS) {
                continue;
            }
            // EXTENSIONS are only available in the desktop Android build.
            if (accountUiDataType.getKey() == UserSelectableType.EXTENSIONS
                    && !shouldShowExtensionsItem()) {
                continue;
            }
            Integer selectedType = accountUiDataType.getKey();
            String prefId = accountUiDataType.getValue();
            dataTypes.put(selectedType, (ChromeSwitchPreference) fragment.findPreference(prefId));
        }
        return dataTypes;
    }

    private PassphraseCreationDialogFragment getPassphraseCreationDialogFragment() {
        return ActivityTestUtils.waitForFragment(
                mSettingsActivity, ManageSyncSettings.FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void assertPaymentsIntegrationEnabled(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Set<Integer> actualDataTypes =
                            mSyncTestRule.getSyncService().getSelectedTypes();
                    if (enabled) {
                        Assert.assertTrue(actualDataTypes.contains(UserSelectableType.PAYMENTS));
                    } else {
                        Assert.assertFalse(actualDataTypes.contains(UserSelectableType.PAYMENTS));
                    }
                });
    }

    private void clickPreference(final Preference pref) {
        ThreadUtils.runOnUiThreadBlocking(
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

    private void render(ManageSyncSettings fragment, String skiaGoldId) throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        // Sanitize the view, in particular to ensure the presence of scroll bars do not cause
        // image diffs.
        ChromeRenderTestRule.sanitize(fragment.getView());
        mRenderTestRule.render(fragment.getView(), skiaGoldId);
    }

    private void scrollToAndVerifyPresence(@StringRes int textId) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(withText(textId))));
        onView(withText(textId)).check(matches(isDisplayed()));
    }

    /** Returns whether the extensions sync item should be shown. */
    private boolean shouldShowExtensionsItem() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ExtensionUi.isEnabled(ProfileManager.getLastUsedRegularProfile()));
    }
}
