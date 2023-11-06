// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.COMPROMISED_PASSWORDS;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PASSWORDS_STATE;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SAFE_BROWSING_STATE;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UPDATES_STATE;

import android.os.Handler;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge.PasswordStoreObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_check.SafetyCheckMediator.SafetyCheckInteractions;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;

/** Unit tests for {@link SafetyCheckMediator}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SafetyCheckMediatorTest {
    private static final String SAFETY_CHECK_INTERACTIONS_HISTOGRAM =
            "Settings.SafetyCheck.Interactions";
    private static final String SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM =
            "Settings.SafetyCheck.PasswordsResult2";
    private static final String SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM =
            "Settings.SafetyCheck.SafeBrowsingResult";
    private static final String SAFETY_CHECK_UPDATES_RESULT_HISTOGRAM =
            "Settings.SafetyCheck.UpdatesResult";

    private static final String TEST_EMAIL_ADDRESS = "test@example.com";

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Rule public JniMocker mJniMocker = new JniMocker();

    private PropertyModel mModel;

    @Mock private SafetyCheckBridge.Natives mSafetyCheckBridge;
    @Mock private Profile mProfile;
    @Mock private SafetyCheckUpdatesDelegate mUpdatesDelegate;
    @Mock private SyncConsentActivityLauncher mSigninLauncher;
    @Mock private SettingsLauncher mSettingsLauncher;
    @Mock private SyncService mSyncService;
    @Mock private Handler mHandler;
    @Mock private PasswordCheck mPasswordCheck;

    // TODO(crbug.com/1346235): Use existing fake instead of mocking
    @Mock private PasswordCheckupClientHelper mPasswordCheckupHelper;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    // TODO(crbug.com/1346235): Use fake instead of mocking
    @Mock private PasswordManagerBackendSupportHelper mBackendSupportHelperMock;

    private SafetyCheckMediator mMediator;

    private Callback<Integer> mBreachPasswordsCallback;

    private Callback<Exception> mBreachPasswordsFailureCallback;

    private Callback<Void> mRunPasswordCheckSuccessfullyCallback;

    private Callback<Exception> mRunPasswordCheckFailedCallback;

    private boolean mUseGmsApi;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{false}, {true}});
    }

    public SafetyCheckMediatorTest(boolean useGmsApi) {
        mUseGmsApi = useGmsApi;
        ContextUtils.initApplicationContextForTests(ApplicationProvider.getApplicationContext());
    }

    private void setPasswordCheckResult(boolean hasError) {
        if (!mUseGmsApi) {
            verify(mPasswordCheck).startCheck();
            mMediator.onPasswordCheckStatusChanged(
                    hasError ? PasswordCheckUIStatus.ERROR_UNKNOWN : PasswordCheckUIStatus.IDLE);
            return;
        }
        if (hasError) {
            assertNotNull(mRunPasswordCheckFailedCallback);
            mRunPasswordCheckFailedCallback.onResult(new Exception());
        } else {
            assertNotNull(mRunPasswordCheckSuccessfullyCallback);
            mRunPasswordCheckSuccessfullyCallback.onResult(null);
        }
        mRunPasswordCheckFailedCallback = null;
        mRunPasswordCheckSuccessfullyCallback = null;
    }

    private void fetchSavedPasswords(int count) {
        if (mUseGmsApi) {
            when(mPasswordStoreBridge.getPasswordStoreCredentialsCount()).thenReturn(count);
            mMediator.onSavedPasswordsChanged(count);
        } else {
            when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(count);
            mMediator.onSavedPasswordsFetchCompleted();
        }
    }

    private void fetchBreachedPasswords(int count) {
        if (mUseGmsApi) {
            assertNotNull(mBreachPasswordsCallback);
            mBreachPasswordsCallback.onResult(count);
            mBreachPasswordsCallback = null;
        } else {
            when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(count);
            mMediator.onCompromisedCredentialsFetchCompleted();
        }
    }

    private void failBreachedPasswordsFetch() {
        if (!mUseGmsApi) return;
        assertNotNull(mBreachPasswordsFailureCallback);
        mBreachPasswordsFailureCallback.onResult(new Exception());
        mBreachPasswordsCallback = null;
        mBreachPasswordsFailureCallback = null;
    }

    private void setInitialPasswordsCount(int passwordCount, int breachedCount) {
        if (mUseGmsApi) {
            doAnswer(
                            invocation -> {
                                Callback<Integer> callback = invocation.getArgument(2);
                                callback.onResult(breachedCount);
                                return null;
                            })
                    .when(mPasswordCheckupHelper)
                    .getBreachedCredentialsCount(anyInt(), any(), any(Callback.class), any());
            setPasswordCountOnStoreBridge(passwordCount);
        } else {
            doAnswer(
                            invocation -> {
                                PasswordCheck.Observer observer =
                                        (PasswordCheck.Observer) (invocation.getArguments()[0]);
                                observer.onCompromisedCredentialsFetchCompleted();
                                observer.onSavedPasswordsFetchCompleted();
                                return null;
                            })
                    .when(mPasswordCheck)
                    .addObserver(mMediator, true);
            when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(passwordCount);
            when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(breachedCount);
        }
    }

    private void setPasswordCountOnStoreBridge(int passwordCount) {
        doAnswer(
                        invocation -> {
                            PasswordStoreObserver observer = invocation.getArgument(0);
                            observer.onSavedPasswordsChanged(passwordCount);
                            return null;
                        })
                .when(mPasswordStoreBridge)
                .addObserver(mMediator, true);
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCount()).thenReturn(passwordCount);
    }

    private void captureBreachPasswordsCallback() {
        if (!mUseGmsApi) return;
        doAnswer(
                        invocation -> {
                            mBreachPasswordsCallback = invocation.getArgument(2);
                            mBreachPasswordsFailureCallback = invocation.getArgument(3);
                            return null;
                        })
                .when(mPasswordCheckupHelper)
                .getBreachedCredentialsCount(
                        anyInt(), any(), any(Callback.class), any(Callback.class));
    }

    private void captureRunPasswordCheckCallback() {
        if (!mUseGmsApi) return;
        doAnswer(
                        invocation -> {
                            mRunPasswordCheckSuccessfullyCallback = invocation.getArgument(2);
                            mRunPasswordCheckFailedCallback = invocation.getArgument(3);
                            return null;
                        })
                .when(mPasswordCheckupHelper)
                .runPasswordCheckupInBackground(
                        anyInt(), any(), any(Callback.class), any(Callback.class));
    }

    private void configureMockSyncService() {
        // SyncService is injected in the mediator, but dependencies still access the factory.
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.hasSyncConsent()).thenReturn(true);
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));
        if (mUseGmsApi) {
            when(mSyncService.getSelectedTypes())
                    .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        } else {
            when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        }
    }

    @Before
    public void setUp() throws PasswordCheckBackendException {
        MockitoAnnotations.initMocks(this);
        configureMockSyncService();

        PasswordManagerBackendSupportHelper.setInstanceForTesting(mBackendSupportHelperMock);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(false);

        mJniMocker.mock(SafetyCheckBridgeJni.TEST_HOOKS, mSafetyCheckBridge);
        Profile.setLastUsedProfileForTesting(mProfile);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(false);

        mModel = SafetyCheckProperties.createSafetyCheckModel();
        if (mUseGmsApi) {
            // TODO(crbug.com/1346235): Use existing fake instead of mocking
            PasswordCheckupClientHelperFactory mockPasswordCheckFactory =
                    mock(PasswordCheckupClientHelperFactory.class);
            when(mockPasswordCheckFactory.createHelper()).thenReturn(mPasswordCheckupHelper);
            PasswordCheckupClientHelperFactory.setFactoryForTesting(mockPasswordCheckFactory);
            mMediator =
                    new SafetyCheckMediator(
                            mModel,
                            mUpdatesDelegate,
                            mSettingsLauncher,
                            mSigninLauncher,
                            mSyncService,
                            mPasswordStoreBridge,
                            mHandler);
        } else {
            PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
            mMediator =
                    new SafetyCheckMediator(
                            mModel,
                            mUpdatesDelegate,
                            mSettingsLauncher,
                            mSigninLauncher,
                            mSyncService,
                            null,
                            mHandler);
        }

        // Execute any delayed tasks immediately.
        doAnswer(
                        invocation -> {
                            Runnable runnable = (Runnable) (invocation.getArguments()[0]);
                            runnable.run();
                            return null;
                        })
                .when(mHandler)
                .postDelayed(any(Runnable.class), anyLong());
        // User is always signed in unless the test specifies otherwise.
        doReturn(true).when(mSafetyCheckBridge).userSignedIn(any(BrowserContextHandle.class));
        // Reset the histogram count.
        UmaRecorderHolder.resetForTesting();
    }

    @Test
    public void testStartInteractionRecorded() {
        mMediator.performSafetyCheck();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_INTERACTIONS_HISTOGRAM, SafetyCheckInteractions.STARTED));
    }

    @Test
    public void testUpdatesCheckUpdated() {
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.UPDATED);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));

        mMediator.performSafetyCheck();
        assertEquals(UpdatesState.UPDATED, mModel.get(UPDATES_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_UPDATES_RESULT_HISTOGRAM, UpdateStatus.UPDATED));
    }

    @Test
    public void testUpdatesCheckOutdated() {
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.OUTDATED);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));

        mMediator.performSafetyCheck();
        assertEquals(UpdatesState.OUTDATED, mModel.get(UPDATES_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_UPDATES_RESULT_HISTOGRAM, UpdateStatus.OUTDATED));
    }

    @Test
    public void testSafeBrowsingCheckEnabledStandard() {
        doReturn(SafeBrowsingStatus.ENABLED_STANDARD)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));

        mMediator.performSafetyCheck();
        assertEquals(SafeBrowsingState.ENABLED_STANDARD, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM,
                        SafeBrowsingStatus.ENABLED_STANDARD));
    }

    @Test
    public void testSafeBrowsingCheckDisabled() {
        doReturn(SafeBrowsingStatus.DISABLED)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));

        mMediator.performSafetyCheck();
        assertEquals(SafeBrowsingState.DISABLED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM, SafeBrowsingStatus.DISABLED));
    }

    @Test
    public void testPasswordsCheckError() {
        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        setPasswordCheckResult(/* hasError= */ true);
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsCheckBackendOutdated() {
        if (!mUseGmsApi) return;
        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        mRunPasswordCheckFailedCallback.onResult(
                new PasswordCheckBackendException(
                        "test", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));
        assertEquals(PasswordsState.BACKEND_VERSION_NOT_SUPPORTED, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsCheckNoPasswords() {
        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        captureBreachPasswordsCallback();
        setPasswordCheckResult(/* hasError= */ false);
        fetchSavedPasswords(0);
        fetchBreachedPasswords(0);
        assertEquals(PasswordsState.NO_PASSWORDS, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.NO_PASSWORDS));
    }

    @Test
    public void testPasswordsCheckNoLeaks() {
        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        captureBreachPasswordsCallback();
        setPasswordCheckResult(/* hasError= */ false);
        fetchSavedPasswords(20);
        fetchBreachedPasswords(0);
        assertEquals(PasswordsState.SAFE, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.SAFE));
    }

    @Test
    public void testPasswordsCheckHasLeaks() {
        int numLeaks = 123;
        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        captureBreachPasswordsCallback();
        setPasswordCheckResult(/* hasError= */ false);
        fetchSavedPasswords(199);
        fetchBreachedPasswords(numLeaks);
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(numLeaks, mModel.get(COMPROMISED_PASSWORDS));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testNullStateLessThan10MinsPasswordsSafeState() {
        // Ran just now.
        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        preferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis());
        // Safe Browsing: on.
        doReturn(SafeBrowsingStatus.ENABLED_STANDARD)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));
        // Passwords: safe state.
        setInitialPasswordsCount(12, 0);

        // Updates: outdated.
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.OUTDATED);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.ENABLED_STANDARD, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.SAFE, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.OUTDATED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateLessThan10MinsNoSavedPasswords() {
        // Ran just now.
        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        preferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis());
        // Safe Browsing: disabled by admin.
        doReturn(SafeBrowsingStatus.DISABLED_BY_ADMIN)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));

        // Passwords: no passwords.
        setInitialPasswordsCount(0, 0);
        // Updates: offline.
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.OFFLINE);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.DISABLED_BY_ADMIN, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.NO_PASSWORDS, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.OFFLINE, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateLessThan10MinsPasswordsUnsafeState() {
        // Ran just now.
        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        preferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis());
        // Safe Browsing: off.
        doReturn(SafeBrowsingStatus.DISABLED)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));

        // Passwords: compromised state.
        setInitialPasswordsCount(20, 18);
        // Updates: updated.
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.UPDATED);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.DISABLED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UPDATED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateMoreThan10MinsPasswordsSafeState() {
        // Ran 20 mins ago.
        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        preferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis() - (20 * 60 * 1000));
        // Safe Browsing: on.
        doReturn(SafeBrowsingStatus.ENABLED_STANDARD)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));

        // Passwords: safe state.
        setInitialPasswordsCount(13, 0);
        // Updates: outdated.
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.OUTDATED);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.UNCHECKED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.UNCHECKED, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UNCHECKED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateMoreThan10MinsPasswordsUnsafeState() {
        // Ran 20 mins ago.
        SharedPreferencesManager preferenceManager = ChromeSharedPreferences.getInstance();
        preferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis() - (20 * 60 * 1000));
        // Safe Browsing: off.
        doReturn(SafeBrowsingStatus.DISABLED)
                .when(mSafetyCheckBridge)
                .checkSafeBrowsing(any(BrowserContextHandle.class));
        // Passwords: compromised state.
        setInitialPasswordsCount(20, 18);
        // Updates: updated.
        doAnswer(
                        invocation -> {
                            Callback<Integer> callback =
                                    ((WeakReference<Callback<Integer>>)
                                                    invocation.getArguments()[0])
                                            .get();
                            callback.onResult(UpdatesState.UPDATED);
                            return null;
                        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();

        // Verify the states.
        assertEquals(SafeBrowsingState.UNCHECKED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UNCHECKED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testPasswordsInitialLoadDuringInitialState() {
        // Order: initial state -> load completed -> done.
        captureBreachPasswordsCallback();
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        fetchBreachedPasswords(18);
        // Not complete fetch - still checking.
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        // Data available.
        fetchSavedPasswords(20);
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsInitialLoadDuringRunningCheck() {
        // Order: initial state -> safety check triggered -> load completed -> check done.
        captureBreachPasswordsCallback();
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        fetchSavedPasswords(20);
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        fetchBreachedPasswords(18);
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        setPasswordCheckResult(/* hasError= */ false);
        captureBreachPasswordsCallback();
        if (mUseGmsApi) {
            fetchBreachedPasswords(18);
        }
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testPasswordCheckWhenRanImmediately() {
        final int savedPasswordsCount = 6;
        if (mUseGmsApi) {
            // Pretend there are passwords saved and they have been fetched.
            setPasswordCountOnStoreBridge(savedPasswordsCount);
        }
        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        if (!mUseGmsApi) {
            verify(mPasswordCheck).addObserver(mMediator, false);
            fetchSavedPasswords(savedPasswordsCount);
        }
        captureBreachPasswordsCallback();
        setPasswordCheckResult(/* hasError= */ false);
        fetchBreachedPasswords(3);
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsInitialLoadAfterRunningCheck() {
        // Order: initial state -> safety check triggered -> check done -> load completed.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        captureBreachPasswordsCallback();
        setPasswordCheckResult(/* hasError= */ false);
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        fetchSavedPasswords(20);
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        fetchBreachedPasswords(18);
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testPasswordsInitialLoadCheckReturnsError() {
        // Order: initial state -> safety check triggered -> check error -> load ignored.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        captureRunPasswordCheckCallback();
        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        setPasswordCheckResult(/* hasError= */ true);
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));

        // Previous check found compromises.
        fetchSavedPasswords(20);
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsInitialLoadUserSignedOut() {
        // Order: initial state is user signed out -> load ignored.
        doReturn(false).when(mSafetyCheckBridge).userSignedIn(any(BrowserContextHandle.class));
        captureBreachPasswordsCallback();
        mMediator.setInitialState();
        assertEquals(PasswordsState.SIGNED_OUT, mModel.get(PASSWORDS_STATE));

        // Previous check found compromises.
        fetchSavedPasswords(20);
        fetchBreachedPasswords(18);
        // The results of the previous check should be ignored.
        assertEquals(PasswordsState.SIGNED_OUT, mModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.SIGNED_OUT));
    }

    @Test
    public void testPasswordCheckFinishedAfterDestroy() {
        captureRunPasswordCheckCallback();
        captureBreachPasswordsCallback();
        mMediator.performSafetyCheck();
        mMediator.destroy();
        setPasswordCheckResult(/* hasError= */ false);
        assertNull(mBreachPasswordsCallback);
    }

    @Test
    public void testPasswordCheckFailedAfterDestroy() {
        captureRunPasswordCheckCallback();
        captureBreachPasswordsCallback();
        mMediator.performSafetyCheck();
        mMediator.destroy();
        setPasswordCheckResult(/* hasError= */ true);
        assertNull(mBreachPasswordsCallback);
    }

    @Test
    public void testFetchBreachedCredentialsFinishedAfterDestroy() {
        captureRunPasswordCheckCallback();
        captureBreachPasswordsCallback();
        mMediator.performSafetyCheck();
        setPasswordCheckResult(/* hasError= */ false);
        mMediator.destroy();
        fetchBreachedPasswords(10);
    }

    @Test
    public void testFetchBreachedCredentialsFailedAfterDestroy() {
        captureRunPasswordCheckCallback();
        captureBreachPasswordsCallback();
        mMediator.performSafetyCheck();
        setPasswordCheckResult(/* hasError= */ false);
        mMediator.destroy();
        failBreachedPasswordsFetch();
    }
}
