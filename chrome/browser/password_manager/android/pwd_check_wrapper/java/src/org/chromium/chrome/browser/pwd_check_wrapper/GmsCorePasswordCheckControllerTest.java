// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import static org.junit.Assume.assumeFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelperFactoryImpl;
import org.chromium.chrome.browser.password_manager.FakePasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordStorageType;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Arrays;
import java.util.Collection;
import java.util.OptionalInt;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/** Unit tests for {@link GmsCorePasswordCheckController}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// This is only used from Safety Check v1 which will be soon deprecated in favor Safety Check v2.
// There is still one entry point to this from the PhishGuard dialog.
public class GmsCorePasswordCheckControllerTest {
    private static final String TEST_EMAIL_ADDRESS = "test@example.com";

    @Parameters
    public static Collection testCases() {
        return Arrays.asList(
                /* isLoginDbDeprecationEnabled= */ false, /* isLoginDbDeprecationEnabled= */ true);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Parameter public boolean mIsLoginDbDeprecationEnabled;

    @Mock private SyncService mSyncService;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;
    FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;

    private GmsCorePasswordCheckController mController;

    @Before
    public void setUp() {
        if (mIsLoginDbDeprecationEnabled) {
            FeatureOverrides.enable(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID);
        } else {
            FeatureOverrides.disable(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID);
        }
        setupUserProfileWithMockPrefService();
        configureMockSyncServiceToSyncPasswords();
        configurePasswordManagerBackendSupport();
        setFakePasswordCheckupClientHelper();
        mController =
                new GmsCorePasswordCheckController(
                        mSyncService,
                        mPrefService,
                        mPasswordStoreBridge,
                        PasswordManagerHelper.getForProfile(mProfile));
    }

    private void configurePasswordManagerBackendSupport() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeNativeMock);
        PasswordManagerHelperJni.setInstanceForTesting(mPasswordManagerHelperNativeMock);
        if (mIsLoginDbDeprecationEnabled) {
            when(mPasswordManagerUtilBridgeNativeMock.isPasswordManagerAvailable(
                            mPrefService, true))
                    .thenReturn(true);
        } else {
            when(mPasswordManagerUtilBridgeNativeMock.shouldUseUpmWiring(
                            mSyncService, mPrefService))
                    .thenReturn(true);
            when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                    .thenReturn(false);
            when(mPasswordManagerUtilBridgeNativeMock.areMinUpmRequirementsMet()).thenReturn(true);
        }

        FakePasswordManagerBackendSupportHelper helper =
                new FakePasswordManagerBackendSupportHelper();
        helper.setBackendPresent(true);
        PasswordManagerBackendSupportHelper.setInstanceForTesting(helper);
    }

    private void configureMockSyncServiceToSyncPasswords() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mSyncService.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                TEST_EMAIL_ADDRESS, new GaiaId("0")));
        when(mPasswordManagerHelperNativeMock.hasChosenToSyncPasswords(mSyncService))
                .thenReturn(true);
    }

    private void setFakePasswordCheckupClientHelper() {
        FakePasswordCheckupClientHelperFactoryImpl passwordCheckupClientHelperFactory =
                new FakePasswordCheckupClientHelperFactoryImpl();
        mPasswordCheckupClientHelper =
                (FakePasswordCheckupClientHelper) passwordCheckupClientHelperFactory.createHelper();
        PasswordCheckupClientHelperFactory.setFactoryForTesting(passwordCheckupClientHelperFactory);
    }

    private void setupUserProfileWithMockPrefService() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
    }

    /**
     * The flow: checkPasswords is called -> as a result of password check 0 breached credentials
     * are obtained -> 10 passwords overall have been loaded.
     */
    @Test
    public void passwordCheckResultIsCompleteNoBreachedCredentials_NoSplitStores()
            throws ExecutionException, InterruptedException {
        assumeFalse(mIsLoginDbDeprecationEnabled);
        // Set fake to return 0 breached credentials.
        final int totalPasswords = 10;
        // Before splitting stores, the account storage is backend by the profile store.
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore())
                .thenReturn(totalPasswords);
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(0);
        mController.onSavedPasswordsChanged(totalPasswords);

        PasswordCheckResult passwordCheckResult =
                mController.checkPasswords(PasswordStorageType.ACCOUNT_STORAGE).get();

        Assert.assertEquals(OptionalInt.of(0), passwordCheckResult.getBreachedCount());
        Assert.assertEquals(
                OptionalInt.of(totalPasswords), passwordCheckResult.getTotalPasswordsCount());
        Assert.assertEquals(null, passwordCheckResult.getError());
    }

    /**
     * The flow: checkPasswords is called -> as a result of password check 0 breached credentials
     * are obtained -> 10 passwords overall have been loaded.
     */
    @Test
    public void passwordCheckResultIsCompleteNoBreachedCredentials_SplitStores()
            throws ExecutionException, InterruptedException {
        // The split stores check is only important before the login db deprecation.
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(!mIsLoginDbDeprecationEnabled);

        // Set fake to return 0 breached credentials.
        final int totalPasswords = 10;
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForAccountStore())
                .thenReturn(totalPasswords);
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore()).thenReturn(0);
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(0);
        mController.onSavedPasswordsChanged(totalPasswords);

        PasswordCheckResult passwordCheckResult =
                mController.checkPasswords(PasswordStorageType.ACCOUNT_STORAGE).get();

        Assert.assertEquals(OptionalInt.of(0), passwordCheckResult.getBreachedCount());
        Assert.assertEquals(
                OptionalInt.of(totalPasswords), passwordCheckResult.getTotalPasswordsCount());
        Assert.assertEquals(null, passwordCheckResult.getError());
    }

    /**
     * The flow: passwords loading has finished and there are 0 passwords -> as a result of password
     * check 0 breached credentials are obtained.
     */
    @Test
    public void passwordCheckResultIsCompleteNoCredentials()
            throws ExecutionException, InterruptedException {
        // Set fake to return 0 breached credentials.
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore()).thenReturn(0);
        mController.onSavedPasswordsChanged(0);
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(0);

        PasswordCheckResult passwordCheckResult =
                mController.checkPasswords(PasswordStorageType.ACCOUNT_STORAGE).get();

        Assert.assertEquals(OptionalInt.of(0), passwordCheckResult.getBreachedCount());
        Assert.assertEquals(OptionalInt.of(0), passwordCheckResult.getTotalPasswordsCount());
        Assert.assertEquals(null, passwordCheckResult.getError());
    }

    /**
     * The flow: passwords loading has finished -> checkPasswords throws an error.
     */
    @Test
    public void passwordCheckThrowsError() throws ExecutionException, InterruptedException {
        final Exception error = new Exception("Simulate that password check throws an exception");
        mPasswordCheckupClientHelper.setError(error);

        PasswordCheckResult passwordCheckResult =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE).get();

        Assert.assertEquals(OptionalInt.empty(), passwordCheckResult.getBreachedCount());
        Assert.assertEquals(OptionalInt.empty(), passwordCheckResult.getTotalPasswordsCount());
        Assert.assertEquals(error, passwordCheckResult.getError());
    }

    @Test
    public void passwordCheckControllerIsDestroyedProperly() {
        mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE);

        mController.destroy();
        verify(mPasswordStoreBridge).removeObserver(mController);
    }

    @Test
    public void passwordCheckForBothStores() throws ExecutionException, InterruptedException {
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(true);
        // Set fake to return 0 breached credentials.
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForAccountStore()).thenReturn(10);
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore()).thenReturn(0);
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(0);
        mController.onSavedPasswordsChanged(10);

        PasswordCheckResult passwordCheckResultLocal =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE).get();
        PasswordCheckResult passwordCheckResultAccount =
                mController.checkPasswords(PasswordStorageType.ACCOUNT_STORAGE).get();

        Assert.assertEquals(OptionalInt.of(0), passwordCheckResultLocal.getBreachedCount());
        Assert.assertEquals(OptionalInt.of(0), passwordCheckResultLocal.getTotalPasswordsCount());
        Assert.assertEquals(OptionalInt.of(0), passwordCheckResultAccount.getBreachedCount());
        Assert.assertEquals(
                OptionalInt.of(10), passwordCheckResultAccount.getTotalPasswordsCount());
    }

    @Test
    public void getBreachedCredentialsCountReturnsBackendVersionNotSupportedError()
            throws ExecutionException, InterruptedException {
        assumeFalse(mIsLoginDbDeprecationEnabled);
        when(mPasswordManagerUtilBridgeNativeMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        PasswordCheckResult passwordCheckResultLocal =
                mController.getBreachedCredentialsCount(PasswordStorageType.LOCAL_STORAGE).get();

        Assert.assertNotNull(passwordCheckResultLocal.getError());
        Assert.assertTrue(
                passwordCheckResultLocal.getError() instanceof PasswordCheckBackendException);
        Assert.assertEquals(
                CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED,
                ((PasswordCheckBackendException) passwordCheckResultLocal.getError()).errorCode);
    }
}
