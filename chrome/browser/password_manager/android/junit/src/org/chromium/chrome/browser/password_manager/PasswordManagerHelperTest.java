// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
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

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.content.pm.PackageInfoBuilder;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.access_loss.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadFlowController;
import org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadFlowControllerFactory;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.Set;

/**
 * Tests for password manager helper methods.
 *
 * <p>Tests related to password checkup can be found in {@link PasswordManagerCheckupHelperTest}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
@Batch(Batch.PER_CLASS)
public class PasswordManagerHelperTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private static final String TEST_NO_EMAIL_ADDRESS = null;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // TODO(crbug.com/40854050): Use fakes for CredentialManagerLauncher.
    @Mock private CredentialManagerLauncherFactory mCredentialManagerLauncherFactoryMock;
    @Mock private CredentialManagerLauncher mCredentialManagerLauncherMock;

    @Mock private Profile mProfile;

    @Mock private PrefService mPrefService;

    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Mock private SyncService mSyncServiceMock;

    @Mock private SettingsNavigation mSettingsNavigationMock;

    @Mock private PendingIntent mPendingIntentMock;

    @Mock private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    // TODO(crbug.com/40854050): Use fake instead of mock
    @Mock private PasswordManagerBackendSupportHelper mBackendSupportHelperMock;

    private ModalDialogManager mModalDialogManager;

    @Mock private LoadingModalDialogCoordinator mLoadingModalDialogCoordinator;
    private LoadingModalDialogCoordinator.Observer mLoadingDialogCoordinatorObserver;

    @Mock private CustomTabIntentHelper mCustomTabIntentHelper;

    private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    private PasswordManagerHelper mPasswordManagerHelper;

    @Before
    public void setUp() throws CredentialManagerBackendException {
        // TODO(crbug.com/40940922): Parametrise the tests for local and account.
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        mPasswordManagerHelper = new PasswordManagerHelper(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(false);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.getAuthError())
                .thenReturn(new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);
        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        doAnswer(
                        invocation -> {
                            mLoadingDialogCoordinatorObserver = invocation.getArgument(0);
                            return null;
                        })
                .when(mLoadingModalDialogCoordinator)
                .addObserver(any(LoadingModalDialogCoordinator.Observer.class));
        PasswordManagerBackendSupportHelper.setInstanceForTesting(mBackendSupportHelperMock);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.areMinUpmRequirementsMet()).thenReturn(true);

        when(mCredentialManagerLauncherFactoryMock.createLauncher())
                .thenReturn(mCredentialManagerLauncherMock);
        CredentialManagerLauncherFactory.setFactoryForTesting(
                mCredentialManagerLauncherFactoryMock);

        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigationMock);
        mSettingsCustomTabLauncher = (Context context, String url) -> {};
    }

    @Test
    public void testSyncCheckNoSyncConsent() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(false);
        assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithNoCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes()).thenReturn(Set.of(DataType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(false);
        assertTrue(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes()).thenReturn(Set.of(DataType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testCanUseUpmCheckup() {
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        assertTrue(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    public void testCanNotUseUpmCheckupWithoutPasswordType() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        assertFalse(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    public void testCanNotUseUpmCheckupWithoutSyncService() {
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(false);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        assertFalse(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    public void testCanNotUseUpmCheckupWithoutSyncConsent() {
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(false);

        assertFalse(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    public void testCanNotUseUpmCheckupWithAuthError() {
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getAuthError())
                .thenReturn(
                        new GoogleServiceAuthError(
                                GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS));

        assertFalse(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    public void testCanNotUseUpmCheckupWithNoBackend() {
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(false);

        assertFalse(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    public void testCanUseUpmCheckupWhenBackendUpdateNeeded() {
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        // TODO(crbug.com/40841269): Replace with fakes
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.areMinUpmRequirementsMet()).thenReturn(false);

        assertTrue(mPasswordManagerHelper.canUseUpm());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowsUpdateDialogOnShowPasswordSettingsWhenBackendUpdateNeeded()
            throws CredentialManagerBackendException {
        chooseToSyncPasswords();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.areMinUpmRequirementsMet()).thenReturn(false);

        when(mCredentialManagerLauncherFactoryMock.createLauncher())
                .thenThrow(
                        new CredentialManagerBackendException(
                                "", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        assertNotNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowsUpdateDialogOnShowPasswordSettingsWhenGmsCoreUpdateIsRequired() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertNotNull(dialogModel);
        assertThat(
                dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1),
                is(context.getString(R.string.password_manager_outdated_gms_dialog_description)));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testDoesNotShowUpdateDialogOnShowPasswordSettingsWhenNoUpdateNeeded() {
        chooseToSyncPasswords();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testResetsUnenrollment() {
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(true);
        mPasswordManagerHelper.resetUpmUnenrollment();

        verify(mPrefService)
                .setBoolean(
                        eq(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS), eq(false));
    }

    @Test
    public void testDoesntResetUnenrollmentIfUnnecessary() {
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);

        when(mPrefService.getBoolean(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS))
                .thenReturn(false);
        mPasswordManagerHelper.resetUpmUnenrollment();

        // If the pref isn't set, don't touch the pref!
        verify(mPrefService, never())
                .setBoolean(
                        eq(Pref.UNENROLLED_FROM_GOOGLE_MOBILE_SERVICES_DUE_TO_ERRORS),
                        anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowPasswordSettingsSyncingPasswordsLaunchesNewUiForAccount() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        chooseToSyncPasswords();

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS),
                        any(Callback.class),
                        any(Callback.class));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowPasswordSettingsSyncingUserNotSyncingPasswordsLaunchesOldUi() {
        chooseToSyncButNotSyncPasswords();
        Context mockContext = mock(Context.class);
        // Set the adequate PasswordManagerUtilBridge response for shouldUseUpmWiring for a syncing
        // user who isn't syncing passwords and isn't eligible to use UPM for local.
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(false);

        mPasswordManagerHelper.showPasswordSettings(
                mockContext,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mockContext).startActivity(any());
        verify(mSettingsNavigationMock)
                .createSettingsIntent(
                        eq(mockContext), eq(SettingsFragment.PASSWORDS), any(Bundle.class));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowPasswordSettingsNotSyncingPasswordsCanNotUseUPMLaunchesOldUi() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        Context mockContext = mock(Context.class);

        mPasswordManagerHelper.showPasswordSettings(
                mockContext,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mockContext).startActivity(any());
        verify(mSettingsNavigationMock)
                .createSettingsIntent(
                        eq(mockContext), eq(SettingsFragment.PASSWORDS), any(Bundle.class));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowPasswordSettingsNotSyncingPasswordsCanUseUPMLaunchesNewUiForLocal() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mCredentialManagerLauncherMock)
                .getLocalCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class),
                        any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsSuccessMetricsForAccountIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1)
                        .expectNoRecords(PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM)
                        .expectIntRecord(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                1)
                        .build();
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    // This test is similar to its account counterpart. To ensure even coverage, but avoid
    // adding even more tests to this suite this will run with LOGIN_DB_DEPRECATION_ANDROID
    // disabled.
    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsSuccessMetricsForLocalIntent() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_LATENCY_HISTOGRAM, 0)
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 1)
                        .expectNoRecords(PasswordMetricsUtil.LOCAL_GET_INTENT_ERROR_HISTOGRAM)
                        .expectIntRecord(
                                PasswordMetricsUtil
                                        .LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                1)
                        .build();
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
        setUpSuccessfulIntentFetchingForLocal();

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsErrorMetricsForAccountIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.UNCATEGORIZED)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectNoRecords(PasswordMetricsUtil.ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.UNCATEGORIZED);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    // This test is similar to its local counterpart. To ensure even coverage, but avoid
    // adding even more tests to this suite this will run with LOGIN_DB_DEPRECATION_ANDROID
    // disabled.
    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsErrorMetricsForLocalIntent() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.LOCAL_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.UNCATEGORIZED)
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectNoRecords(PasswordMetricsUtil.LOCAL_GET_INTENT_LATENCY_HISTOGRAM)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
        returnErrorWhenFetchingIntentForLocal(CredentialManagerError.UNCATEGORIZED);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsMetricsWhenAccountIntentFails() throws CanceledException {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1)
                        .expectNoRecords(PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM)
                        .expectIntRecord(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                0)
                        .build();
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    // This test is similar to its local counterpart. To ensure even coverage, but avoid
    // adding even more tests to this suite this will run with LOGIN_DB_DEPRECATION_ANDROID
    // disabled.
    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsMetricsWhenLocalIntentFails() throws CanceledException {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_LATENCY_HISTOGRAM, 0)
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 1)
                        .expectNoRecords(PasswordMetricsUtil.LOCAL_GET_INTENT_ERROR_HISTOGRAM)
                        .expectIntRecord(
                                PasswordMetricsUtil
                                        .LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                0)
                        .build();
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
        setUpSuccessfulIntentFetchingForLocal();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testShowsLoadingDialogOnPasswordSettings() throws CanceledException {
        chooseToSyncPasswords();

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).show();
    }

    @Test
    public void testDismissesLoadingDialogWhenPasswordSettingsIntentSent()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testDismissesLoadingDialogOnPasswordSettingsIntentSendError()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testDismissesLoadingDialogOnPasswordSettingsIntentGetError()
            throws CanceledException {
        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_EXCEPTION);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testDoesNotLaunchPasswordSettingsIntentWhenLoadingDialogCancelled()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    public void testDoesNotLaunchPasswordSettingsIntentWhenLoadingDialogTimedOut()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    public void testPasswordSettingsLaunchWaitsForDialogDismissability() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mPendingIntentMock, never()).send();

        mLoadingDialogCoordinatorObserver.onDismissable();
        verify(mPendingIntentMock).send();
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogNotShown() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogShownDismissable()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(true);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogShownNonDismissable()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(false);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        mLoadingDialogCoordinatorObserver.onDismissable();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogCancelled()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogCancelledDuringLoad()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.CANCELLED);
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogTimeout() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnDialogTimeoutDuringLoad()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.TIMED_OUT);
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnIntentFetchError()
            throws CanceledException {
        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_EXCEPTION);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsSettingsLoadingDialogMetricsOnIntentSendError()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        mPasswordManagerHelper.launchTheCredentialManager(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mSyncServiceMock,
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                TEST_EMAIL_ADDRESS);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsApiErrorWhenFetchingAccountCredentialManagerIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();

        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.API_EXCEPTION)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM,
                                CommonStatusCodes.INTERNAL_ERROR)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        chooseToSyncPasswords();
        ApiException returnedException =
                new ApiException(new Status(CommonStatusCodes.INTERNAL_ERROR));
        returnExceptionWhenFetchingIntentForAccount(returnedException);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsOtherApiErrorWhenFetchingAccountCredentialManagerIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.OTHER_API_ERROR)
                        .expectNoRecords(PasswordMetricsUtil.ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        chooseToSyncPasswords();
        NullPointerException returnedException = new NullPointerException();
        returnExceptionWhenFetchingIntentForAccount(returnedException);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    // This test is similar to its account counterpart. To ensure even coverage, but avoid
    // adding even more tests to this suite this will run with LOGIN_DB_DEPRECATION_ANDROID
    // disabled.
    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsApiErrorWhenFetchingLocalCredentialManagerIntent() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.LOCAL_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.API_EXCEPTION)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        ApiException returnedException =
                new ApiException(new Status(CommonStatusCodes.INTERNAL_ERROR));
        returnExceptionWhenFetchingIntentForLocal(returnedException);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    // This test is similar to its local counterpart. To ensure even coverage, but avoid
    // adding even more tests to this suite this will run with LOGIN_DB_DEPRECATION_ANDROID
    // disabled.
    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsConnectionResultWhenFetchingAccountCredentialManagerIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.API_EXCEPTION)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_API_ERROR_HISTOGRAM,
                                CommonStatusCodes.API_NOT_CONNECTED)
                        .expectIntRecord(
                                PasswordMetricsUtil
                                        .ACCOUNT_GET_INTENT_ERROR_CONNECTION_RESULT_CODE_HISTOGRAM,
                                ConnectionResult.API_UNAVAILABLE)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();

        chooseToSyncPasswords();
        ApiException returnedException =
                new ApiException(
                        new Status(new ConnectionResult(ConnectionResult.API_UNAVAILABLE), ""));
        returnExceptionWhenFetchingIntentForAccount(returnedException);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testRecordsConnectionResultWhenFetchingLocalCredentialManagerIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(PasswordMetricsUtil.LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectIntRecord(
                                PasswordMetricsUtil.LOCAL_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.API_EXCEPTION)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        ApiException returnedException =
                new ApiException(
                        new Status(new ConnectionResult(ConnectionResult.API_UNAVAILABLE), ""));
        returnExceptionWhenFetchingIntentForLocal(returnedException);

        mPasswordManagerHelper.showPasswordSettings(
                ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testUseAccountSettings() {
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(false);

        assertTrue(PasswordManagerHelper.canUseAccountSettings());
    }

    @Test
    public void testCannotUseAccountSettingsWithNoBackend() {
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(false);

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(false);

        assertFalse(PasswordManagerHelper.canUseAccountSettings());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testPasswordAccessLossDialogNoUpm() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NO_UPM);

        // PasswordAccessLossDialog requires an Activity Context.
        try (ActivityScenario<Activity> scenario = ActivityScenario.launch(Activity.class)) {
            scenario.onActivity(
                    activity -> {
                        mPasswordManagerHelper.showPasswordSettings(
                                activity,
                                ManagePasswordsReferrer.CHROME_SETTINGS,
                                mModalDialogManagerSupplier,
                                /* managePasskeys= */ false,
                                TEST_NO_EMAIL_ADDRESS,
                                mSettingsCustomTabLauncher);
                    });
        }

        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertEquals(
                context.getString(R.string.access_loss_update_gms_title),
                ((TextView) customView.findViewById(R.id.title)).getText());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testPasswordAccessLossDialogNotDisplayed() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NONE);
        chooseToSyncPasswords();
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);

        // PasswordAccessLossDialog requires an Activity Context.
        try (ActivityScenario<Activity> scenario = ActivityScenario.launch(Activity.class)) {
            scenario.onActivity(
                    activity -> {
                        mPasswordManagerHelper.showPasswordSettings(
                                activity,
                                ManagePasswordsReferrer.CHROME_SETTINGS,
                                mModalDialogManagerSupplier,
                                /* managePasskeys= */ false,
                                TEST_NO_EMAIL_ADDRESS,
                                mSettingsCustomTabLauncher);
                    });
        }

        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowDownloadCsvDialogIfCsvIsPresentAndPwmNotAvailable() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(true);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        setUpUpdatableGmsCore(testActivity);

        PasswordCsvDownloadFlowController mockController =
                mock(PasswordCsvDownloadFlowController.class);
        PasswordCsvDownloadFlowControllerFactory.setControllerForTesting(mockController);
        mPasswordManagerHelper.showPasswordSettings(
                testActivity,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mockController)
                .showDialogAndStartFlow(
                        eq(testActivity),
                        eq(mProfile),
                        /* isGooglePlayServicesAvailable= */ eq(true),
                        /* isPasswordManagerAvailable= */ eq(false),
                        eq(mSettingsCustomTabLauncher));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowDownloadCsvDialogIfCsvIsPresentAndPwmAvailable() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(true);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        setUpUpdatableGmsCore(testActivity);

        PasswordCsvDownloadFlowController mockController =
                mock(PasswordCsvDownloadFlowController.class);
        PasswordCsvDownloadFlowControllerFactory.setControllerForTesting(mockController);
        mPasswordManagerHelper.showPasswordSettings(
                testActivity,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mockController)
                .showDialogAndStartFlow(
                        eq(testActivity),
                        eq(mProfile),
                        /* isGooglePlayServicesAvailable= */ eq(true),
                        /* isPasswordManagerAvailable= */ eq(true),
                        eq(mSettingsCustomTabLauncher));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowDownloadCsvDialogIfCsvIsPresentAndNoGms() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(true);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();

        PasswordCsvDownloadFlowController mockController =
                mock(PasswordCsvDownloadFlowController.class);
        PasswordCsvDownloadFlowControllerFactory.setControllerForTesting(mockController);
        mPasswordManagerHelper.showPasswordSettings(
                testActivity,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mockController)
                .showDialogAndStartFlow(
                        eq(testActivity),
                        eq(mProfile),
                        /* isGooglePlayServicesAvailable= */ eq(false),
                        /* isPasswordManagerAvailable= */ eq(false),
                        eq(mSettingsCustomTabLauncher));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowPwmUnavailableDialogNoCsvNoGms() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();

        mPasswordManagerHelper.showPasswordSettings(
                testActivity,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        assertEquals(
                testActivity.getResources().getString(R.string.pwm_disabled_no_gms_dialog_title),
                dialogModel.get(ModalDialogProperties.TITLE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testShowPwmUnavailableDialogNoCsvUpdatableGms() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        setUpUpdatableGmsCore(testActivity);
        mPasswordManagerHelper.showPasswordSettings(
                testActivity,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        assertEquals(
                testActivity.getResources().getString(R.string.access_loss_update_gms_title),
                dialogModel.get(ModalDialogProperties.TITLE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testDoesntShowDialogIfExportNotFinished() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(false);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        setUpUpdatableGmsCore(testActivity);
        mPasswordManagerHelper.showPasswordSettings(
                testActivity,
                ManagePasswordsReferrer.CHROME_SETTINGS,
                mModalDialogManagerSupplier,
                /* managePasskeys= */ false,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        // Check that the unavailability dialog is not shown.
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNull(dialogModel);

        // Check that the download CSV dialog is not shown.
        PasswordCsvDownloadFlowController mockController =
                mock(PasswordCsvDownloadFlowController.class);
        PasswordCsvDownloadFlowControllerFactory.setControllerForTesting(mockController);
        verify(mockController, never())
                .showDialogAndStartFlow(
                        any(), any(), anyBoolean(), anyBoolean(), eq(mSettingsCustomTabLauncher));

        // Check that the management UI is not shown (the pwm is unavailable).
        verify(mCredentialManagerLauncherMock, never())
                .getLocalCredentialManagerIntent(anyInt(), any(), any());
    }

    private void setUpPwmAvailableWithoutUnmigratedPasswords() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);
    }

    private void setUpUpdatableGmsCore(Context context) {
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(context.getPackageManager());
        PackageInfo gmsPackageInfo =
                PackageInfoBuilder.newBuilder().setPackageName("com.google.android.gms").build();
        shadowPackageManager.installPackage(gmsPackageInfo);

        PackageInfo playStorePackageInfo =
                PackageInfoBuilder.newBuilder().setPackageName("com.android.vending").build();
        shadowPackageManager.installPackage(playStorePackageInfo);
    }

    private void chooseToSyncPasswords() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncServiceMock.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                TEST_EMAIL_ADDRESS, new GaiaId("0")));
        // Set the adequate PasswordManagerUtilBridge response for shouldUseUpmWiring for a syncing
        // user.
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
    }

    private void chooseToSyncButNotSyncPasswords() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getSelectedTypes()).thenReturn(new HashSet<>());
        when(mSyncServiceMock.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                TEST_EMAIL_ADDRESS, new GaiaId("0")));
    }

    private void setUpSuccessfulIntentFetchingForAccount() {
        doAnswer(
                        invocation -> {
                            Callback<PendingIntent> cb = invocation.getArgument(2);
                            cb.onResult(mPendingIntentMock);
                            return true;
                        })
                .when(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void setUpSuccessfulIntentFetchingForLocal() {
        doAnswer(
                        invocation -> {
                            Callback<PendingIntent> cb = invocation.getArgument(1);
                            cb.onResult(mPendingIntentMock);
                            return true;
                        })
                .when(mCredentialManagerLauncherMock)
                .getLocalCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForAccount(@CredentialManagerError int error) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(new CredentialManagerBackendException("", error));
                            return true;
                        })
                .when(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForLocal(@CredentialManagerError int error) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(2);
                            cb.onResult(new CredentialManagerBackendException("", error));
                            return true;
                        })
                .when(mCredentialManagerLauncherMock)
                .getLocalCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void returnExceptionWhenFetchingIntentForAccount(Exception exception) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(exception);
                            return true;
                        })
                .when(mCredentialManagerLauncherMock)
                .getAccountCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void returnExceptionWhenFetchingIntentForLocal(Exception exception) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(2);
                            cb.onResult(exception);
                            return true;
                        })
                .when(mCredentialManagerLauncherMock)
                .getLocalCredentialManagerIntent(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class),
                        any(Callback.class));
    }
}
