// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.core.StringStartsWith.startsWith;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.app.PendingIntent;

import androidx.preference.Preference;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.settings.IdentityErrorCardPreference;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.DataType;

import java.util.Set;

/** Test for ManageSyncSettings with FakeSyncServiceImpl. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSyncSettingsWithFakeSyncServiceImplTest {
    @Rule(order = 0)
    public final SyncTestRule mSyncTestRule =
            new SyncTestRule() {
                @Override
                protected FakeSyncServiceImpl createSyncServiceImpl() {
                    return new FakeSyncServiceImpl();
                }
            };

    // SettingsActivity has to be finished before the outer ChromeTabbedActivity can be finished,
    // otherwise trying to finish ChromeTabbedActivity won't work (SyncTestRule extends
    // ChromeTabbedActivityTestRule).
    @Rule(order = 1)
    public final SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    @Rule(order = 2)
    public final JniMocker mJniMocker = new JniMocker();

    private SettingsActivity mSettingsActivity;

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Prevent "GmsCore outdated" error from being exposed in bots with old version.
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(false);
    }

    /** Test that triggering OnPassphraseAccepted dismisses PassphraseDialogFragment. */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/986243")
    public void testPassphraseDialogDismissed() {
        final FakeSyncServiceImpl fakeSyncServiceImpl =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        // Trigger PassphraseDialogFragment to be shown when taping on Encryption.
        fakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);

        final ManageSyncSettings fragment = startManageSyncPreferences();
        Preference encryption = fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);
        clickPreference(encryption);

        final PassphraseDialogFragment passphraseFragment =
                ActivityTestUtils.waitForFragment(
                        mSettingsActivity, ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting PassphraseRequired to false
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        fakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(false);
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
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardShownForSignedInUsers() {
        // Fake an identity error.
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setRequiresClientUpgrade(true);

        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        // The error card exists.
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardNotShownIfNoError() {
        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardNotShownForSyncingUsers() {
        // Fake an identity error.
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setRequiresClientUpgrade(true);

        // Expect no records.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in, enable sync and open settings.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardDynamicallyShownOnError() {
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();

        // Expect no records initially.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        // No error card exists right now.
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();

        watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);

        // Error card is showing now.
        onViewWaiting(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardDynamicallyHidden() {
        // Fake an identity error.
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setRequiresClientUpgrade(true);

        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        IdentityErrorCardPreference preference =
                (IdentityErrorCardPreference)
                        fragment.findPreference(
                                ManageSyncSettings.PREF_IDENTITY_ERROR_CARD_PREFERENCE);

        // The error card exists right now.
        Assert.assertTrue(preference.isShown());
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();

        // Expect no records now.
        watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Clear the error.
        fakeSyncService.setRequiresClientUpgrade(false);

        // The error card is now hidden.
        Assert.assertFalse(preference.isShown());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardActionForAuthError() throws Exception {
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        // The error card exists.
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));

        FakeAccountManagerFacade fakeAccountManagerFacade =
                spy((FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance());
        AccountManagerFacadeProvider.setInstanceForTests(fakeAccountManagerFacade);

        doAnswer(
                        invocation -> {
                            // Simulate re-auth by clearing the auth error.
                            fakeSyncService.setAuthError(GoogleServiceAuthError.State.NONE);
                            return null;
                        })
                .when(fakeAccountManagerFacade)
                .updateCredentials(any(), any(), any());

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        // No error card exists anymore.
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardActionForClientOutdatedError() throws Exception {
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setRequiresClientUpgrade(true);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        // The error card exists.
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));

        Intents.init();
        // Stub all external intents.
        intending(IntentMatchers.anyIntent())
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        intended(IntentMatchers.hasDataString(startsWith("market")));
        Intents.release();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testTrustedVaultKeyRetrievalForSignedInUsers() {
        // TODO(crbug.com/334124078): Simplify the test using FakeTrustedVaultClientBackend once the
        // bug is resolved.
        TestTrustedVaultClientBackend backend = new TestTrustedVaultClientBackend();
        TrustedVaultClient.setInstanceForTesting(new TrustedVaultClient(backend));

        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setEngineInitialized(true);
        fakeSyncService.setTrustedVaultKeyRequired(true);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();

        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        Preference encryption = fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);

        // Check text summary.
        String expectedSummary = fragment.getString(R.string.identity_error_card_button_verify);
        Assert.assertEquals(encryption.getSummary().toString(), expectedSummary);

        // Mimic the user tapping on Encryption.
        clickPreference(encryption);

        CriteriaHelper.pollUiThread(() -> backend.isSuccess());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignOutUnsavedDataDialogShown() {
        final FakeSyncServiceImpl fakeSyncService =
                (FakeSyncServiceImpl) mSyncTestRule.getSyncService();
        fakeSyncService.setTypesWithUnsyncedData(Set.of(DataType.BOOKMARKS));
        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        ManageSyncSettings fragment = startManageSyncPreferences();
        onViewWaiting(allOf(is(fragment.getView()), isDisplayed()));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(R.string.sign_out)).perform(click());

        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    private ManageSyncSettings startManageSyncPreferences() {
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }

    private void clickPreference(final Preference pref) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> pref.getOnPreferenceClickListener().onPreferenceClick(pref));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    // An empty implementation to test only the fact that "something" happens when the encryption
    // dialog is clicked.
    public static class TestTrustedVaultClientBackend extends TrustedVaultClient.EmptyBackend {
        private boolean mSuccess;

        public TestTrustedVaultClientBackend() {
            mSuccess = false;
        }

        public boolean isSuccess() {
            return mSuccess;
        }

        @Override
        public Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo) {
            mSuccess = true;
            return super.createKeyRetrievalIntent(accountInfo);
        }
    }
}
