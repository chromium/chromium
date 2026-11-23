// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;

import androidx.activity.result.ActivityResult;

import com.google.android.gms.common.api.ResolvableApiException;
import com.google.android.gms.common.api.Status;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.Promise;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.trusted_vault.SecurityDomainId;
import org.chromium.components.trusted_vault.TrustedVaultClient;
import org.chromium.components.trusted_vault.TrustedVaultUserActionTriggerForUMA;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.MockitoHelper;

import java.lang.ref.WeakReference;
import java.util.Optional;

/** Unit tests for the error message helper bridge. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.PASSWORD_SAVE_IN_CONTEXT_ERROR_RESOLUTION)
public class PasswordManagerErrorMessageHelperBridgeTest {
    /** Overrides getActivityResultTracker() so the test can verify interactions with it. */
    private static class TestActivity extends ChromeBaseAppCompatActivity {
        private final ActivityResultTracker mTracker = mock(ActivityResultTracker.class);

        @Override
        public ActivityResultTracker getActivityResultTracker() {
            return mTracker;
        }
    }

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private Profile mProfile;

    @Mock private PrefService mPrefService;

    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private WindowAndroid mWindowAndroidMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private TrustedVaultClient.Backend mTrustedVaultBackend;

    @Mock private PasswordStoreAndroidBackendFactory mBackendFactoryMock;

    @Mock private PasswordStoreAndroidBackend mBackendMock;

    @Mock private SyncService mSyncService;

    @Mock private PendingIntent mPendingIntent;

    @Captor private ArgumentCaptor<Callback<byte[]>> mLoginsReplyCaptor;

    @Captor private ArgumentCaptor<Callback<Exception>> mFailureCallbackCaptor;

    @Captor private ArgumentCaptor<ActivityResultTracker.ResultListener> mResultListenerCaptor;

    @Captor private ArgumentCaptor<Intent> mProxyIntentCaptor;

    private SharedPreferencesManager mSharedPrefsManager;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        mSharedPrefsManager = ChromeSharedPreferences.getInstance();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfile))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TrustedVaultClient.get(SecurityDomainId.CHROME_SYNC)
                .setBackendForTesting(mTrustedVaultBackend);
        PasswordStoreAndroidBackendFactory.setFactoryInstanceForTesting(mBackendFactoryMock);
        when(mBackendFactoryMock.createBackend()).thenReturn(mBackendMock);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @After
    public void tearDown() {
        mSharedPrefsManager.removeKey(ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME);
        mFakeTimeTestRule.resetTimes();
    }

    @Test
    public void testNotEnoughTimeSinceLastUi() {
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
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUi(mProfile));
    }

    @Test
    public void testNotEnoughTimeSinceLastSyncUi() {
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
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUi(mProfile));
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
        assertTrue(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUi(mProfile));
    }

    @Test
    public void testSaveErrorUiShownTimestamp() {
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
    public void testUpdateCredentialsCallsAccountManager() {
        final Activity activity = Robolectric.buildActivity(Activity.class).get();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));

        PasswordManagerErrorMessageHelperBridge.startUpdateAccountCredentialsFlow(
                mWindowAndroidMock, mProfile);

        verify(mFakeAccountManagerFacade)
                .updateCredentials(eq(TestAccounts.ACCOUNT1.getId()), eq(activity), any());
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

        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUi(mProfile));
    }

    @Test
    public void testDontShowMessageWithtoutAccount() {
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(null);
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowSignInErrorUi(mProfile));
    }

    @Test
    public void testDontTryToUpdateCredentialWithNoAccount() {
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(null);
        PasswordManagerErrorMessageHelperBridge.startUpdateAccountCredentialsFlow(
                mWindowAndroidMock, mProfile);
        verify(mFakeAccountManagerFacade, never())
                .updateCredentials(
                        any(CoreAccountId.class), any(Activity.class), MockitoHelper.anyCallback());
    }

    private static ResolvableApiException createResolvableException(
            @ChromeSyncStatusCode int statusCode, PendingIntent resolution) {
        return new ResolvableApiException(
                new Status(statusCode, /* statusMessage= */ "", resolution));
    }

    private TestActivity setUpSyncingTestActivity() {
        TestActivity activity = Robolectric.buildActivity(TestActivity.class).get();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mSyncService.getAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);
        return activity;
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_SAVE_IN_CONTEXT_ERROR_RESOLUTION)
    public void testStartTrustedVaultKeyRetrievalFlow_WithActivityResultTracker_Success() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultBackend.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.ACCOUNT_MENU,
                completionCallback);

        intentPromise.fulfill(mPendingIntent);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mBackendMock, never()).getAllLogins(any(), any(), any());

        verify(tracker).register(mResultListenerCaptor.capture());
        verify(tracker).startActivity(eq(mResultListenerCaptor.getValue()), any(), eq(null));

        ActivityResultTracker.ResultListener capturedListener = mResultListenerCaptor.getValue();
        assertTrue(
                capturedListener
                        .getRestorationKey()
                        .startsWith(
                                PasswordManagerErrorMessageHelperBridge
                                        .TRUSTED_VAULT_KEY_RETRIEVAL_RESTORATION_KEY_NOT_TO_BE_RESTORED));

        // Verify callback is not called before activity result is delivered.
        verify(completionCallback, never()).run();

        // Simulate activity result returning.
        ActivityResult result = new ActivityResult(Activity.RESULT_OK, /* data= */ null);
        capturedListener.onActivityResult(result, null);

        verify(tracker).unregister(capturedListener);
        verify(completionCallback).run();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_SAVE_IN_CONTEXT_ERROR_RESOLUTION)
    public void testStartTrustedVaultKeyRetrievalFlow_TrackerStartActivityThrows() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        doThrow(new ActivityNotFoundException()).when(tracker).startActivity(any(), any(), any());

        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultBackend.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.ACCOUNT_MENU,
                completionCallback);

        intentPromise.fulfill(mPendingIntent);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(tracker).unregister(any());
        verify(completionCallback).run();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_SAVE_IN_CONTEXT_ERROR_RESOLUTION)
    public void testStartTrustedVaultKeyRetrievalFlow_IntentPromiseFails() {
        final Activity activity =
                Robolectric.buildActivity(ChromeBaseAppCompatActivity.class).get();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mSyncService.getAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultBackend.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.ACCOUNT_MENU,
                completionCallback);

        intentPromise.reject(new Exception());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_NoAccount() {
        when(mSyncService.getAccountInfo()).thenReturn(null);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.ACCOUNT_MENU,
                completionCallback);

        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_NullActivityRunsCallback() {
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(null));
        when(mSyncService.getAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.ACCOUNT_MENU,
                completionCallback);

        verify(mTrustedVaultBackend, never()).createKeyRetrievalIntent(any());
        verify(mBackendMock, never()).getAllLogins(any(), any(), any());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_ResolvableException() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mTrustedVaultBackend, never()).createKeyRetrievalIntent(any());
        verify(mBackendMock)
                .getAllLogins(
                        eq(
                                Optional.of(
                                        AccountUtils.createAccountFromEmail(
                                                TestAccounts.ACCOUNT1.getEmail()))),
                        any(),
                        mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, mPendingIntent));

        verify(tracker).register(mResultListenerCaptor.capture());
        verify(tracker)
                .startActivity(
                        eq(mResultListenerCaptor.getValue()),
                        mProxyIntentCaptor.capture(),
                        eq(null));

        Intent expectedProxyIntent =
                TrustedVaultClient.get(SecurityDomainId.CHROME_SYNC)
                        .getProxyIntentFactory()
                        .createKeyRetrievalProxyIntent(
                                mPendingIntent,
                                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                                SecurityDomainId.CHROME_SYNC);
        Intent proxyIntent = mProxyIntentCaptor.getValue();
        assertTrue(expectedProxyIntent.filterEquals(proxyIntent));
        assertEquals(expectedProxyIntent.getFlags(), proxyIntent.getFlags());
        Bundle expectedExtras = expectedProxyIntent.getExtras();
        Bundle actualExtras = proxyIntent.getExtras();
        assertNotNull(expectedExtras);
        assertNotNull(actualExtras);
        assertEquals(expectedExtras.keySet(), actualExtras.keySet());
        for (String key : expectedExtras.keySet()) {
            assertEquals(expectedExtras.get(key), actualExtras.get(key));
        }

        ActivityResultTracker.ResultListener capturedListener = mResultListenerCaptor.getValue();
        String expectedPrefix =
                PasswordManagerErrorMessageHelperBridge
                        .TRUSTED_VAULT_KEY_RETRIEVAL_RESTORATION_KEY_NOT_TO_BE_RESTORED;
        assertTrue(capturedListener.getRestorationKey().startsWith(expectedPrefix));

        verify(completionCallback, never()).run();

        ActivityResult result = new ActivityResult(Activity.RESULT_OK, /* data= */ null);
        capturedListener.onActivityResult(result, null);

        verify(tracker).unregister(capturedListener);
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_FolsomEmptyDomain() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.FOLSOM_EMPTY_DOMAIN, mPendingIntent));

        verify(tracker).register(mResultListenerCaptor.capture());
        verify(tracker).startActivity(eq(mResultListenerCaptor.getValue()), any(), eq(null));
        verify(completionCallback, never()).run();

        ActivityResult result = new ActivityResult(Activity.RESULT_OK, /* data= */ null);
        mResultListenerCaptor.getValue().onActivityResult(result, null);

        verify(tracker).unregister(mResultListenerCaptor.getValue());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_FolsomIrretrievableDomain() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.FOLSOM_IRRETRIEVABLE_DOMAIN, mPendingIntent));

        verify(tracker).register(mResultListenerCaptor.capture());
        verify(tracker).startActivity(eq(mResultListenerCaptor.getValue()), any(), eq(null));
        verify(completionCallback, never()).run();

        ActivityResult result = new ActivityResult(Activity.RESULT_OK, /* data= */ null);
        mResultListenerCaptor.getValue().onActivityResult(result, null);

        verify(tracker).unregister(mResultListenerCaptor.getValue());
        verify(completionCallback).run();
    }

    @Test
    public void
            testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_NonKeyRetrievalResolvableException() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.AUTH_ERROR_RESOLVABLE, mPendingIntent));

        verify(tracker, never()).register(any());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_StartActivityThrows() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        doThrow(new ActivityNotFoundException()).when(tracker).startActivity(any(), any(), any());

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, mPendingIntent));

        verify(tracker).unregister(any());
        verify(completionCallback).run();
    }

    @Test
    public void
            testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_StartActivityThrowsSecurityException() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        doThrow(new SecurityException()).when(tracker).startActivity(any(), any(), any());

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, mPendingIntent));

        verify(tracker).unregister(any());
        verify(completionCallback).run();
    }

    @Test
    public void
            testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_StartActivityThrowsUnexpectedException() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        doThrow(new NullPointerException()).when(tracker).startActivity(any(), any(), any());

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        Callback<Exception> failureCallback = mFailureCallbackCaptor.getValue();
        ResolvableApiException resolvableException =
                createResolvableException(
                        ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, mPendingIntent);
        assertThrows(
                NullPointerException.class, () -> failureCallback.onResult(resolvableException));
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ActivityFinishingAtStartRunsCallback() {
        final TestActivity activity = setUpSyncingTestActivity();
        activity.finish();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mTrustedVaultBackend, never()).createKeyRetrievalIntent(any());
        verify(mBackendMock, never()).getAllLogins(any(), any(), any());
        verify(completionCallback).run();
    }

    @Test
    public void
            testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_ActivityFinishingBeforeCallback() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        activity.finish();
        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, mPendingIntent));

        verify(tracker, never()).register(any());
        verify(completionCallback).run();
    }

    @Test
    public void
            testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_ActivityDestroyedBeforeCallback() {
        final ActivityController<TestActivity> controller =
                Robolectric.buildActivity(TestActivity.class);
        final TestActivity activity = controller.get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        controller.create();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        when(mSyncService.getAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        controller.destroy();
        assertTrue(activity.isDestroyed());
        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, mPendingIntent));

        verify(tracker, never()).register(any());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_NonResolvableFailure() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor.getValue().onResult(new Exception("Non-resolvable error"));

        verify(tracker, never()).register(any());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_NullResolution() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), any(), mFailureCallbackCaptor.capture());

        mFailureCallbackCaptor
                .getValue()
                .onResult(
                        createResolvableException(
                                ChromeSyncStatusCode.KEY_RETRIEVAL_REQUIRED, null));

        verify(tracker, never()).register(any());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_LoginsSucceed() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock).getAllLogins(any(), mLoginsReplyCaptor.capture(), any());

        mLoginsReplyCaptor.getValue().onResult(new byte[0]);

        verify(tracker, never()).register(any());
        verify(completionCallback).run();
    }

    @Test
    public void testStartTrustedVaultKeyRetrievalFlow_ViaPasswordStore_NullBackend() {
        final TestActivity activity = setUpSyncingTestActivity();
        final ActivityResultTracker tracker = activity.getActivityResultTracker();
        when(mBackendFactoryMock.createBackend()).thenReturn(null);

        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultBackend.createKeyRetrievalIntent(TestAccounts.ACCOUNT1))
                .thenReturn(intentPromise);

        Runnable completionCallback = mock(Runnable.class);

        PasswordManagerErrorMessageHelperBridge.startTrustedVaultKeyRetrievalFlow(
                mWindowAndroidMock,
                mProfile,
                TrustedVaultUserActionTriggerForUMA.PASSWORD_SAVE_PROMPT,
                completionCallback);

        verify(mBackendMock, never()).getAllLogins(any(), any(), any());
        verify(mTrustedVaultBackend).createKeyRetrievalIntent(TestAccounts.ACCOUNT1);

        intentPromise.fulfill(mPendingIntent);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(tracker).register(mResultListenerCaptor.capture());
        verify(tracker).startActivity(eq(mResultListenerCaptor.getValue()), any(), eq(null));

        ActivityResult result = new ActivityResult(Activity.RESULT_OK, /* data= */ null);
        mResultListenerCaptor.getValue().onActivityResult(result, null);

        verify(completionCallback).run();
    }
}
