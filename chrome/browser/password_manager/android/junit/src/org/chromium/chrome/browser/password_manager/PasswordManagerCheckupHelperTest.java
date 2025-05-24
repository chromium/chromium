// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
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
import android.content.pm.PackageInfo;

import androidx.fragment.app.FragmentActivity;
import androidx.test.core.content.pm.PackageInfoBuilder;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.loading_modal.LoadingModalDialogCoordinator;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadFlowController;
import org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadFlowControllerFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collection;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.Set;

/** Tests for the password checkup-related methods in {@link PasswordManagerHelper}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
@Batch(Batch.PER_CLASS)
public class PasswordManagerCheckupHelperTest {
    @ParameterizedRobolectricTestRunner.Parameters
    public static Collection testCases() {
        return Arrays.asList(
                /* isLoginDbDeprecationEnabled= */ true, /* isLoginDbDeprecationEnabled= */ false);
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private static final String TEST_NO_EMAIL_ADDRESS = null;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @ParameterizedRobolectricTestRunner.Parameter public boolean mIsLoginDbDeprecationEnabled;

    // TODO(crbug.com/40854050): Use a fake for PasswordCheckupClientHelper.
    @Mock private PasswordCheckupClientHelperFactory mPasswordCheckupClientHelperFactoryMock;
    @Mock private PasswordCheckupClientHelper mPasswordCheckupClientHelperMock;

    @Mock private Profile mProfile;

    @Mock private PrefService mPrefService;

    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Mock private SyncService mSyncServiceMock;

    @Mock private PendingIntent mPendingIntentMock;

    @Mock private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    // TODO(crbug.com/40854050): Use fake instead of mock
    @Mock private PasswordManagerBackendSupportHelper mBackendSupportHelperMock;

    private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    private ModalDialogManager mModalDialogManager;

    @Mock private LoadingModalDialogCoordinator mLoadingModalDialogCoordinator;
    private LoadingModalDialogCoordinator.Observer mLoadingDialogCoordinatorObserver;

    private PasswordManagerHelper mPasswordManagerHelper;

    @Before
    public void setUp() throws PasswordCheckBackendException {
        if (mIsLoginDbDeprecationEnabled) {
            FeatureOverrides.enable(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID);
        } else {
            FeatureOverrides.disable(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID);
        }
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        mPasswordManagerHelper = new PasswordManagerHelper(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
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
        if (mIsLoginDbDeprecationEnabled) {
            when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                            eq(mPrefService), eq(true)))
                    .thenReturn(true);
        } else {
            when(mPasswordManagerUtilBridgeJniMock.areMinUpmRequirementsMet()).thenReturn(true);
        }
        when(mPasswordCheckupClientHelperFactoryMock.createHelper())
                .thenReturn(mPasswordCheckupClientHelperMock);
        PasswordCheckupClientHelperFactory.setFactoryForTesting(
                mPasswordCheckupClientHelperFactoryMock);
        mSettingsCustomTabLauncher = (Context context, String url) -> {};
    }

    @Test
    public void testThrowsPasswordManagerNotAvailableException() {
        // This test only applies if the login DB deprecation is enabled.
        assumeTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID));
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        chooseToSyncPasswords();
        setUpSuccessfulRunPasswordCheckup();

        Callback<Exception> failureCallback = mock(Callback.class);
        mPasswordManagerHelper.runPasswordCheckupInBackground(
                org.chromium.chrome.browser.password_manager.PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                failureCallback);
        final ArgumentCaptor<PasswordCheckBackendException> captor =
                ArgumentCaptor.forClass(PasswordCheckBackendException.class);

        verify(failureCallback).onResult(captor.capture());
        assertEquals(
                CredentialManagerError.PASSWORD_MANAGER_NOT_AVAILABLE, captor.getValue().errorCode);
    }

    @Test
    public void testShowsUpdateDialogOnShowPasswordCheckupForAccountWhenBackendUpdateNeeded()
            throws PasswordCheckBackendException {
        chooseToSyncPasswords();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.areMinUpmRequirementsMet()).thenReturn(false);

        when(mPasswordCheckupClientHelperFactoryMock.createHelper())
                .thenThrow(
                        new PasswordCheckBackendException(
                                "", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        assertNotNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testShowsUpdateDialogOnShowPasswordCheckupForLocalWhenBackendUpdateNeeded()
            throws PasswordCheckBackendException {
        chooseToSyncPasswords();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.areMinUpmRequirementsMet()).thenReturn(false);

        when(mPasswordCheckupClientHelperFactoryMock.createHelper())
                .thenThrow(
                        new PasswordCheckBackendException(
                                "", CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED));

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        assertNotNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testDoesNotShowUpdateDialogOnShowPasswordCheckupForAccountWhenNoUpdateNeeded() {
        chooseToSyncPasswords();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testDoesNotShowUpdateDialogOnShowPasswordCheckupForLocalWhenNoUpdateNeeded() {
        chooseToSyncPasswords();

        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        assertNull(mModalDialogManager.getCurrentDialogForTest());
    }

    @Test
    public void testRetrievesIntentForAccountCheckup() {
        chooseToSyncPasswords();

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(
                        eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.of(TEST_EMAIL_ADDRESS)),
                        any(Callback.class),
                        any(Callback.class));
    }

    @Test
    public void testRetrievesIntentForLocalCheckup() {
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(
                        eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.empty()),
                        any(Callback.class),
                        any(Callback.class));
    }

    @Test
    public void testPasswordCheckupIntentForAccountCalledIfSuccess() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);
        verify(mPendingIntentMock).send();
    }

    @Test
    public void testPasswordCheckupIntentForLocalCalledIfSuccess() throws CanceledException {
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);

        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_NO_EMAIL_ADDRESS);
        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);
        verify(mPendingIntentMock).send();
    }

    @Test
    public void testRecordsSuccessMetricsForPasswordCheckupIntentForAccount() {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        HistogramWatcher histogram =
                builder.expectIntRecord(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                1)
                        .build();

        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testRecordsSuccessMetricsForPasswordCheckupIntentForLocal() {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        HistogramWatcher histogram =
                builder.expectIntRecord(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                1)
                        .build();
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_NO_EMAIL_ADDRESS);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testRecordsErrorMetricsForPasswordCheckupIntentForAccount() {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                        CredentialManagerError.UNCATEGORIZED,
                        OptionalInt.empty());
        HistogramWatcher histogram =
                builder.expectNoRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();

        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED),
                TEST_EMAIL_ADDRESS);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testRecordsErrorMetricsForPasswordCheckupIntentForLocal() {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                        CredentialManagerError.UNCATEGORIZED,
                        OptionalInt.empty());
        HistogramWatcher histogram =
                builder.expectNoRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();

        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED),
                TEST_NO_EMAIL_ADDRESS);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testRecordsApiErrorMetricsForPasswordCheckupIntentForAccount() {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                        CredentialManagerError.API_EXCEPTION,
                        OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
        HistogramWatcher histogram =
                builder.expectNoRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();

        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)),
                TEST_EMAIL_ADDRESS);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testRecordsApiErrorMetricsForPasswordCheckupIntentForLocal() {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT,
                        CredentialManagerError.API_EXCEPTION,
                        OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
        HistogramWatcher histogram =
                builder.expectNoRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM)
                        .build();
        when(mPasswordManagerUtilBridgeJniMock.shouldUseUpmWiring(mSyncServiceMock, mPrefService))
                .thenReturn(true);
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)),
                TEST_NO_EMAIL_ADDRESS);

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testRecordsSuccessMetricsForRunPasswordCheckup() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                                PasswordCheckOperation.RUN_PASSWORD_CHECKUP)
                        .build();
        chooseToSyncPasswords();
        setUpSuccessfulRunPasswordCheckup();

        mPasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsErrorMetricsForRunPasswordCheckup() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.RUN_PASSWORD_CHECKUP,
                                CredentialManagerError.UNCATEGORIZED,
                                OptionalInt.empty())
                        .build();

        chooseToSyncPasswords();
        returnErrorWhenRunningPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        mPasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsApiErrorMetricsForRunPasswordCheckup() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.RUN_PASSWORD_CHECKUP,
                                CredentialManagerError.API_EXCEPTION,
                                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR))
                        .build();

        chooseToSyncPasswords();
        returnErrorWhenRunningPasswordCheckup(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        mPasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsSuccessMetricsForGetBreachedCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                                PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT)
                        .build();

        chooseToSyncPasswords();
        setUpSuccessfulGetBreachedCredentialsCount();

        mPasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsSuccessMetricsForGetWeakCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                                PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT)
                        .build();

        chooseToSyncPasswords();
        setUpSuccessfulGetWeakCredentialsCount();

        mPasswordManagerHelper.getWeakCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsSuccessMetricsForGetReusedCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                                PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT)
                        .build();

        chooseToSyncPasswords();
        setUpSuccessfulGetReusedCredentialsCount();

        mPasswordManagerHelper.getReusedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsErrorMetricsForGetBreachedCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT,
                                CredentialManagerError.UNCATEGORIZED,
                                OptionalInt.empty())
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenGettingBreachedCredentialsCount(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        mPasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsErrorMetricsForGetWeakCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT,
                                CredentialManagerError.UNCATEGORIZED,
                                OptionalInt.empty())
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenGettingWeakCredentialsCount(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        mPasswordManagerHelper.getWeakCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsErrorMetricsForGetReusedCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT,
                                CredentialManagerError.UNCATEGORIZED,
                                OptionalInt.empty())
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenGettingReusedCredentialsCount(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED));

        mPasswordManagerHelper.getReusedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsApiErrorMetricsForGetBreachedCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT,
                                CredentialManagerError.API_EXCEPTION,
                                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR))
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenGettingBreachedCredentialsCount(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        mPasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsApiErrorMetricsForGetWeakCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT,
                                CredentialManagerError.API_EXCEPTION,
                                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR))
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenGettingWeakCredentialsCount(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        mPasswordManagerHelper.getWeakCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsApiErrorMetricsForGetReusedCredentialsCount() {
        HistogramWatcher histogram =
                histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                                PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT,
                                CredentialManagerError.API_EXCEPTION,
                                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR))
                        .build();
        chooseToSyncPasswords();
        returnErrorWhenGettingReusedCredentialsCount(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));

        mPasswordManagerHelper.getReusedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));

        histogram.assertExpected();
    }

    @Test
    public void testRecordsMetricsWhenPasswordCheckupIntentFails() throws CanceledException {
        HistogramWatcher.Builder builder =
                histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                        PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT);
        HistogramWatcher histogram =
                builder.expectIntRecord(
                                PasswordMetricsUtil
                                        .PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM,
                                0)
                        .build();
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        mPasswordManagerHelper.showPasswordCheckup(
                ContextUtils.getApplicationContext(),
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);

        histogram.assertExpected();
    }

    @Test
    public void testShowsLoadingDialogOnPasswordCheckup() throws CanceledException {
        chooseToSyncPasswords();

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).show();
    }

    @Test
    public void testDismissesLoadingDialogWhenPasswordCheckupIntentSent() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testDismissesLoadingDialogOnPasswordCheckupIntentSendError()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testDismissesLoadingDialogOnPasswordCheckupIntentGetError()
            throws CanceledException {
        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED),
                TEST_EMAIL_ADDRESS);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testDoesNotLaunchPasswordCheckupIntentWhenLoadingDialogCancelled()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    public void testDoesNotLaunchPasswordCheckupIntentWhenLoadingDialogTimedOut()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mPendingIntentMock, never()).send();
    }

    @Test
    public void testPasswordCheckupLaunchWaitsForDialogDismissability() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mPendingIntentMock, never()).send();

        mLoadingDialogCoordinatorObserver.onDismissable();
        verify(mPendingIntentMock).send();
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogNotShown() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogShown() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(true);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogShownNonDismissable()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);
        when(mLoadingModalDialogCoordinator.isImmediatelyDismissable()).thenReturn(false);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        mLoadingDialogCoordinatorObserver.onDismissable();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogCancelled() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogCancelledDuringLoad()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.CANCELLED);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.CANCELLED);
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogTimeout() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnDialogTimeoutDuringLoad()
            throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.SHOWN);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.TIMED_OUT);
        mLoadingDialogCoordinatorObserver.onDismissedWithState(
                LoadingModalDialogCoordinator.State.TIMED_OUT);
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnIntentFetchError()
            throws CanceledException {
        chooseToSyncPasswords();
        returnErrorWhenFetchingIntentForPasswordCheckup(
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED),
                TEST_EMAIL_ADDRESS);
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsCheckupLoadingDialogMetricsOnIntentSendError() throws CanceledException {
        chooseToSyncPasswords();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock, TEST_EMAIL_ADDRESS);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();
        when(mLoadingModalDialogCoordinator.getState())
                .thenReturn(LoadingModalDialogCoordinator.State.PENDING);

        mPasswordManagerHelper.launchPasswordCheckup(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_EMAIL_ADDRESS),
                mLoadingModalDialogCoordinator,
                mModalDialogManagerSupplier,
                ContextUtils.getApplicationContext(),
                mSettingsCustomTabLauncher);

        verify(mLoadingModalDialogCoordinator).dismiss();
    }

    @Test
    public void testRecordsErrorMetricsWhenRunPasswordCheckupFails() {
        chooseToSyncPasswords();
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenRunningPasswordCheckup(expectedException);

        mPasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));
    }

    @Test
    public void testRecordsErrorMetricsWhenGetBreachedCredentialsCountFails() {
        chooseToSyncPasswords();
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenGettingBreachedCredentialsCount(expectedException);

        mPasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));
    }

    @Test
    public void testRecordsErrorMetricsWhenGetWeakCredentialsCountFails() {
        chooseToSyncPasswords();
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenGettingWeakCredentialsCount(expectedException);

        mPasswordManagerHelper.getWeakCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));
    }

    @Test
    public void testRecordsErrorMetricsWhenGetReusedCredentialsCountFails() {
        chooseToSyncPasswords();
        Exception expectedException =
                new PasswordCheckBackendException("", CredentialManagerError.UNCATEGORIZED);
        returnErrorWhenGettingReusedCredentialsCount(expectedException);

        mPasswordManagerHelper.getReusedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                TEST_EMAIL_ADDRESS,
                mock(Callback.class),
                mock(Callback.class));
    }

    @Test
    public void testShowDownloadCsvDialogIfCsvIsPresentAndPwmNotAvailable() {
        // The dialog exists only if the login db deprecation is enabled.
        assumeTrue(mIsLoginDbDeprecationEnabled);
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
        mPasswordManagerHelper.showPasswordCheckup(
                testActivity,
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
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
    public void testShowDownloadCsvDialogIfCsvIsPresentAndNoGms() {
        // The dialog exists only if the login db deprecation is enabled.
        assumeTrue(mIsLoginDbDeprecationEnabled);
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
        mPasswordManagerHelper.showPasswordCheckup(
                testActivity,
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
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
        // The dialog exists only if the login db deprecation is enabled.
        assumeTrue(mIsLoginDbDeprecationEnabled);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();

        mPasswordManagerHelper.showPasswordCheckup(
                testActivity,
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        assertEquals(
                testActivity
                        .getResources()
                        .getString(
                                org.chromium.chrome.browser.access_loss.R.string
                                        .pwm_disabled_no_gms_dialog_title),
                dialogModel.get(ModalDialogProperties.TITLE));
    }

    @Test
    public void testShowPwmUnavailableDialogNoCsvUpdatableGms() {
        // The dialog exists only if the login db deprecation is enabled.
        assumeTrue(mIsLoginDbDeprecationEnabled);
        when(mBackendSupportHelperMock.isBackendPresent()).thenReturn(true);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(
                        eq(mPrefService), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        LoginDbDeprecationUtilBridge.setHasCsvFileForTesting(false);

        FragmentActivity testActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        setUpUpdatableGmsCore(testActivity);
        mPasswordManagerHelper.showPasswordCheckup(
                testActivity,
                PasswordCheckReferrer.SAFETY_CHECK,
                mModalDialogManagerSupplier,
                TEST_NO_EMAIL_ADDRESS,
                mSettingsCustomTabLauncher);
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertNotNull(dialogModel);
        assertEquals(
                testActivity
                        .getResources()
                        .getString(
                                org.chromium.chrome.browser.access_loss.R.string
                                        .access_loss_update_gms_title),
                dialogModel.get(ModalDialogProperties.TITLE));
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

    private void setUpSuccessfulCheckupIntentFetching(PendingIntent intent, String accountEmail) {
        doAnswer(
                        invocation -> {
                            Callback<PendingIntent> cb = invocation.getArgument(2);
                            cb.onResult(intent);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(
                        anyInt(),
                        eq(accountEmail == null ? Optional.empty() : Optional.of(accountEmail)),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForPasswordCheckup(
            Exception error, String accountEmail) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(error);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupIntent(
                        anyInt(),
                        eq(accountEmail == null ? Optional.empty() : Optional.of(accountEmail)),
                        any(Callback.class),
                        any(Callback.class));
    }

    private void setUpSuccessfulRunPasswordCheckup() {
        doAnswer(
                        invocation -> {
                            Callback<Void> cb = invocation.getArgument(2);
                            cb.onResult(null);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .runPasswordCheckupInBackground(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulGetBreachedCredentialsCount() {
        doAnswer(
                        invocation -> {
                            Callback<Integer> cb = invocation.getArgument(2);
                            cb.onResult(0);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getBreachedCredentialsCount(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulGetWeakCredentialsCount() {
        doAnswer(
                        invocation -> {
                            Callback<Integer> cb = invocation.getArgument(2);
                            cb.onResult(0);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getWeakCredentialsCount(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulGetReusedCredentialsCount() {
        doAnswer(
                        invocation -> {
                            Callback<Integer> cb = invocation.getArgument(2);
                            cb.onResult(0);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getReusedCredentialsCount(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenRunningPasswordCheckup(Exception error) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(error);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .runPasswordCheckupInBackground(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenGettingBreachedCredentialsCount(Exception error) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(error);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getBreachedCredentialsCount(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenGettingWeakCredentialsCount(Exception error) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(error);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getWeakCredentialsCount(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenGettingReusedCredentialsCount(Exception error) {
        doAnswer(
                        invocation -> {
                            Callback<Exception> cb = invocation.getArgument(3);
                            cb.onResult(error);
                            return true;
                        })
                .when(mPasswordCheckupClientHelperMock)
                .getReusedCredentialsCount(
                        anyInt(), eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private HistogramWatcher.Builder
            histogramWatcherBuilderOfPasswordCheckupSuccessHistogramsForOperation(
                    @PasswordCheckOperation int operation) {
        final String nameWithSuffix =
                PasswordMetricsUtil.PASSWORD_CHECKUP_HISTOGRAM_BASE
                        + "."
                        + getPasswordCheckupHistogramSuffixForOperation(operation);
        return HistogramWatcher.newBuilder()
                .expectIntRecord(nameWithSuffix + ".Success", 1)
                .expectIntRecord(nameWithSuffix + ".Latency", 0)
                .expectNoRecords(nameWithSuffix + ".ErrorLatency")
                .expectNoRecords(nameWithSuffix + ".Error")
                .expectNoRecords(nameWithSuffix + ".ApiError");
    }

    private HistogramWatcher.Builder
            histogramWatcherBuilderOfPasswordCheckupFailureHistogramsForOperation(
                    @PasswordCheckOperation int operation,
                    int errorCode,
                    OptionalInt apiErrorCode) {
        final String nameWithSuffix =
                PasswordMetricsUtil.PASSWORD_CHECKUP_HISTOGRAM_BASE
                        + "."
                        + getPasswordCheckupHistogramSuffixForOperation(operation);

        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(nameWithSuffix + ".Success", 0)
                        .expectNoRecords(nameWithSuffix + ".Latency")
                        .expectIntRecord(nameWithSuffix + ".ErrorLatency", 0)
                        .expectIntRecord(nameWithSuffix + ".Error", errorCode);
        if (apiErrorCode.isPresent()) {
            return builder.expectIntRecord(nameWithSuffix + ".APIError", apiErrorCode.getAsInt());
        } else {
            return builder.expectNoRecords(nameWithSuffix + ".APIError");
        }
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
            case PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT:
                return "GetWeakCredentialsCount";
            case PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT:
                return "GetReusedCredentialsCount";
            default:
                throw new AssertionError();
        }
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
}
