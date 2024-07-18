// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelperFactoryImpl;
import org.chromium.chrome.browser.password_manager.FakePasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests {@link SafetyHubModuleDelegate} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class SafetyHubModuleDelegateTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PendingIntent mPasswordCheckIntentForAccountCheckup;
    @Mock private SyncService mSyncService;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNatives;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private SyncConsentActivityLauncher mSyncLauncher;

    private ModalDialogManager mModalDialogManager;

    private Context mContext;

    private SafetyHubModuleDelegate mSafetyHubModuleDelegate;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNatives);
        mJniMocker.mock(PasswordManagerHelperJni.TEST_HOOKS, mPasswordManagerHelperNativeMock);

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        setUpPasswordManagerBackendForTesting();

        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);

        mSafetyHubModuleDelegate =
                new SafetyHubModuleDelegateImpl(
                        mProfile, mModalDialogManagerSupplier, mSigninLauncher, mSyncLauncher);
    }

    private void setSignedInState(boolean signedIn) {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(signedIn);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(
                        signedIn
                                ? CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0")
                                : null);
    }

    private void setUPMStatus(boolean isUPMEnabled) {
        when(mPasswordManagerUtilBridgeNatives.shouldUseUpmWiring(mSyncService, mPrefService))
                .thenReturn(isUPMEnabled);
        when(mPasswordManagerUtilBridgeNatives.areMinUpmRequirementsMet()).thenReturn(true);
    }

    private void setUpPasswordManagerBackendForTesting() {
        FakePasswordManagerBackendSupportHelper helper =
                new FakePasswordManagerBackendSupportHelper();
        helper.setBackendPresent(true);
        PasswordManagerBackendSupportHelper.setInstanceForTesting(helper);

        setUpFakePasswordCheckupClientHelper();
    }

    private void setUpFakePasswordCheckupClientHelper() {
        FakePasswordCheckupClientHelperFactoryImpl passwordCheckupClientHelperFactory =
                new FakePasswordCheckupClientHelperFactoryImpl();
        PasswordCheckupClientHelperFactory.setFactoryForTesting(passwordCheckupClientHelperFactory);
        FakePasswordCheckupClientHelper fakePasswordCheckupClientHelper =
                (FakePasswordCheckupClientHelper) passwordCheckupClientHelperFactory.createHelper();

        fakePasswordCheckupClientHelper.setIntentForAccountCheckup(
                mPasswordCheckIntentForAccountCheckup);
    }

    @Test
    public void testOpenPasswordCheckUI() throws PendingIntent.CanceledException {
        setSignedInState(true);
        setUPMStatus(true);

        mSafetyHubModuleDelegate.showPasswordCheckUI(mContext);
        verify(mPasswordCheckIntentForAccountCheckup, times(1)).send();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testLaunchSigninPromo() {
        setSignedInState(false);
        mSafetyHubModuleDelegate.launchSyncOrSigninPromo(mContext);
        verify(mSigninLauncher)
                .launchActivityIfAllowed(
                        eq(mContext),
                        eq(mProfile),
                        any(),
                        eq(SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET),
                        eq(
                                SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE),
                        eq(SigninAccessPoint.SAFETY_CHECK));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testLaunchSyncPromo() {
        setSignedInState(false);
        mSafetyHubModuleDelegate.launchSyncOrSigninPromo(mContext);
        verify(mSyncLauncher)
                .launchActivityIfAllowed(eq(mContext), eq(SigninAccessPoint.SAFETY_CHECK));
    }
}
