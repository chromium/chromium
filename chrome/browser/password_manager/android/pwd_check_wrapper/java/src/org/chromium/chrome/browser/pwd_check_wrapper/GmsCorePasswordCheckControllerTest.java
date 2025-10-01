// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelperFactoryImpl;
import org.chromium.chrome.browser.password_manager.FakePasswordManagerBackendSupportHelper;
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
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Set;
import java.util.concurrent.ExecutionException;

/** Unit tests for {@link GmsCorePasswordCheckController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// This is only used from Safety Check v1 which will be soon deprecated in favor Safety Check v2.
// There is still one entry point to this from the PhishGuard dialog.
public class GmsCorePasswordCheckControllerTest {
    private static final String TEST_EMAIL_ADDRESS = "test@example.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SyncService mSyncService;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;
    @Mock private Profile mProfile;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;
    FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;

    private GmsCorePasswordCheckController mController;

    @Before
    public void setUp() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        configureMockSyncServiceToSyncPasswords();
        configurePasswordManagerBackendSupport();
        setFakePasswordCheckupClientHelper();
        mController =
                new GmsCorePasswordCheckController(
                        mSyncService,
                        mPasswordStoreBridge,
                        PasswordManagerHelper.getForProfile(mProfile));
    }

    private void configurePasswordManagerBackendSupport() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeNativeMock);
        PasswordManagerHelperJni.setInstanceForTesting(mPasswordManagerHelperNativeMock);
        when(mPasswordManagerUtilBridgeNativeMock.isPasswordManagerAvailable(true))
                .thenReturn(true);

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

    /**
     * The flow: checkPasswords is called -> as a result of password check 0 breached credentials
     * are obtained -> 10 passwords overall have been loaded.
     */
    @Test
    public void passwordCheckResultIsCompleteNoBreachedCredentials()
            throws ExecutionException, InterruptedException {
        // Set fake to return 0 breached credentials.
        final int totalPasswords = 10;
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForAccountStore())
                .thenReturn(totalPasswords);
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore()).thenReturn(0);
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(0);
        mController.onSavedPasswordsChanged(totalPasswords);

        PasswordCheckResult passwordCheckResult =
                mController.checkPasswords(PasswordStorageType.ACCOUNT_STORAGE).get();

        Assert.assertEquals(0, passwordCheckResult.getBreachedCount().intValue());
        Assert.assertEquals(
                totalPasswords, passwordCheckResult.getTotalPasswordsCount().intValue());
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

        Assert.assertEquals(0, passwordCheckResult.getBreachedCount().intValue());
        Assert.assertEquals(0, passwordCheckResult.getTotalPasswordsCount().intValue());
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

        Assert.assertNull(passwordCheckResult.getBreachedCount());
        Assert.assertNull(passwordCheckResult.getTotalPasswordsCount());
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
        // Set fake to return 0 breached credentials.
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForAccountStore()).thenReturn(10);
        when(mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore()).thenReturn(0);
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(0);
        mController.onSavedPasswordsChanged(10);

        PasswordCheckResult passwordCheckResultLocal =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE).get();
        PasswordCheckResult passwordCheckResultAccount =
                mController.checkPasswords(PasswordStorageType.ACCOUNT_STORAGE).get();

        Assert.assertEquals(0, passwordCheckResultLocal.getBreachedCount().intValue());
        Assert.assertEquals(0, passwordCheckResultLocal.getTotalPasswordsCount().intValue());
        Assert.assertEquals(0, passwordCheckResultAccount.getBreachedCount().intValue());
        Assert.assertEquals(10, passwordCheckResultAccount.getTotalPasswordsCount().intValue());
    }
}
