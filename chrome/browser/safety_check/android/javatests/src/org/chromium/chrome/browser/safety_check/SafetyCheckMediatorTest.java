// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
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

import android.os.Handler;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncherFactory;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_check_wrapper.FakePasswordCheckControllerFactory;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordStorageType;
import org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckMediator.SafetyCheckInteractions;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

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

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private PropertyModel mSafetyCheckModel;
    private PropertyModel mPasswordCheckModel;

    @Mock private SafetyCheckBridge.Natives mSafetyCheckBridge;
    @Mock private Profile mProfile;
    @Mock private SafetyCheckUpdatesDelegate mUpdatesDelegate;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private SyncService mSyncService;
    @Mock private Handler mHandler;
    // TODO(crbug.com/40854050): Use existing fake instead of mocking
    @Mock private PasswordCheckupClientHelper mPasswordCheckupHelper;
    @Mock private CredentialManagerLauncher mCredentialManagerLauncher;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;

    // TODO(crbug.com/40854050): Use fake instead of mocking
    @Mock private PasswordManagerBackendSupportHelper mBackendSupportHelperMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;
    @Mock LoadingModalDialogCoordinator mLoadingModalDialogCoordinator;
    private FakePasswordCheckControllerFactory mPasswordCheckControllerFactory;
    @Mock private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    private SafetyCheckMediator mMediator;

    private final boolean mUseGmsApi;

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
        mPasswordCheckControllerFactory
                .getLastCreatedController()
                .setPasswordCheckResult(
                        passwordStorageType,
                        new PasswordCheckResult(
                                /* totalPasswordsCount= */ 0, /* breachedCount= */ 00));
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
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                TEST_EMAIL_ADDRESS, new GaiaId("0")));
        when(mPasswordManagerHelperNativeMock.hasChosenToSyncPasswords(mSyncService))
                .thenReturn(true);

        // TODO(crbug.com/41483841): Parametrize the tests in SafetyCheckMediatorTest for local and
        // account storage.
        // This will no longer be true once the local and account store split happens.
        if (mUseGmsApi) {
            when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        } else {
            when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        }
    }

    private SafetyCheckMediator createSafetyCheckMediator(
            PropertyModel passwordCheckAccountModel, PropertyModel passwordCheckLocalModel) {
        return new SafetyCheckMediator(
                mSafetyCheckModel,
                passwordCheckAccountModel,
                passwordCheckLocalModel,
                mUpdatesDelegate,
                new SafetyCheckBridge(mProfile),
                mSyncService,
                mHandler,
                mPasswordStoreBridge,
                mPasswordCheckControllerFactory,
                PasswordManagerHelper.getForProfile(mProfile),
                mModalDialogManagerSupplier,
                mSettingsCustomTabLauncher);
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
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeNativeMock);
        PasswordManagerHelperJni.setInstanceForTesting(mPasswordManagerHelperNativeMock);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        configureMockSyncService();

        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        PasswordManagerBackendSupportHelper.setInstanceForTesting(mBackendSupportHelperMock);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);

        SafetyCheckBridgeJni.setInstanceForTesting(mSafetyCheckBridge);

        mSafetyCheckModel = SafetyCheckProperties.createSafetyCheckModel();
        mPasswordCheckModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        mPasswordCheckControllerFactory = new FakePasswordCheckControllerFactory();
        when(mPasswordManagerUtilBridgeNativeMock.isPasswordManagerAvailable(true))
                .thenReturn(mUseGmsApi);
        // TODO(crbug.com/40854050): Use existing fake instead of mocking
        PasswordCheckupClientHelperFactory mockPasswordCheckFactory =
                mock(PasswordCheckupClientHelperFactory.class);
        when(mockPasswordCheckFactory.createHelper()).thenReturn(mPasswordCheckupHelper);
        PasswordCheckupClientHelperFactory.setFactoryForTesting(mockPasswordCheckFactory);
        CredentialManagerLauncherFactory mockCredentialManagerLauncherFactory =
                mock(CredentialManagerLauncherFactory.class);
        when(mockCredentialManagerLauncherFactory.createLauncher())
                .thenReturn(mCredentialManagerLauncher);
        CredentialManagerLauncherFactory.setFactoryForTesting(mockCredentialManagerLauncherFactory);
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
    public void testClickListenerLeadsToUPMAccountPasswordCheckup() {
        // Order: initial state -> safety check triggered -> check done -> load completed.
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mPasswordCheckModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mPasswordCheckModel.get(PASSWORDS_STATE));

        click(getPasswordsClickListener(mPasswordCheckModel));

        verify(mPasswordCheckupHelper, times(mUseGmsApi ? 1 : 0))
                .getPasswordCheckupIntent(eq(SAFETY_CHECK), eq(TEST_EMAIL_ADDRESS), any(), any());
    }

    @Test
    public void testClickListenerDoesntLeadToPasswordCheckupIfThereWasError() {
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
        assumeTrue(mUseGmsApi);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);
        PropertyModel passwordCheckLocalModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        PropertyModel passwordCheckAccountModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel(
                        "Passwords for account");
        mMediator = createSafetyCheckMediator(passwordCheckAccountModel, passwordCheckLocalModel);

        mMediator.setInitialState();
        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE, new PasswordCheckResult(10, 0));

        assertEquals(PasswordsState.UNCHECKED, passwordCheckLocalModel.get(PASSWORDS_STATE));
        click(getPasswordsClickListener(passwordCheckLocalModel));

        verify(mCredentialManagerLauncher, times(mUseGmsApi ? 1 : 0))
                .getLocalCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.SAFETY_CHECK), any(), any());
    }

    @Test
    public void testClickListenerLeadsToUPMLocalPasswordCheckup() {
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);
        PropertyModel passwordCheckLocalModel =
                PasswordsCheckPreferenceProperties.createPasswordSafetyCheckModel("Passwords");
        mMediator =
                createSafetyCheckMediator(
                        /* passwordCheckAccountModel= */ null, passwordCheckLocalModel);

        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());

        // Order: initial state -> safety check triggered -> check done -> load completed.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckLocalModel.get(PASSWORDS_STATE));

        click(getPasswordsClickListener(passwordCheckLocalModel));

        verify(mPasswordCheckupHelper, times(mUseGmsApi ? 1 : 0))
                .getPasswordCheckupIntent(eq(SAFETY_CHECK), eq(null), any(), any());
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
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckAccountModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 0));
        assertEquals(PasswordsState.UNCHECKED, passwordCheckLocalModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, passwordCheckAccountModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckLocalModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.LOCAL_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckLocalModel.get(PASSWORDS_STATE));
        assertEquals(PasswordsState.CHECKING, passwordCheckAccountModel.get(PASSWORDS_STATE));

        setUpPasswordCheckToReturnResult(
                PasswordStorageType.ACCOUNT_STORAGE,
                new PasswordCheckResult(/* totalPasswordsCount= */ 20, /* breachedCount= */ 18));
        assertEquals(
                PasswordsState.COMPROMISED_EXIST, passwordCheckAccountModel.get(PASSWORDS_STATE));
    }
}
