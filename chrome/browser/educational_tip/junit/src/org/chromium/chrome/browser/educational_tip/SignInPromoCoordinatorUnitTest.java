// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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
import org.chromium.chrome.browser.educational_tip.cards.SignInPromoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link SignInPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class SignInPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private SetupListManager mSetupListManager;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;

    private SignInPromoCoordinator mSignInPromoCoordinator;

    @Before
    public void setUp() {
        SetupListManager.setInstanceForTesting(mSetupListManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        NonNullObservableSupplier<Profile> profileSupplier =
                ObservableSuppliers.createNonNull(mProfile);
        when(mActionDelegate.getProfileSupplier()).thenReturn(profileSupplier);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        mSignInPromoCoordinator =
                new SignInPromoCoordinator(mOnModuleClickedCallback, mActionDelegate);
        verify(mActionDelegate)
                .createBottomSheetSigninAndHistorySyncCoordinator(
                        any(), eq(SigninAccessPoint.SET_UP_LIST));
    }

    @Test
    @SmallTest
    public void testOnCardClicked() {
        mSignInPromoCoordinator.onCardClicked();
        verify(mActionDelegate).startSignInFlow(any());
        verify(mOnModuleClickedCallback).run();
    }

    @Test
    @SmallTest
    public void testIsComplete_AlreadyCompleted() {
        when(mSetupListManager.isModuleCompleted(ModuleType.SIGN_IN_PROMO)).thenReturn(true);
        assertTrue(mSignInPromoCoordinator.isComplete());
    }

    @Test
    @SmallTest
    public void testIsComplete_SignedIn() {
        when(mSetupListManager.isModuleCompleted(ModuleType.SIGN_IN_PROMO)).thenReturn(false);
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);

        assertTrue(mSignInPromoCoordinator.isComplete());
    }

    @Test
    @SmallTest
    public void testIsComplete_NotCompletedNotSignedIn() {
        when(mSetupListManager.isModuleCompleted(ModuleType.SIGN_IN_PROMO)).thenReturn(false);
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);

        assertFalse(mSignInPromoCoordinator.isComplete());
    }
}
