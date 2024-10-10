// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import static java.util.Map.entry;

import android.app.Activity;
import android.app.Dialog;
import android.app.Instrumentation.ActivityResult;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.PopupMenu;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.account_storage_toggle.AccountStorageToggleFragmentArgs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.chrome.browser.sync.settings.IdentityErrorCardPreference;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncErrorCardPreference;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.TransportState;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync.internal.SyncPrefNames;
import org.chromium.components.user_prefs.UserPrefs;
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
public class ManageSyncSettingsTest {
    private static final int RENDER_TEST_REVISION = 6;

    /** Maps selected types to their UI element IDs. */
    private Map<Integer, String> mUiDataTypes;

    /** Maps selected types to their Account UI element IDs. */
    private static final Map<Integer, String> ACCOUNT_UI_DATATYPES =
            Map.ofEntries(
                    entry(
                            UserSelectableType.AUTOFILL,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_ADDRESSES_TOGGLE),
                    entry(
                            UserSelectableType.BOOKMARKS,
                            ManageSyncSettings.PREF_ACCOUNT_SECTION_BOOKMARKS_TOGGLE),
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
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private UnifiedConsentServiceBridge.Natives mUnifiedConsentServiceBridgeMock;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private GoogleActivityController mGoogleActivityController;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private HistorySyncHelper mHistorySyncHelperMock;
    @Mock private SyncService mSyncService;
    @Mock private ReauthenticatorBridge mReauthenticatorMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                UnifiedConsentServiceBridgeJni.TEST_HOOKS, mUnifiedConsentServiceBridgeMock);
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

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isEeaChoiceCountry()).thenReturn(false);

        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);

        mUiDataTypes = new HashMap<>();
        mUiDataTypes.put(UserSelectableType.AUTOFILL, ManageSyncSettings.PREF_SYNC_AUTOFILL);
        mUiDataTypes.put(UserSelectableType.BOOKMARKS, ManageSyncSettings.PREF_SYNC_BOOKMARKS);
        mUiDataTypes.put(
                UserSelectableType.PAYMENTS, ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
        mUiDataTypes.put(UserSelectableType.HISTORY, ManageSyncSettings.PREF_SYNC_HISTORY);
        mUiDataTypes.put(UserSelectableType.PASSWORDS, ManageSyncSettings.PREF_SYNC_PASSWORDS);
        mUiDataTypes.put(
                UserSelectableType.READING_LIST, ManageSyncSettings.PREF_SYNC_READING_LIST);
        mUiDataTypes.put(UserSelectableType.TABS, ManageSyncSettings.PREF_SYNC_RECENT_TABS);
        mUiDataTypes.put(UserSelectableType.PREFERENCES, ManageSyncSettings.PREF_SYNC_SETTINGS);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_APK_BACKUP_AND_RESTORE_BACKEND)) {
            mUiDataTypes.put(UserSelectableType.APPS, ManageSyncSettings.PREF_SYNC_APPS);
        }

        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testOpenSyncSettingsIsFromSigninScreenIsTrueWithoutSyncConsent() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity(ManageSyncSettings.createArguments(true));
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
    @SmallTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSyncAccountDataTypes() {
        // The types that should be default-enabled in transport mode depend on various flags.
        Set<String> expectedEnabledTypes =
                new HashSet<>(
                        Arrays.asList(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PAYMENTS_TOGGLE,
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_SETTINGS_TOGGLE));
        if (SyncFeatureMap.isEnabled(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)) {
            expectedEnabledTypes.add(ManageSyncSettings.PREF_ACCOUNT_SECTION_BOOKMARKS_TOGGLE);
        }
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.READING_LIST_ENABLE_SYNC_TRANSPORT_MODE_UPON_SIGNIN)) {
            expectedEnabledTypes.add(ManageSyncSettings.PREF_ACCOUNT_SECTION_READING_LIST_TOGGLE);
        }
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)) {
            expectedEnabledTypes.add(ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE);
        }
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.SYNC_ENABLE_CONTACT_INFO_DATA_TYPE_IN_TRANSPORT_MODE)) {
            expectedEnabledTypes.add(ManageSyncSettings.PREF_ACCOUNT_SECTION_ADDRESSES_TOGGLE);
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
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    @Policies.Add({
        @Policies.Item(key = "SyncTypesListDisabled", string = "[\"passwords\", \"autofill\"]")
    })
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
                    || dataType.getKey()
                            .equals(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION)) {
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
                    || dataType.getKey()
                            .equals(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION)) {
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testPressingSignOut() {
        mSyncTestRule.setUpAccountAndSignInForTesting();

        Assert.assertNotNull(
                mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));

        startManageSyncPreferences();

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(R.string.sign_out)).perform(click());
        Assert.assertNull(mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSyncAddressesWithCustomPasspharaseShowsWarningDialog() {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();
        mSyncTestRule.getSyncService();

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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSyncHistoryAndTabsToggle() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();

        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference historyToggle =
                (ChromeSwitchPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_HISTORY_TOGGLE);

        // Switching history sync on from settings clears history sync declined prefs.
        mSyncTestRule.togglePreference(historyToggle);
        verify(mHistorySyncHelperMock).clearHistorySyncDeclinedPrefs();
        // Switching history sync off from settings records history sync declined prefs.
        mSyncTestRule.togglePreference(historyToggle);
        verify(mHistorySyncHelperMock).recordHistorySyncDeclinedPrefs();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testPressingSignOutAndTurnOffSyncShowsSignOutDialog() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();

        Preference turnOffSyncPreference =
                fragment.findPreference(ManageSyncSettings.PREF_TURN_OFF_SYNC);
        Assert.assertTrue(
                "Sign out and turn off sync button should be shown",
                turnOffSyncPreference.isVisible());
        ThreadUtils.runOnUiThreadBlocking(
                fragment.findPreference(ManageSyncSettings.PREF_TURN_OFF_SYNC)::performClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        onView(withText(R.string.turn_off_sync_and_signout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        ThreadUtils.runOnUiThreadBlocking(
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

        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationUnchecked() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        Set<Integer> allDataTypesExceptPayments = new HashSet<>(mUiDataTypes.keySet());
        allDataTypesExceptPayments.remove(UserSelectableType.PAYMENTS);

        mSyncTestRule.setSelectedTypes(false, allDataTypesExceptPayments);
        ManageSyncSettings fragment = startManageSyncPreferences();

        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

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

        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
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

        mSyncTestRule.setSelectedTypes(false, mUiDataTypes.keySet());
        ManageSyncSettings fragment = startManageSyncPreferences();

        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
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

        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncTestRule.togglePreference(paymentsIntegration);

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(false);

        Assert.assertFalse(
                "There should be no server cards remaining",
                mSyncTestRule.hasServerAutofillCreditCards());
    }

    // Before crbug.com/40265120, the autofill and payments toggles used to be coupled. This test
    // verifies they no longer are.
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testPaymentsIntegrationNotDisabledByAutofillSyncCheckbox() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Get the UI elements.
        ManageSyncSettings fragment = startManageSyncPreferences();
        ChromeSwitchPreference syncEverything = getSyncEverything(fragment);
        CheckBoxPreference syncAutofill =
                (CheckBoxPreference) fragment.findPreference(ManageSyncSettings.PREF_SYNC_AUTOFILL);
        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

        assertSyncOnState(fragment);
        Assert.assertFalse(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());

        mSyncTestRule.togglePreference(syncEverything);

        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());

        mSyncTestRule.togglePreference(syncAutofill);

        Assert.assertTrue(paymentsIntegration.isEnabled());
        Assert.assertTrue(paymentsIntegration.isChecked());

        closeFragment(fragment);
        assertPaymentsIntegrationEnabled(true);
    }

    /**
     * Test that choosing a passphrase type while sync is off doesn't crash.
     *
     * <p>This is a regression test for http://crbug.com/507557.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testChoosePassphraseTypeWhenSyncIsOff() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        Preference encryption = getEncryption(fragment);
        clickPreference(encryption);

        getPassphraseTypeDialogFragment();
        mSyncTestRule.signOut();

        // Mimic the user clicking on the explicit passphrase checkbox immediately after signing
        // out.
        ThreadUtils.runOnUiThreadBlocking(fragment::onChooseCustomPassphraseRequested);

        // No crash means we passed.
    }

    /** Test that entering a passphrase while sync is off doesn't crash. */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testEnterPassphraseWhenSyncIsOff() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        mSyncTestRule.signOut();
        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onPassphraseEntered("passphrase"));
        // No crash means we passed.
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/1188548")
    public void testPassphraseCreation() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
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
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        CheckBoxPreference paymentsIntegration =
                (CheckBoxPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_PAYMENTS_INTEGRATION);

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
        SyncTestRule.FakeTrustedVaultClientBackend.get()
                .setKeys(Collections.singletonList(trustedVaultKey));

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

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
    public void testTrustedVaultRecoverabilityFix() {
        final byte[] trustedVaultKey = new byte[] {1, 2, 3, 4};

        mSyncTestRule.getFakeServerHelper().setTrustedVaultNigori(trustedVaultKey);

        // Mimic retrieval having completed earlier.
        SyncTestRule.FakeTrustedVaultClientBackend.get()
                .setKeys(Collections.singletonList(trustedVaultKey));
        SyncTestRule.FakeTrustedVaultClientBackend.get().startPopulateKeys();

        SyncTestRule.FakeTrustedVaultClientBackend.get().setRecoverabilityDegraded(true);

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

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
    @EnableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.ENABLE_BATCH_UPLOAD_FROM_SETTINGS
    })
    public void testSigninSettingsBatchUploadCardVisibilityWhenSyncIsConfiguring()
            throws Exception {
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.ONLY_LSKF_AVAILABLE);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.getTransportState()).thenReturn(TransportState.CONFIGURING);
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
        Assert.assertFalse(
                fragment.findPreference(ManageSyncSettings.PREF_BATCH_UPLOAD_CARD_PREFERENCE)
                        .isVisible());
        Assert.assertNull(fragment.getView().findViewById(R.id.signin_settings_card));
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSigninSettingsTopAvatarWithNoName() throws Exception {
        mSyncTestRule
                .getSigninTestRule()
                .addAccountThenSignin(AccountManagerTestRule.TEST_ACCOUNT_NO_NAME);
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSigninSettingsTopAvatarWithNonDisplayableEmail() throws Exception {
        mSyncTestRule
                .getSigninTestRule()
                .addAccountThenSignin(AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL);
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSigninSettingsTopAvatarWithNonDisplayableEmailAndNoName() throws Exception {
        mSyncTestRule
                .getSigninTestRule()
                .addAccountThenSignin(
                        AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME);
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
    @EnableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.ENABLE_BATCH_UPLOAD_FROM_SETTINGS
    })
    public void testSigninSettingsBatchUploadEntryDescriptionPassword() throws Exception {
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
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
    @EnableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.ENABLE_BATCH_UPLOAD_FROM_SETTINGS
    })
    public void testSigninSettingsBatchUploadEntryDescriptionOther() throws Exception {
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
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
    @EnableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.ENABLE_BATCH_UPLOAD_FROM_SETTINGS
    })
    public void testSigninSettingsBatchUploadEntryDescriptionPasswordAndOther() throws Exception {
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
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
    @Feature({"Sync"})
    @EnableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        ChromeFeatureList.ENABLE_BATCH_UPLOAD_FROM_SETTINGS
    })
    public void
            testSigninSettingsBatchUploadEntryDescriptionForPasswordsNotRequestedWhenAuthUnavailable()
                    throws Exception {
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        mSyncTestRule.setUpAccountAndSignInForTesting();
        startManageSyncPreferences();
        verify(mSyncService, atLeast(1))
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.READING_LIST)), any(Callback.class));
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    // Sometimes the rendered image may not contain the scrollbar and cause
                    // flakiness.
                    // Hide the scrollbar altogether to reduce flakiness.
                    recyclerView.setVerticalScrollBarEnabled(false);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        render(fragment, "advanced_sync_flow_bottom_view");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    @EnableFeatures({ChromeFeatureList.WEB_APK_BACKUP_AND_RESTORE_BACKEND})
    public void testAdvancedSyncFlowBottomViewWithWebApks() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    // Sometimes the rendered image may not contain the scrollbar and cause
                    // flakiness.
                    // Hide the scrollbar altogether to reduce flakiness.
                    recyclerView.setVerticalScrollBarEnabled(false);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        render(fragment, "advanced_sync_flow_bottom_view_with_webapks");
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    // Sometimes the rendered image may not contain the scrollbar and cause
                    // flakiness.
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    // Sometimes the rendered image may not contain the scrollbar and cause
                    // flakiness.
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    // Sometimes the rendered image may not contain the scrollbar and cause
                    // flakiness.
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
        @Policies.Item(
                key = "SyncTypesListDisabled",
                string =
                        "[\"bookmarks\", \"readingList\", \"preferences\", \"passwords\","
                                + " \"autofill\", \"typedUrls\", \"tabs\"]")
    })
    public void testSyncSettingsTopViewWithSyncTypesManagedByPolicy() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        render(fragment, "sync_settings_top_view_with_sync_types_disabled_by_policy");
    }

    @Test
    @LargeTest
    @Feature({"Sync", "RenderTest"})
    @Policies.Add({
        @Policies.Item(
                key = "SyncTypesListDisabled",
                string =
                        "[\"bookmarks\", \"readingList\", \"preferences\", \"passwords\","
                                + " \"autofill\", \"typedUrls\", \"tabs\"]")
    })
    @DisableIf.Build(
            message = "Flaky on emulators. See crbug.com/332882352.",
            supported_abis_includes = "x86")
    public void testSyncSettingsBottomViewWithSyncTypesManagedByPolicy() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        render(fragment, "sync_settings_bottom_view_with_sync_types_disabled_by_policy");
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices", "RenderTest"})
    public void testGoogleActivityControls() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
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
    @EnableFeatures({ChromeFeatureList.LINKED_SERVICES_SETTING})
    public void testLinkedServicesSetting() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
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
    @EnableFeatures({ChromeFeatureList.LINKED_SERVICES_SETTING})
    public void testLinkedServicesSettingEea() throws Exception {
        when(mTemplateUrlService.isEeaChoiceCountry()).thenReturn(true);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
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
    @DisableFeatures({ChromeFeatureList.LINKED_SERVICES_SETTING})
    public void testClickGoogleActivityControls() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        // Click the Google ActivityControls pref
        onView(withText(R.string.sign_in_google_activity_controls_title)).perform(click());
        verify(mGoogleActivityController).openWebAndAppActivitySettings(any(), any());
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @DisableFeatures({ChromeFeatureList.LINKED_SERVICES_SETTING})
    public void testClickGoogleActivityControlsWhenSyncPromosShouldBeReplacedWithSigninPromos() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        final ManageSyncSettings fragment = startManageSyncPreferences();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        // Click the Google ActivityControls pref
        onView(withText(R.string.sign_in_google_activity_controls_title)).perform(click());
        verify(mGoogleActivityController).openWebAndAppActivitySettings(any(), any());
    }

    @Test
    @LargeTest
    @Feature({"PersonalizedGoogleServices"})
    @EnableFeatures({ChromeFeatureList.LINKED_SERVICES_SETTING})
    public void testClickPersonalizeGoogleServicesNonEEA() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
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
    @EnableFeatures({ChromeFeatureList.LINKED_SERVICES_SETTING})
    public void testClickPersonalizeGoogleServicesEEA() {
        when(mTemplateUrlService.isEeaChoiceCountry()).thenReturn(true);
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Mockito.when(
                                    mUnifiedConsentServiceBridgeMock
                                            .isUrlKeyedAnonymizedDataCollectionManaged(
                                                    ProfileManager.getLastUsedRegularProfile()))
                            .thenReturn(true);
                    Mockito.when(
                                    mUnifiedConsentServiceBridgeMock
                                            .isUrlKeyedAnonymizedDataCollectionEnabled(
                                                    ProfileManager.getLastUsedRegularProfile()))
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Mockito.when(
                                    mUnifiedConsentServiceBridgeMock
                                            .isUrlKeyedAnonymizedDataCollectionManaged(
                                                    ProfileManager.getLastUsedRegularProfile()))
                            .thenReturn(true);
                    Mockito.when(
                                    mUnifiedConsentServiceBridgeMock
                                            .isUrlKeyedAnonymizedDataCollectionEnabled(
                                                    ProfileManager.getLastUsedRegularProfile()))
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
    public void testAdvancedSyncFlowFromSyncConsentBackToHomeDoesNotEnableUKM() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        final ManageSyncSettings fragment = startManageSyncPreferencesFromSyncConsentFlow();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PopupMenu p = new PopupMenu(mSettingsActivity, null);
                    Menu menu = p.getMenu();
                    MenuItem menuItem = menu.add(0, android.R.id.home, 0, "");
                    fragment.onOptionsItemSelected(menuItem);
                });

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

    // TODO(crbug.com/330438265): Extend this test for the identity error card.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSyncErrorCardActionForUpmBackendOutdatedError() {
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        // The error card exists.
        onView(withId(R.id.signin_promo_view_wrapper)).check(matches(isDisplayed()));

        Intents.init();
        // Stub all external intents.
        intending(not(IntentMatchers.isInternal()))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        onView(withId(R.id.sync_promo_signin_button)).perform(click());
        // Expect intent to open the play store.
        // TODO(crbug.com/327623232): Have this as a constant in PasswordManagerHelper.
        intended(IntentMatchers.hasPackage("com.android.vending"));

        Intents.release();
    }

    // TODO(crbug.com/330438265): Extend this test for the identity error card.
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncErrorCardForUpmBackendOutdatedUpdatedDynamically() {
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));
        SyncErrorCardPreference preference =
                (SyncErrorCardPreference)
                        fragment.findPreference(ManageSyncSettings.PREF_SYNC_ERROR_CARD_PREFERENCE);

        // The error card exists.
        Assert.assertTrue(preference.isShown());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                            .thenReturn(false);
                    // TODO(crbug.com/327623232): Observe such changes instead.
                    preference.syncStateChanged();
                });
        // The error card is now hidden.
        Assert.assertFalse(preference.isShown());
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardActionForPassphraseRequired() throws Exception {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();

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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testPasswordsToggleWithHighlighting() {
        mSyncTestRule.setUpAccountAndSignInForTesting();

        Bundle args = new Bundle();
        args.putBoolean(AccountStorageToggleFragmentArgs.HIGHLIGHT, true);
        mSettingsActivityTestRule.startSettingsActivity(args);

        ChromeSwitchPreference toggle =
                (ChromeSwitchPreference)
                        mSettingsActivityTestRule
                                .getFragment()
                                .findPreference(
                                        ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE);
        @Nullable Integer backgroundColor = toggle.getBackgroundColor();
        Assert.assertNotNull(backgroundColor);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testPasswordsToggleWithoutHighlighting() {
        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings settings = startManageSyncPreferences();

        ChromeSwitchPreference toggle =
                (ChromeSwitchPreference)
                        settings.findPreference(
                                ManageSyncSettings.PREF_ACCOUNT_SECTION_PASSWORDS_TOGGLE);
        Assert.assertNull(toggle.getBackgroundColor());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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

    private ManageSyncSettings startManageSyncPreferences() {
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }

    private ManageSyncSettings startManageSyncPreferencesFromSyncConsentFlow() {
        mSettingsActivity =
                mSettingsActivityTestRule.startSettingsActivity(
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
        return (ChromeSwitchPreference)
                fragment.findPreference(ManageSyncSettings.PREF_SYNC_EVERYTHING);
    }

    private ChromeSwitchPreference getUrlKeyedAnonymizedData(ManageSyncSettings fragment) {
        return (ChromeSwitchPreference)
                fragment.findPreference(ManageSyncSettings.PREF_URL_KEYED_ANONYMIZED_DATA);
    }

    private Map<Integer, CheckBoxPreference> getDataTypes(ManageSyncSettings fragment) {
        Map<Integer, CheckBoxPreference> dataTypes = new HashMap<>();
        for (Map.Entry<Integer, String> uiDataType : mUiDataTypes.entrySet()) {
            Integer selectedType = uiDataType.getKey();
            String prefId = uiDataType.getValue();
            dataTypes.put(selectedType, (CheckBoxPreference) fragment.findPreference(prefId));
        }
        return dataTypes;
    }

    private Map<Integer, ChromeSwitchPreference> getAccountDataTypes(ManageSyncSettings fragment) {
        Map<Integer, ChromeSwitchPreference> dataTypes = new HashMap<>();
        for (Map.Entry<Integer, String> accountUiDataType : ACCOUNT_UI_DATATYPES.entrySet()) {
            if (accountUiDataType.getKey() == UserSelectableType.TABS) {
                continue;
            }
            Integer selectedType = accountUiDataType.getKey();
            String prefId = accountUiDataType.getValue();
            dataTypes.put(selectedType, (ChromeSwitchPreference) fragment.findPreference(prefId));
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
        Assert.assertTrue(
                "The google activity controls button should always be enabled.",
                getGoogleActivityControls(fragment).isEnabled());
        Assert.assertTrue(
                "The encryption button should always be enabled.",
                getEncryption(fragment).isEnabled());
        Assert.assertTrue(
                "The review your synced data button should be always enabled.",
                getReviewData(fragment).isEnabled());
    }

    private void assertSelectedTypesAre(final Set<Integer> enabledDataTypes) {
        final Set<Integer> disabledDataTypes = new HashSet<>(mUiDataTypes.keySet());
        disabledDataTypes.removeAll(enabledDataTypes);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Set<Integer> actualDataTypes =
                            mSyncTestRule.getSyncService().getSelectedTypes();
                    Assert.assertTrue(actualDataTypes.containsAll(enabledDataTypes));
                    Assert.assertTrue(Collections.disjoint(disabledDataTypes, actualDataTypes));
                });
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

    private void verifyUrlKeyedAnonymizedDataCollectionSet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    verify(mUnifiedConsentServiceBridgeMock, Mockito.atLeastOnce())
                            .setUrlKeyedAnonymizedDataCollectionEnabled(profile, true);
                });
    }

    private void verifyUrlKeyedAnonymizedDataCollectionNotSet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    verify(mUnifiedConsentServiceBridgeMock, Mockito.never())
                            .setUrlKeyedAnonymizedDataCollectionEnabled(profile, true);
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
}
