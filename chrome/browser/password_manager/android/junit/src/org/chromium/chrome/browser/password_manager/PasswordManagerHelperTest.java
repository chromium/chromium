// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;
import com.google.common.base.Optional;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.sync.ModelType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.OptionalInt;

/** Tests for password manager helper methods. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowSystemClock.class, ShadowRecordHistogram.class})
@Batch(Batch.PER_CLASS)
public class PasswordManagerHelperTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    private static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    private static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    private static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    private static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    private static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    private static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    private static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    private static final String PASSWORD_CHECKUP_HISTOGRAM_BASE = "PasswordManager.PasswordCheckup";

    private static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

    private static final String LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM =
            "PasswordManager.ModalLoadingDialog.CredentialManager.Outcome";
    private static final String LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM =
            "PasswordManager.ModalLoadingDialog.PasswordCheckup.Outcome";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private CredentialManagerLauncher mCredentialManagerLauncherMock;

    @Mock
    private PasswordCheckupClientHelper mPasswordCheckupClientHelperMock;

    @Mock
    private Profile mProfile;

    @Mock
    private PrefService mPrefService;

    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;

    @Mock
    private SyncService mSyncServiceMock;

    @Mock
    private SettingsLauncher mSettingsLauncherMock;

    @Mock
    private PendingIntent mPendingIntentMock;

    @Mock
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    private ModalDialogManager mModalDialogManager;

    @Mock
    LoadingModalDialogCoordinator mLoadingModalDialogCoordinator;

    private LoadingModalDialogCoordinator.Observer mLoadingDialogCoordinatorObserver;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(false);
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.getAuthError()).thenReturn(GoogleServiceAuthError.State.NONE);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);
        mModalDialogManager = new ModalDialogManager(
                mock(ModalDialogManager.Presenter.class), ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        doAnswer(invocation -> {
            mLoadingDialogCoordinatorObserver = invocation.getArgument(0);
            return null;
        })
                .when(mLoadingModalDialogCoordinator)
                .addObserver(any(LoadingModalDialogCoordinator.Observer.class));
    }

    @Test
    public void testSyncCheckFeatureNotEnabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testSyncCheckNoSyncConsent() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(false);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsDisabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes()).thenReturn(Collections.EMPTY_SET);
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsEnabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        Assert.assertTrue(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
    }

    @Test
    public void testSyncEnabledWithCustomPassphrase() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        Assert.assertTrue(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithNoCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(false);
        Assert.assertTrue(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesCredentialManagerSync() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock, mModalDialogManagerSupplier);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForAccount(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForAccountIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock, mModalDialogManagerSupplier);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForAccountIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_ERROR);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock, mModalDialogManagerSupplier);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM, CredentialManagerError.API_ERROR));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsMetricsWhenAccountIntentFails() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock, mModalDialogManagerSupplier);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesPasswordCheckupSync() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.of(TEST_EMAIL_ADDRESS)), any(Callback.class),
                        any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesPasswordCheckupForLocal() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.absent()), any(Callback.class), any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testPasswordCheckupIntentCalledIfSuccess() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);
        verify(mPendingIntentMock).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForPasswordCheckupIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);
        checkPasswordCheckupSuccessHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForPasswordCheckupIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);

        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                CredentialManagerError.UNCATEGORIZED, OptionalInt.empty());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsApiErrorMetricsForPasswordCheckupIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);
        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                CredentialManagerError.API_ERROR,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForRunPasswordCheckup() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulRunPasswordCheckup();

        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
        checkPasswordCheckupSuccessHistogramsForOperation(
                PasswordCheckOperation.RUN_PASSWORD_CHECKUP);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForRunPasswordCheckup() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenRunningPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.RUN_PASSWORD_CHECKUP, CredentialManagerError.UNCATEGORIZED,
                OptionalInt.empty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsApiErrorMetricsForRunPasswordCheckup() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenRunningPasswordCheckup(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.RUN_PASSWORD_CHECKUP, CredentialManagerError.API_ERROR,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForGetBreachedCredentialsCount() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulGetBreachedCredentialsCount();

        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
        checkPasswordCheckupSuccessHistogramsForOperation(
                PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForGetBreachedCredentialsCount() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenGettingBreachedCredentialsCount(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT,
                CredentialManagerError.UNCATEGORIZED, OptionalInt.empty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsApiErrorMetricsForGetBreachedCredentialsCount() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenGettingBreachedCredentialsCount(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT,
                CredentialManagerError.API_ERROR,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsMetricsWhenPasswordCheckupIntentFails() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mPasswordCheckupClientHelperMock,
                mSyncServiceMock, mModalDialogManagerSupplier);
        checkPasswordCheckupSuccessHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowsLoadingDialogOnPasswordCheckup() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).show();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogWhenPasswordCheckupIntentSent() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogOnPasswordCheckupIntentSendError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogOnPasswordCheckupIntentGetError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesNotLaunchPasswordCheckupIntentWhenLoadingDialogCancelled()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesNotLaunchPasswordCheckupIntentWhenLoadingDialogTimedOut()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testPasswordCheckupLaunchWaitsForDialogDismissability() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mPendingIntentMock, never()).send();

        mLoadingDialogCoordinatorObserver.onDismissable();
        verify(mPendingIntentMock).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowsLoadingDialogOnPasswordSettings() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).show();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogWhenPasswordSettingsIntentSent()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogOnPasswordSettingsIntentSendError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogOnPasswordSettingsIntentGetError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_ERROR);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesNotLaunchPasswordSettingsIntentWhenLoadingDialogCancelled()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesNotLaunchPasswordSettingsIntentWhenLoadingDialogTimedOut()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testPasswordSettingsLaunchWaitsForDialogDismissability() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mPendingIntentMock, never()).send();

        mLoadingDialogCoordinatorObserver.onDismissable();
        verify(mPendingIntentMock).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogNotShown() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.NOT_SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogShownDismissable()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(true);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_LOADED));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogShownNonDismissable()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(false);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM));

        mLoadingDialogCoordinatorObserver.onDismissable();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_LOADED));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogCancelled()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_CANCELLED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogCancelledDuringLoad()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM));

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.CANCELLED);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_CANCELLED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogTimeout() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_TIMED_OUT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogTimeoutDuringLoad()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM));

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.TIMED_OUT);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_TIMED_OUT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnIntentFetchError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_ERROR);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.NOT_SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnIntentSendError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mCredentialManagerLauncherMock, mSyncServiceMock, mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_CREDENTIAL_MANAGER_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.NOT_SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogNotShown() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.NOT_SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogShown() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(true);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogShownNonDismissable()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(false);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM));

        mLoadingDialogCoordinatorObserver.onDismissable();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_LOADED));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogCancelled() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_CANCELLED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogCancelledDuringLoad()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM));

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.CANCELLED);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_CANCELLED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogTimeout() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_TIMED_OUT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogTimeoutDuringLoad()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM));

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.TIMED_OUT);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.SHOWN_TIMED_OUT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnIntentFetchError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.NOT_SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnIntentSendError() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator);

        verify(mLoadingModalDialogCoordinator).dismiss();

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOADING_DIALOG_PASSWORD_CHECKUP_HISTOGRAM,
                        PasswordManagerHelper.LoadingDialogOutcome.NOT_SHOWN_LOADED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsWhenRunPasswordCheckupFails() {
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenRunningPasswordCheckup(expectedException);

        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsWhenGetBreachedCredentialsCountFails() {
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenGettingBreachedCredentialsCount(expectedException);

        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, Optional.of(TEST_EMAIL_ADDRESS),
                mock(Callback.class), mock(Callback.class));
    }

    private void chooseToSyncPasswordsWithoutCustomPassphrase() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));
    }

    private void setUpSuccessfulIntentFetchingForAccount() {
        doAnswer(invocation -> {
            Callback<PendingIntent> cb = invocation.getArgument(2);
            cb.onResult(mPendingIntentMock);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForAccount(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulCheckupIntentFetching(PendingIntent intent) {
        doAnswer(invocation -> {
            Callback<PendingIntent> cb = invocation.getArgument(2);
            cb.onResult(intent);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(anyInt(), eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForAccount(@CredentialManagerError int error) {
        doAnswer(invocation -> {
            Callback<Integer> cb = invocation.getArgument(3);
            cb.onResult(error);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForAccount(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForPasswordCheckup(Exception error) {
        doAnswer(invocation -> {
            Callback<Exception> cb = invocation.getArgument(3);
            cb.onResult(error);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(anyInt(), eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulRunPasswordCheckup() {
        doAnswer(invocation -> {
            Callback<Void> cb = invocation.getArgument(2);
            cb.onResult(null);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .runPasswordCheckupInBackground(anyInt(), eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulGetBreachedCredentialsCount() {
        doAnswer(invocation -> {
            Callback<Integer> cb = invocation.getArgument(2);
            cb.onResult(0);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .getBreachedCredentialsCount(anyInt(), eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenRunningPasswordCheckup(Exception error) {
        doAnswer(invocation -> {
            Callback<Exception> cb = invocation.getArgument(3);
            cb.onResult(error);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .runPasswordCheckupInBackground(anyInt(), eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenGettingBreachedCredentialsCount(Exception error) {
        doAnswer(invocation -> {
            Callback<Exception> cb = invocation.getArgument(3);
            cb.onResult(error);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .getBreachedCredentialsCount(anyInt(), eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class), any(Callback.class));
    }

    private void checkPasswordCheckupSuccessHistogramsForOperation(
            @PasswordCheckOperation int operation) {
        final String nameWithSuffix = PASSWORD_CHECKUP_HISTOGRAM_BASE + "."
                + getPasswordCheckupHistogramSuffixForOperation(operation);
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".Success", 1));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".Latency", 0));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffix + ".ErrorLatency"));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".Error"));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffix + ".ApiError"));
    }

    private void checkPasswordCheckupFailureHistogramsForOperation(
            @PasswordCheckOperation int operation, int errorCode, OptionalInt apiErrorCode) {
        final String nameWithSuffix = PASSWORD_CHECKUP_HISTOGRAM_BASE + "."
                + getPasswordCheckupHistogramSuffixForOperation(operation);
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".Success", 0));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffix + ".Latency"));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".ErrorLatency", 0));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".Error", errorCode));
        apiErrorCode.ifPresentOrElse(apiError
                -> Assert.assertEquals(1,
                        ShadowRecordHistogram.getHistogramValueCountForTesting(
                                nameWithSuffix + ".APIError", apiError)),
                ()
                        -> Assert.assertEquals(0,
                                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                                        nameWithSuffix + ".APIError")));
    }

    private String getPasswordCheckupHistogramSuffixForOperation(
            @PasswordCheckOperation int operation) {
        switch (operation) {
            case PasswordCheckOperation.RUN_PASSWORD_CHECKUP:
                return "RunPasswordCheckup";
            case PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT:
                return "GetBreachedCredentialsCount";
            case PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT:
                return "GetIntent";
            default:
                throw new AssertionError();
        }
    }
}
