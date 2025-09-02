// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
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
import android.content.pm.PackageInfo;

import androidx.fragment.app.FragmentActivity;
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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadFlowController;
import org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadFlowControllerFactory;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.test.BrowserUiTestFragmentActivity;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

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

    private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    private PasswordManagerHelper mPasswordManagerHelper;

    @Before
    public void setUp() throws CredentialManagerBackendException {
        // TODO(crbug.com/40940922): Parametrise the tests for local and account.
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(anyBoolean()))
                .thenReturn(true);
        mPasswordManagerHelper = new PasswordManagerHelper(mProfile);
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

        when(mCredentialManagerLauncherFactoryMock.createLauncher())
                .thenReturn(mCredentialManagerLauncherMock);
        CredentialManagerLauncherFactory.setFactoryForTesting(
                mCredentialManagerLauncherFactoryMock);

        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigationMock);
        mSettingsCustomTabLauncher = (Context context, String url) -> {};
    }

    @Test
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

    @Test
    public void testRecordsErrorMetricsForAccountIntent() {
        setUpPwmAvailableWithoutUnmigratedPasswords();
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_ERROR_HISTOGRAM,
                                CredentialManagerError.NO_ACCOUNT_NAME)
                        .expectIntRecord(
                                PasswordMetricsUtil.ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0)
                        .expectNoRecords(PasswordMetricsUtil.ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM)
                        .expectNoRecords(
                                PasswordMetricsUtil
                                        .ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.NO_ACCOUNT_NAME);

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

    @Test
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
    public void testShowDownloadCsvDialogIfCsvIsPresentAndPwmNotAvailable() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(false);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(true);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class).setup().get();
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
    public void testShowDownloadCsvDialogIfCsvIsPresentAndPwmAvailable() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(true);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class).setup().get();
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
    public void testShowDownloadCsvDialogIfCsvIsPresentAndNoGms() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(false);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(true);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class).setup().get();

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
    public void testShowPwmUnavailableDialogNoCsvNoGms() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(false);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class).setup().get();

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
    public void testShowPwmUnavailableDialogNoCsvUpdatableGms() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(false);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class).setup().get();
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

    private void setUpPwmAvailableWithoutUnmigratedPasswords() {
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(true);
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
