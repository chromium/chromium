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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
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
    @Mock private SyncConsentActivityLauncher mSyncLauncher;

    private ModalDialogManager mModalDialogManager;
    private Context mContext;
    private SafetyHubModuleDelegate mSafetyHubModuleDelegate;
    private Profile mProfile;
    private PendingIntent mPasswordCheckIntentForAccountCheckup;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mProfile = mSafetyHubTestRule.getProfile();
        mPasswordCheckIntentForAccountCheckup =
                mSafetyHubTestRule.getIntentForAccountPasswordCheckup();

        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);

        mSafetyHubModuleDelegate =
                new SafetyHubModuleDelegateImpl(
                        mProfile, mModalDialogManagerSupplier, mSigninLauncher, mSyncLauncher);
    }

    @Test
    public void testOpenPasswordCheckUI() throws PendingIntent.CanceledException {
        mSafetyHubTestRule.setSignedInState(true);
        mSafetyHubTestRule.setUPMStatus(true);

        mSafetyHubModuleDelegate.showPasswordCheckUI(mContext);
        verify(mPasswordCheckIntentForAccountCheckup, times(1)).send();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testLaunchSigninPromo() {
        mSafetyHubTestRule.setSignedInState(false);
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
        mSafetyHubTestRule.setSignedInState(false);
        mSafetyHubModuleDelegate.launchSyncOrSigninPromo(mContext);
        verify(mSyncLauncher)
                .launchActivityIfAllowed(eq(mContext), eq(SigninAccessPoint.SAFETY_CHECK));
    }
}
