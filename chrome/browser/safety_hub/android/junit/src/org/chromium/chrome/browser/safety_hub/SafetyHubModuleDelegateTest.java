// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests {@link SafetyHubModuleDelegate} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
})
public class SafetyHubModuleDelegateTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    @Mock private Intent mSigninIntent;
    @Mock private Context mContext;
    @Mock private SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    private SafetyHubModuleDelegate mSafetyHubModuleDelegate;
    private Profile mProfile;
    private PendingIntent mPasswordCheckIntentForAccountCheckup;
    private PendingIntent mPasswordCheckIntentForLocalCheckup;
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        mProfile = mSafetyHubTestRule.getProfile();
        mPasswordCheckIntentForAccountCheckup =
                mSafetyHubTestRule.getIntentForAccountPasswordCheckup();
        mPasswordCheckIntentForLocalCheckup = mSafetyHubTestRule.getIntentForLocalPasswordCheckup();

        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);

        OneshotSupplierImpl<BottomSheetController> bottomSheetControllerSupplier =
                new OneshotSupplierImpl<>();
        bottomSheetControllerSupplier.set(mBottomSheetController);
        when(mSigninLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                        eq(mWindowAndroid),
                        eq(mActivity),
                        eq(mActivityResultTracker),
                        any(),
                        eq(mDeviceLockActivityLauncher),
                        any(),
                        any(),
                        eq(mModalDialogManager),
                        eq(mSnackbarManager),
                        eq(SigninAccessPoint.SAFETY_CHECK)))
                .thenReturn(mSigninCoordinator);
        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(mSigninLauncher);

        mSafetyHubModuleDelegate =
                new SafetyHubModuleDelegateImpl(
                        mWindowAndroid,
                        mActivity,
                        mActivityResultTracker,
                        mDeviceLockActivityLauncher,
                        mProfile,
                        mSnackbarManager,
                        bottomSheetControllerSupplier,
                        () -> mModalDialogManager,
                        mSigninLauncher,
                        mSettingsCustomTabLauncher);

        ShadowLooper.idleMainLooper();
    }

    @After
    public void tearDown() {
        mModalDialogManager.destroy();
    }

    @Test
    public void testOpenPasswordCheckUi() throws PendingIntent.CanceledException {
        mSafetyHubTestRule.setSignedInState(true);
        mSafetyHubTestRule.setPasswordManagerAvailable(true);

        Context context = ContextUtils.getApplicationContext();
        mSafetyHubModuleDelegate.showPasswordCheckUi(context);
        verify(mPasswordCheckIntentForAccountCheckup, times(1)).send();
    }

    @Test
    public void testOpenLocalPasswordCheckUi() throws PendingIntent.CanceledException {
        mSafetyHubTestRule.setSignedInState(true);
        mSafetyHubTestRule.setPasswordManagerAvailable(true);

        Context context = ContextUtils.getApplicationContext();
        mSafetyHubModuleDelegate.showLocalPasswordCheckUi(context);
        verify(mPasswordCheckIntentForLocalCheckup, times(1)).send();
    }

    @Test
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testLaunchSigninPromo_legacy() {
        mSafetyHubTestRule.setSignedInState(false);
        when(mContext.getString(anyInt())).thenReturn("string");
        when(mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SAFETY_CHECK)))
                .thenReturn(mSigninIntent);

        mSafetyHubModuleDelegate.launchSigninPromo(mContext);

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mSigninLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mContext),
                        eq(mProfile),
                        configCaptor.capture(),
                        eq(SigninAccessPoint.SAFETY_CHECK));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        verify(mContext).startActivity(mSigninIntent);
        verifyNoInteractions(mSigninCoordinator);
    }

    @Test
    public void testLaunchSigninPromo() {
        mSafetyHubTestRule.setSignedInState(false);
        when(mContext.getString(anyInt())).thenReturn("string");

        mSafetyHubModuleDelegate.launchSigninPromo(mContext);

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mSigninCoordinator).startSigninFlow(configCaptor.capture());
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        verify(mContext, never()).startActivity(mSigninIntent);
    }

    @Test
    public void testDestroy() {
        mSafetyHubModuleDelegate.destroy();

        verify(mSigninCoordinator).destroy();
    }
}
