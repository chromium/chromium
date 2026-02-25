// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.cards.SignInPromoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link SignInPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
@EnableFeatures({
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
})
public class SignInPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private SetupListManager mSetupListManager;
    @Mock private Profile mProfile;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSignInCoordinator;

    private SignInPromoCoordinator mSignInPromoCoordinator;

    @Before
    public void setUp() {
        SetupListManager.setInstanceForTesting(mSetupListManager);
        NonNullObservableSupplier<Profile> profileSupplier =
                ObservableSuppliers.createNonNull(mProfile);
        when(mActionDelegate.getProfileSupplier()).thenReturn(profileSupplier);
        when(mActionDelegate.createBottomSheetSigninAndHistorySyncCoordinator(
                        any(), eq(SigninAccessPoint.SET_UP_LIST)))
                .thenReturn(mSignInCoordinator);

        mSignInPromoCoordinator =
                new SignInPromoCoordinator(mOnModuleClickedCallback, mActionDelegate);
    }

    @Test
    @SmallTest
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOnCardClicked_legacy() {
        mSignInPromoCoordinator.onCardClicked();

        verify(mActionDelegate).showSignInLegacy();
        verify(mOnModuleClickedCallback).run();
        verify(mSignInCoordinator, never()).startSigninFlow(any());
    }

    @Test
    @SmallTest
    public void testOnCardClicked() {
        mSignInPromoCoordinator.onCardClicked();

        verify(mActionDelegate).createSigninBottomSheetConfig();
        verify(mSignInCoordinator).startSigninFlow(any());
        verify(mOnModuleClickedCallback).run();
        verify(mActionDelegate, never()).showSignInLegacy();
    }

    @Test
    @SmallTest
    public void testIsComplete_Completed() {
        when(mSetupListManager.isModuleCompleted(ModuleType.SIGN_IN_PROMO)).thenReturn(true);
        assertTrue(mSignInPromoCoordinator.isComplete());
    }

    @Test
    @SmallTest
    public void testIsComplete_NotCompleted() {
        when(mSetupListManager.isModuleCompleted(ModuleType.SIGN_IN_PROMO)).thenReturn(false);
        assertFalse(mSignInPromoCoordinator.isComplete());
    }
}
