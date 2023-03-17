// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

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
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.Optional;
import java.util.OptionalInt;

/** Tests for password manager helper methods. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowSystemClock.class})
@Batch(Batch.PER_CLASS)
public class PasswordManagerHelperTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    private static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    private static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    private static final String ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError";
    private static final String ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.APIError.ConnectionResultCode";
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

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    // TODO(crbug.com/1346235): Use fakes for CredentialManagerLauncher, PasswordCheckupClientHelper
    // and corresponding factories
    @Mock
    private PasswordCheckupClientHelperFactory mPasswordCheckupClientHelperFactoryMock;
    @Mock
    private CredentialManagerLauncherFactory mCredentialManagerLauncherFactoryMock;
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

    // TODO(crbug.com/1346235): Use fake instead of mock
    @Mock
    private PasswordManagerBackendSupportHelper mBackendSupportHelperMock;

    private ModalDialogManager mModalDialogManager;

    @Mock
    LoadingModalDialogCoordinator mLoadingModalDialogCoordinator;

    private LoadingModalDialogCoordinator.Observer mLoadingDialogCoordinatorObserver;

    @Before
    public void setUp() throws PasswordCheckBackendException, CredentialManagerBackendException {
        UmaRecorderHolder.resetForTesting();
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
        PasswordManagerBackendSupportHelper.setInstanceForTesting(mBackendSupportHelperMock);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(false);

        when(mPasswordCheckupClientHelperFactoryMock.createHelper())
                .thenReturn(mPasswordCheckupClientHelperMock);
        PasswordCheckupClientHelperFactory.setFactoryForTesting(
                mPasswordCheckupClientHelperFactoryMock);

        when(mCredentialManagerLauncherFactoryMock.createLauncher())
                .thenReturn(mCredentialManagerLauncherMock);
        CredentialManagerLauncherFactory.setFactoryForTesting(
                mCredentialManagerLauncherFactoryMock);
    }

    @Test
    public void testSyncCheckFeatureNotEnabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        assertFalse(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testSyncCheckNoSyncConsent() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(false);
        assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsDisabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Collections.EMPTY_SET);
        assertFalse(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsEnabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        assertTrue(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
    }

    @Test
    public void testSyncEnabledWithCustomPassphrase() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        assertTrue(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithNoCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(false);
        assertTrue(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanUseUpmCheckup() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        assertTrue(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanNotUseUpmCheckupWithoutPasswordType() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        assertFalse(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanNotUseUpmCheckupWithoutSyncService() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(false);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        assertFalse(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanNotUseUpmCheckupWithoutSyncConsent() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(false);

        assertFalse(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanNotUseUpmCheckupWithAuthError() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getAuthError()).thenReturn(State.INVALID_GAIA_CREDENTIALS);

        assertFalse(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanNotUseUpmCheckupWithNoBackend() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(false);

        assertFalse(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testCanUseUpmCheckupWhenBackendUpdateNeeded() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        // TODO(crbug.com/1327578): Replace with fakes
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(true);

        assertTrue(PasswordManagerHelper.canUseUpm());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowsUpdateDialogOnShowPasswordSettingsWhenBackendUpdateNeeded()
            throws CredentialManagerBackendException {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(true);

        when(mCredentialManagerLauncherFactoryMock.createLauncher())
                .thenThrow(new CredentialManagerBackendException(
                        "", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        assertNotNull(mModalDialogManager.getCurrentDialogForTest());

        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowsUpdateDialogOnShowPasswordCheckupWhenBackendUpdateNeeded()
            throws PasswordCheckBackendException {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(true);

        when(mPasswordCheckupClientHelperFactoryMock.createHelper())
                .thenThrow(new PasswordCheckBackendException(
                        "", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);

        assertNotNull(mModalDialogManager.getCurrentDialogForTest());

        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesNotShowUpdateDialogOnShowPasswordSettingsWhenNoUpdateNeeded() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(false);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        assertNull(mModalDialogManager.getCurrentDialogForTest());

        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesNotShowUpdateDialogOnShowPasswordCheckupWhenNoUpdateNeeded() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mBackendSupportHelperMock.isUpdateNeeded()).thenReturn(false);

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);

        assertNull(mModalDialogManager.getCurrentDialogForTest());

        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testSendsIntentOnLaunchGmsUpdate() {
        Context mockContext = mock(Context.class);

        PasswordManagerHelper.launchGmsUpdate(mockContext);

        doAnswer(invocation -> {
            Intent intent = invocation.getArgument(0);
            assertEquals(intent.getAction(), Intent.ACTION_VIEW);
            assertEquals(intent.getPackage(), "com.android.vending");
            assertEquals(intent.getBooleanExtra("overlay", false), true);
            assertEquals(intent.getStringExtra("callerId"), mockContext.getPackageName());
            assertEquals(intent.getData(),
                    Uri.parse("market://details?id="
                            + GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE
                            + "&referrer=chrome_upm"));
            return null;
        })
                .when(mockContext)
                .startActivity(any(Intent.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testResetsUnenrollment() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(true);
        PasswordManagerHelper.resetUpmUnenrollment();

        verify(mPrefService)
                .setBoolean(
                        eq(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS), eq(false));

        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesntResetUnenrollmentIfUnnecessary() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(false);
        PasswordManagerHelper.resetUpmUnenrollment();

        // If the pref isn't set, don't touch the pref!
        verify(mPrefService, never())
                .setBoolean(eq(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS),
                        anyBoolean());

        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesCredentialManagerSync() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        verify(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowPasswordSettingsNoSyncLaunchesOldUI() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        Context mockContext = mock(Context.class);

        PasswordManagerHelper.showPasswordSettings(mockContext,
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        verify(mockContext).startActivity(any());
        verify(mSettingsLauncherMock)
                .createSettingsActivityIntent(
                        eq(mockContext), eq(PasswordSettings.class.getName()), any(Bundle.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForAccountIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForAccountIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.UNCATEGORIZED);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM, CredentialManagerError.UNCATEGORIZED));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM));
        assertEquals(0,
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
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesPasswordCheckupSync() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.of(TEST_EMAIL_ADDRESS)), any(Callback.class),
                        any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testPasswordCheckupIntentCalledIfSuccess() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);
        verify(mPendingIntentMock).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForPasswordCheckupIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);

        PasswordManagerHelper.showPasswordCheckup(ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);
        checkPasswordCheckupSuccessHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        assertEquals(1,
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
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);

        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                CredentialManagerError.UNCATEGORIZED, OptionalInt.empty());
        assertEquals(0,
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
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);
        checkPasswordCheckupFailureHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                CredentialManagerError.API_ERROR,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));

        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForRunPasswordCheckup() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulRunPasswordCheckup();

        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
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
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
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
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
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
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
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
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
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
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
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
                PasswordCheckReferrer.SAFETY_CHECK, mSyncServiceMock, mModalDialogManagerSupplier);
        checkPasswordCheckupSuccessHistogramsForOperation(
                PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowsLoadingDialogOnPasswordCheckup() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).show();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogWhenPasswordCheckupIntentSent() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        verify(mPendingIntentMock, never()).send();

        mLoadingDialogCoordinatorObserver.onDismissable();
        verify(mPendingIntentMock).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testShowsLoadingDialogOnPasswordSettings() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).show();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogWhenPasswordSettingsIntentSent()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDismissesLoadingDialogOnPasswordSettingsIntentGetError()
            throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_ERROR);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        mLoadingDialogCoordinatorObserver.onDismissable();

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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());
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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.CANCELLED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSettingsLoadingDialogMetricsOnDialogTimeout() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        PasswordManagerHelper.launchTheCredentialManager(ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());
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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.TIMED_OUT);
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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
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
                mSyncServiceMock, mLoadingModalDialogCoordinator, mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogNotShown() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        mLoadingDialogCoordinatorObserver.onDismissable();

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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());
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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.CANCELLED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsCheckupLoadingDialogMetricsOnDialogTimeout() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        PasswordManagerHelper.launchPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());
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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.TIMED_OUT);
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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
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
                Optional.of(TEST_EMAIL_ADDRESS), mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier, ContextUtils.getApplicationContext());

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsWhenRunPasswordCheckupFails() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenRunningPasswordCheckup(expectedException);

        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsWhenGetBreachedCredentialsCountFails() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenGettingBreachedCredentialsCount(expectedException);

        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS), mock(Callback.class), mock(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsApiErrorWhenFetchingCredentialManagerIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        ApiException returnedException =
                new ApiException(new Status(CommonStatusCodes.INTERNAL_ERROR));
        returnApiExceptionWhenFetchingIntentForAccount(returnedException);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM, CredentialManagerError.API_ERROR));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM, CommonStatusCodes.INTERNAL_ERROR));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsConnectionResultWhenFetchingCredentialManagerIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        ApiException returnedException = new ApiException(
                new Status(new ConnectionResult(ConnectionResult.API_UNAVAILABLE), ""));
        returnApiExceptionWhenFetchingIntentForAccount(returnedException);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock, mSyncServiceMock,
                mModalDialogManagerSupplier, /*managePasskeys=*/false);

        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM, CredentialManagerError.API_ERROR));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM,
                        CommonStatusCodes.API_NOT_CONNECTED));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM,
                        ConnectionResult.API_UNAVAILABLE));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.PASSKEY_MANAGEMENT_USING_ACCOUNT_SETTINGS_ANDROID})
    public void
    testUseAccountSettings() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(false);

        assertTrue(PasswordManagerHelper.canUseAccountSettings());
        SyncService.resetForTests();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.PASSKEY_MANAGEMENT_USING_ACCOUNT_SETTINGS_ANDROID})
    public void
    testCannotUseAccountSettingsWithNoBackend() {
        SyncService.overrideForTests(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(false);

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(false);

        assertFalse(PasswordManagerHelper.canUseAccountSettings());
        SyncService.resetForTests();
    }

    private void chooseToSyncPasswordsWithoutCustomPassphrase() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
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
                .getAccountCredentialManagerIntent(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
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
            Callback<Exception> cb = invocation.getArgument(3);
            cb.onResult(new CredentialManagerBackendException("", error));
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnApiExceptionWhenFetchingIntentForAccount(ApiException exception) {
        doAnswer(invocation -> {
            Callback<Exception> cb = invocation.getArgument(3);
            cb.onResult(exception);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
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
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffix + ".Success", 1));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffix + ".Latency", 0));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".ErrorLatency"));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".Error"));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".ApiError"));
    }

    private void checkPasswordCheckupFailureHistogramsForOperation(
            @PasswordCheckOperation int operation, int errorCode, OptionalInt apiErrorCode) {
        final String nameWithSuffix = PASSWORD_CHECKUP_HISTOGRAM_BASE + "."
                + getPasswordCheckupHistogramSuffixForOperation(operation);
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffix + ".Success", 0));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".Latency"));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".ErrorLatency", 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".Error", errorCode));
        apiErrorCode.ifPresentOrElse(apiError
                -> assertEquals(1,
                        RecordHistogram.getHistogramValueCountForTesting(
                                nameWithSuffix + ".APIError", apiError)),
                ()
                        -> assertEquals(0,
                                RecordHistogram.getHistogramTotalCountForTesting(
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
