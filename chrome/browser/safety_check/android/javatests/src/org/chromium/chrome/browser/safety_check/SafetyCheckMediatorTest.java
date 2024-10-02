// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_manager.PasswordCheckReferrer.SAFETY_CHECK;
import static org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.COMPROMISED_PASSWORDS_COUNT;
import static org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.PASSWORDS_STATE;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SAFE_BROWSING_STATE;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UPDATES_STATE;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.os.Handler;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.intent.Intents;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncherFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_check_wrapper.FakePasswordCheckControllerFactory;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordStorageType;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckNativeException;
import org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckMediator.SafetyCheckInteractions;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Optional;

/** Unit tests for {@link SafetyCheckMediator}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(
        ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
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

    @Rule public JniMocker mJniMocker = new JniMocker();

    private PropertyModel mSafetyCheckModel;
    private PropertyModel mPasswordCheckModel;

    @Mock private SafetyCheckBridge.Natives mSafetyCheckBridge;
    @Mock private Profile mProfile;
    @Mock private SafetyCheckUpdatesDelegate mUpdatesDelegate;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private SyncConsentActivityLauncher mSyncLauncher;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private SyncService mSyncService;
    @Mock private Handler mHandler;
    @Mock private PasswordCheck mPasswordCheck;
    // TODO(crbug.com/40854050): Use existing fake instead of mocking
    @Mock private PasswordCheckupClientHelper mPasswordCheckupHelper;
    @Mock private CredentialManagerLauncher mCredentialManagerLauncher;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    // TODO(crbug.com/40854050): Use fake instead of mocking
    @Mock private PasswordManagerBackendSupportHelper mBackendSupportHelperMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;
    @Mock LoadingModalDialogCoordinator mLoadingModalDialogCoordinator;
    private FakePasswordCheckControllerFactory mPasswordCheckControllerFactory;

    private SafetyCheckMediator mMediator;

    private boolean mUseGmsApi;

    private ModalDialogManager mModalDialogManager;

    private final ObservableSupplierImpl<ModalDialogManager> mModalDialogManagerSupplier =
            new ObservableSupplierImpl<>();

    private LoadingModalDialogCoordinator.Observer mLoadingDialogCoordinatorObserver;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{false}, {true}});
    }

    public SafetyCheckMediatorTest(boolean useGmsApi) {
        mUseGmsApi = useGmsApi;
        ContextUtils.initApplicationContextForTests(ApplicationProvider.getApplicationContext());
    }

    private void setUpPasswordCheckToReturnError(
            @PasswordStorageType int passwordStorageType, Exception error) {
        mPasswordCheckControllerFactory
                .getLastCreatedController()
                .setPasswordCheckResult(passwordStorageType, new PasswordCheckResult(error));
    }

    private void setUpPasswordCheckToReturnNoPasswords(
            @PasswordStorageType int passwordStorageType) {
        if (mUseGmsApi) {
            mPasswordCheckControllerFactory
                    .getLastCreatedController()
                    .setPasswordCheckResult(
                            passwordStorageType,
                            new PasswordCheckResult(
                                    /* totalPasswordsCount= */ 0, /* breachedCount= */ 00));
        } else {
            PasswordCheckNativeException noPasswordsError =
                    new PasswordCheckNativeException(
                            "Test exception", PasswordCheckUIStatus.ERROR_NO_PASSWORDS);
            mPasswordCheckControllerFactory
                    .getLastCreatedController()
                    .setPasswordCheckResult(
                            passwordStorageType, new PasswordCheckResult(noPasswordsError));
        }
    }

    private void setUpPasswordCheckToReturnResult(
            @PasswordStorageType int passwordStorageType, PasswordCheckResult result) {
        mPasswordCheckControllerFactory
                .getLastCreatedController()
                .setPasswordCheckResult(passwordStorageType, result);
    }

    private void configureMockSyncService() {
        // SyncService is injected in the mediator, but dependencies still access the factory.
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.hasSyncConsent()).thenReturn(true);
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));
        when(mPasswordManagerHelperNativeMock.hasChosenToSyncPasswords(mSyncService))
                .thenReturn(true);

        // TODO(crbug.com/41483841): Parametrize the tests in SafetyCheckMediatorTest for local and
        // account storage.
        // This will no longer be true once the local and account store split happens.
        if (mUseGmsApi) {
            when(mSyncService.getSelectedTypes())
                    .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        } else {
            when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        }
    }

    private SafetyCheckMediator createSafetyCheckMediator(
            PropertyModel passwordCheckAccountModel, PropertyModel passwordCheckLocalModel) {
        return new SafetyCheckMediator(
                mProfile,
                mSafetyCheckModel,
                passwordCheckAccountModel,
                passwordCheckLocalModel,
                mUpdatesDelegate,
                new SafetyCheckBridge(mProfile),
                mSigninLauncher,
                mSyncLauncher,
                mSyncService,
                mPrefService,
                mPasswordStoreBridge,
                mPasswordCheckControllerFactory,
                PasswordManagerHelper.getForProfile(mProfile),
                mHandler,
                mModalDialogManagerSupplier);
    }

    private Preference.OnPreferenceClickListener getPasswordsClickListener(
            PropertyModel passwordCheckModel) {
        return (Preference.OnPreferenceClickListener)
                passwordCheckModel.get(PasswordsCheckPreferenceProperties.PASSWORDS_CLICK_LISTENER);
    }

    private void click(Preference.OnPreferenceClickListener listener) {
        listener.onPreferenceClick(new Preference(ContextUtils.getApplicationContext()));
    }

    @Before
    public void setUp() throws PasswordCheckBackendException, CredentialManagerBackendException {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mJniMocker.mock(PasswordManagerHelperJni.TEST_HOOKS, mPasswordManagerHelperNativeMock);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        configureMockSyncService();

        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        PasswordManagerBackendSupportHelper.setInstanceForTesting(mBackendSupportHelperMock);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeNativeMock.areMinUpmRequirementsMet()).thenReturn(true);

        // Availability of the UPM backend will be checked by the SafetyCheckMediator using
        // PasswordManagerHelper so the bridge method needs to be mocked.
        // The parameter mUseGmsApi currently means that the mock SyncService will be configured to
        // sync passwords, which so far is the only case in which the GMS APIs can be used.
        when(mPasswordManagerUtilBridgeNativeMock.shouldUseUpmWiring(mSyncService, mPrefService))
                .thenReturn(mUseGmsApi);

        mJniMocker.mock(SafetyCheckBridgeJni.TEST_HOOKS, mSafetyCheckBridge);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(false);

        mSafetyCheckModel = SafetyCheckProperties.createSafetyCheckModel();
        mPasswordCheckModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        mPasswordCheckControllerFactory = new FakePasswordCheckControllerFactory();
        if (mUseGmsApi) {
            // TODO(crbug.com/40854050): Use existing fake instead of mocking
            PasswordCheckupClientHelperFactory mockPasswordCheckFactory =
                    mock(PasswordCheckupClientHelperFactory.class);
            when(mockPasswordCheckFactory.createHelper()).thenReturn(mPasswordCheckupHelper);
            PasswordCheckupClientHelperFactory.setFactoryForTesting(mockPasswordCheckFactory);
            CredentialManagerLauncherFactory mockCredentialManagerLauncherFactory =
                    mock(CredentialManagerLauncherFactory.class);
            when(mockCredentialManagerLauncherFactory.createLauncher())
                    .thenReturn(mCredentialManagerLauncher);
            CredentialManagerLauncherFactory.setFactoryForTesting(
                    mockCredentialManagerLauncherFactory);
        } else {
            PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        }
        mMediator =
                createSafetyCheckMediator(mPasswordCheckModel, /* passwordCheckLocalModel= */ null);

        // Execute any delayed tasks immediately.
        doAnswer(
                        invocation -> {
                            Runnable runnable = (Runnable) invocation.getArguments()[0];
                            runnable.run();
                            return null;
                        })
                .when(mHandler)
                .postDelayed(any(Runnable.class), anyLong());
        // User is always signed in unless the test specifies otherwise.
        doReturn(true).when(mSafetyCheckBridge).userSignedIn(any(BrowserContextHandle.class));
        // Reset the histogram count.

        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        mModalDialogManagerSupplier.set(mModalDialogManager);
        doAnswer(
                        invocation -> {
                            mLoadingDialogCoordinatorObserver = invocation.getArgument(0);
                            return null;
                        })
                .when(mLoadingModalDialogCoordinator)
                .addObserver(any(LoadingModalDialogCoordinator.Observer.class));
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
        assertEquals(UpdatesState.UPDATED, mSafetyCheckModel.get(UPDATES_STATE));
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
        assertEquals(UpdatesState.OUTDATED, mSafetyCheckModel.get(UPDATES_STATE));
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
        assertEquals(
                SafeBrowsingState.ENABLED_STANDARD, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
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
        assertEquals(SafeBrowsingState.DISABLED, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM, SafeBrowsingStatus.DISABLED));
    }

    @Test
    public void testPasswordsCheckError() {
        mMediator.performSafetyCheck();
        setUpPasswordCheckToReturnError(
                PasswordStorageType.ACCOUNT_STORAGE, new Exception("Test exception"));

        assertEquals(PasswordsState.ERROR, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsCheckBackendOutdated() {
        if (!mUseGmsApi) return;

        mMediator.performSafetyCheck();
        setUpPasswordCheckToReturnError(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckBackendException(
                        "test", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));

        assertEquals(
                PasswordsState.BACKEND_VERSION_NOT_SUPPORTED,
                mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsCheckNoPasswords() {
        mMediator.performSafetyCheck();
        setUpPasswordCheckToReturnNoPasswords(PasswordStorageType.ACCOUNT_STORAGE);

        assertEquals(PasswordsState.NO_PASSWORDS, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.NO_PASSWORDS));
    }

    @Test
    public void testPasswordsCheckNoLeaks() {
        mMediator.performSafetyCheck();
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 0));

        assertEquals(PasswordsState.SAFE, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.SAFE));
    }

    @Test
    public void testPasswordsCheckHasLeaks() {
        final int numLeaks = 123;

        mMediator.performSafetyCheck();
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 199, numLeaks));

        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(numLeaks, mPasswordCheckModel.get(COMPROMISED_PASSWORDS_COUNT));
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
        // Passwords: safe state.
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 12, /* breachedCount= */ 0));

        // Verify the states.
        assertEquals(
                SafeBrowsingState.ENABLED_STANDARD, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.SAFE, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.OUTDATED, mSafetyCheckModel.get(UPDATES_STATE));
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
        // Passwords: no passwords.
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 0, /* breachedCount= */ 0));

        // Verify the states.
        assertEquals(
                SafeBrowsingState.DISABLED_BY_ADMIN, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.NO_PASSWORDS, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.OFFLINE, mSafetyCheckModel.get(UPDATES_STATE));
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
        // Passwords: compromised state.
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));

        // Verify the states.
        assertEquals(SafeBrowsingState.DISABLED, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UPDATED, mSafetyCheckModel.get(UPDATES_STATE));
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
        // Passwords: safe state.
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 13, /* breachedCount= */ 0));

        // Verify the states.
        assertEquals(SafeBrowsingState.UNCHECKED, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.UNCHECKED, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UNCHECKED, mSafetyCheckModel.get(UPDATES_STATE));
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
        // Passwords: compromised state.
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));

        // Verify the states.
        assertEquals(SafeBrowsingState.UNCHECKED, mSafetyCheckModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UNCHECKED, mSafetyCheckModel.get(UPDATES_STATE));
    }

    @Test
    public void testPasswordsInitialLoadDuringInitialState() {
        // Order: setting initial state -> showing CHECK while the check is still running -> done.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsInitialLoadDuringRunningCheck() {
        // Order: initial state -> safety check triggered -> load completed -> check done.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testPasswordCheckWhenRanImmediately() {
        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 6, /* breachedCount= */ 3));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsInitialLoadCheckReturnsError() {
        // Order: initial state -> safety check triggered -> check error -> load ignored.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnError(
                PasswordStorageType.ACCOUNT_STORAGE, new Exception("Test exception"));
        assertEquals(PasswordsState.ERROR, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsInitialLoadUserSignedOut() {
        // Order: initial state is user signed out -> should display signed out error.
        mMediator.setInitialState();
        setUpPasswordCheckToReturnError(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckNativeException(
                        "Test signed out error", PasswordCheckUIStatus.ERROR_SIGNED_OUT));

        assertEquals(PasswordsState.SIGNED_OUT, mPasswordCheckModel.get(PASSWORDS_STATE));
        // The results of the previous check should be ignored.
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.SIGNED_OUT));
    }

    @Test
    public void testPasswordCheckFinishedAfterDestroy() {
        mMediator.performSafetyCheck();

        @PasswordsState int stateBeforeDestroy = mPasswordCheckModel.get(PASSWORDS_STATE);
        mMediator.destroy();
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 0));

        // After calling destroy() on mediator, the model is not expected to change any more.
        assertEquals(stateBeforeDestroy, mPasswordCheckModel.get(PASSWORDS_STATE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testClickListenerStartsSignInFlowWhenUserSignedOut() {
        mMediator.setInitialState();
        setUpPasswordCheckToReturnError(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckNativeException(
                        "Test signed out error", PasswordCheckUIStatus.ERROR_SIGNED_OUT));
        assertEquals(PasswordsState.SIGNED_OUT, mPasswordCheckModel.get(PASSWORDS_STATE));

        click(getPasswordsClickListener(mPasswordCheckModel));

        verify(mSigninLauncher)
                .launchActivityIfAllowed(
                        any(),
                        eq(mProfile),
                        any(),
                        eq(SigninAndHistorySyncCoordinator.NoAccountSigninMode.ADD_ACCOUNT),
                        eq(
                                SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.SAFETY_CHECK));
    }

    @Test
    public void testClickListenerLeadsToUPMAccountPasswordCheckup() {
        // Order: initial state -> safety check triggered -> check done -> load completed.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* passwordsTotalCount= */ 20, /* breachedCount= */ 18));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));

        click(getPasswordsClickListener(mPasswordCheckModel));

        verify(mPasswordCheckupHelper, times(mUseGmsApi ? 1 : 0))
                .getPasswordCheckupIntent(
                        eq(SAFETY_CHECK), eq(Optional.of(TEST_EMAIL_ADDRESS)), any(), any());
    }

    @Test
    public void testClickListenerDontLeadToPasswordCheckupIfThereWasError() {
        // Order: initial state -> safety check triggered -> check done -> load completed.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        setUpPasswordCheckToReturnError(
                PasswordStorageType.ACCOUNT_STORAGE, new Exception("Test exception"));

        assertEquals(PasswordsState.ERROR, mPasswordCheckModel.get(PASSWORDS_STATE));
        assertNull(getPasswordsClickListener(mPasswordCheckModel));
    }

    @Test
    public void testClickListenerLeadsToPasswordSettingsWhenUnchecked() {
        if (!mUseGmsApi) {
            Intents.init();

            Intent settingsLauncherIntent = new Intent();
            settingsLauncherIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            when(mSettingsNavigation.createSettingsIntent(any(), anyInt(), any()))
                    .thenReturn(settingsLauncherIntent);
            intending(anyIntent())
                    .respondWith(new ActivityResult(Activity.RESULT_OK, new Intent()));
        }
        PropertyModel passwordCheckLocalModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        PropertyModel passwordCheckAccountModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel(
                        "Passwords for acount");
        mMediator = createSafetyCheckMediator(passwordCheckAccountModel, passwordCheckLocalModel);

        mMediator.setInitialState();
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE, new PasswordCheckResult(10, 0));

        assertEquals(PasswordsState.UNCHECKED, passwordCheckLocalModel.get(PASSWORDS_STATE));
        click(getPasswordsClickListener(passwordCheckLocalModel));

        verify(mCredentialManagerLauncher, times(mUseGmsApi ? 1 : 0))
                .getLocalCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.SAFETY_CHECK), any(), any());
        verify(mSettingsNavigation, times(mUseGmsApi ? 0 : 1))
                .createSettingsIntent(any(), anyInt(), any());
    }

    @Test
    public void testClickListenerLeadsToUPMLocalPasswordCheckup() {
        // TODO(crbug.com/41483841): Parametrize the tests in SafetyCheckMediatorTest for local and
        // account storage.
        // These behaviours are set here again because the tests are currently not parametrised in
        // a way to support UPM for non password syncing users.
        PropertyModel passwordCheckLocalModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        mMediator =
                createSafetyCheckMediator(
                        /* passwordCheckAccountModel= */ null, passwordCheckLocalModel);

        when(mPasswordManagerUtilBridgeNativeMock.shouldUseUpmWiring(mSyncService, mPrefService))
                .thenReturn(mUseGmsApi);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());

        // Order: initial state -> safety check triggered -> check done -> load completed.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE,
                new PasswordCheckResult(/* passwordsTotalCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckLocalModel.get(PASSWORDS_STATE));

        click(getPasswordsClickListener(passwordCheckLocalModel));

        verify(mPasswordCheckupHelper, times(mUseGmsApi ? 1 : 0))
                .getPasswordCheckupIntent(eq(SAFETY_CHECK), eq(Optional.empty()), any(), any());
    }

    @Test
    public void testPasswordCheckCompletesForTwoStorages() {
        // Set up both local and account models
        PropertyModel passwordCheckAccountModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel(
                        "Passwords saved on " + TEST_EMAIL_ADDRESS);
        PropertyModel passwordCheckLocalModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        mMediator = createSafetyCheckMediator(passwordCheckAccountModel, passwordCheckLocalModel);

        // Order: initial state -> set result of the initial check -> password check -> set result
        // of the password check.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, passwordCheckAccountModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* passwordsTotalCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckAccountModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE,
                new PasswordCheckResult(/* passwordsTotalCount= */ 20, /* breachedCount= */ 0));
        assertEquals(PasswordsState.UNCHECKED, passwordCheckLocalModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, passwordCheckAccountModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE,
                new PasswordCheckResult(/* passwordsTotalCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckLocalModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckAccountModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* passwordsTotalCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckAccountModel.get(PASSWORDS_STATE));
    }
}
