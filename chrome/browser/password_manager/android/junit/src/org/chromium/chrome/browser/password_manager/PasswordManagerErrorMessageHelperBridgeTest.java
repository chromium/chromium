// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.app.Activity;
import android.app.PendingIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.Promise;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for the error message helper bridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordManagerErrorMessageHelperBridgeTest {
    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private Profile mProfile;

    @Mock private PrefService mPrefService;

    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private WindowAndroid mWindowAndroidMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private TrustedVaultClient mTrustedVaultClient;

    @Mock private SyncService mSyncService;

    @Mock private PendingIntent mPendingIntent;

    private SharedPreferencesManager mSharedPrefsManager;

    private CoreAccountInfo mCoreAccountInfo;

    private static final String TEST_EMAIL = "test.account@gmail.com";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        mSharedPrefsManager = ChromeSharedPreferences.getInstance();
        mCoreAccountInfo = mAccountManagerTestRule.addAccount(TEST_EMAIL);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfile))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TrustedVaultClient.setInstanceForTesting(mTrustedVaultClient);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @After
    public void tearDown() {
        mSharedPrefsManager.removeKey(ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME);
        mFakeTimeTestRule.resetTimes();
    }

    @Test
    public void testNotEnoughTimeSinceLastUI() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        final long timeOfSyncPrompt =
                timeOfFirstUpmPrompt
                        + PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfSyncPrompt);
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS);
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUI(mProfile));
    }

    @Test
    public void testNotEnoughTimeSinceLastSyncUI() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS + 1);
        final long timeOfSyncPrompt =
                TimeUtils.currentTimeMillis()
                        - PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfSyncPrompt);
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUI(mProfile));
    }

    @Test
    public void testEnoughTimeSinceBothUis() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        final long timeOfSyncPrompt =
                timeOfFirstUpmPrompt
                        + PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;

        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfSyncPrompt);
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS + 1);
        assertTrue(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUI(mProfile));
    }

    @Test
    public void testEnoughTimeSinceLastUpmError() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS + 1);
        assertTrue(
                PasswordManagerErrorMessageHelperBridge.shouldShowUpdateGMSCoreErrorUI(mProfile));
    }

    @Test
    public void testNotEnoughTimeSinceLastUpmError() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        assertFalse(
                PasswordManagerErrorMessageHelperBridge.shouldShowUpdateGMSCoreErrorUI(mProfile));
    }

    @Test
    public void testSaveErrorUIShownTimestamp() {
        final long currentTimeMs = TimeUtils.currentTimeMillis();
        final long timeIncrementMs = 30;
        mFakeTimeTestRule.advanceMillis(timeIncrementMs);
        PasswordManagerErrorMessageHelperBridge.saveErrorUiShownTimestamp(mProfile);
        verify(mPrefService)
                .setString(
                        Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP,
                        Long.toString(currentTimeMs + timeIncrementMs));
    }

    @Test
    public void testUpdateCredentialsRecordsSuccessWhenSigningInSucceeds() {
        final Activity activity = mock(Activity.class);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(2);
                            callback.onResult(true);
                            return null;
                        })
                .when(mFakeAccountManagerFacade)
                .updateCredentials(
                        eq(CoreAccountInfo.getAndroidAccountFrom(mCoreAccountInfo)),
                        eq(activity),
                        any());

        PasswordManagerErrorMessageHelperBridge.startUpdateAccountCredentialsFlow(
                mWindowAndroidMock, mProfile);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PasswordManager.UPMUpdateSignInCredentialsSucces", 1));
        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PasswordManager.UPMUpdateSignInCredentialsSucces", 0));
    }

    @Test
    public void testUpdateCredentialsRecordsSuccessWhenSigningInFailed() {
        final Activity activity = mock(Activity.class);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(2);
                            callback.onResult(false);
                            return null;
                        })
                .when(mFakeAccountManagerFacade)
                .updateCredentials(
                        eq(CoreAccountInfo.getAndroidAccountFrom(mCoreAccountInfo)),
                        eq(activity),
                        any());

        PasswordManagerErrorMessageHelperBridge.startUpdateAccountCredentialsFlow(
                mWindowAndroidMock, mProfile);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PasswordManager.UPMUpdateSignInCredentialsSucces", 0));
        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PasswordManager.UPMUpdateSignInCredentialsSucces", 1));
    }

    @Test
    public void testDontShowMessageOnIncognito() {
        when(mIdentityServicesProviderMock.getIdentityManager(mProfile)).thenReturn(null);
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        final long timeOfSyncPrompt =
                timeOfFirstUpmPrompt
                        + PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfSyncPrompt);
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS + 1);

        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUI(mProfile));
    }

    @Test
    public void testDontShowMessageWithtoutAccount() {
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUI(mProfile));
    }

    @Test
    public void testDontTryToUpdateCredentialWithNoAccount() {
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        PasswordManagerErrorMessageHelperBridge.startUpdateAccountCredentialsFlow(
                mWindowAndroidMock, mProfile);
        verify(mFakeAccountManagerFacade, never())
                .updateCredentials(any(Account.class), any(Activity.class), any(Callback.class));
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow() {
        final Activity activity = mock(Activity.class);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mSyncService.getAccountInfo()).thenReturn(mCoreAccountInfo);

        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultClient.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock, mProfile);

        intentPromise.fulfill(mPendingIntent);
        ShadowLooper.idleMainLooper();

        verify(activity).startActivity(any(), any());
    }
}
