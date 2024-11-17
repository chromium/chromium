// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests {@link SafetyHubModuleDelegate} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class SafetyHubModuleDelegateTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private Intent mSigninIntent;
    @Mock private Context mContext;

    private SafetyHubModuleDelegate mSafetyHubModuleDelegate;
    private Profile mProfile;
    private PendingIntent mPasswordCheckIntentForAccountCheckup;

    @Before
    public void setUp() {
        mProfile = mSafetyHubTestRule.getProfile();
        mPasswordCheckIntentForAccountCheckup =
                mSafetyHubTestRule.getIntentForAccountPasswordCheckup();

        ModalDialogManager modalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(modalDialogManager);

        mSafetyHubModuleDelegate =
                new SafetyHubModuleDelegateImpl(
                        mProfile, mModalDialogManagerSupplier, mSigninLauncher);
    }

    @Test
    public void testOpenPasswordCheckUi() throws PendingIntent.CanceledException {
        mSafetyHubTestRule.setSignedInState(true);
        mSafetyHubTestRule.setUPMStatus(true);

        Context context = ContextUtils.getApplicationContext();
        mSafetyHubModuleDelegate.showPasswordCheckUi(context);
        verify(mPasswordCheckIntentForAccountCheckup, times(1)).send();
    }

    @Test
    public void testLaunchSigninPromo() {
        mSafetyHubTestRule.setSignedInState(false);
        when(mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                        eq(mContext),
                        eq(mProfile),
                        any(),
                        eq(
                                BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode
                                        .BOTTOM_SHEET),
                        eq(
                                BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(HistorySyncConfig.OptInMode.NONE),
                        eq(SigninAccessPoint.SAFETY_CHECK),
                        isNull()))
                .thenReturn(mSigninIntent);

        mSafetyHubModuleDelegate.launchSigninPromo(mContext);

        verify(mContext).startActivity(mSigninIntent);
    }
}
