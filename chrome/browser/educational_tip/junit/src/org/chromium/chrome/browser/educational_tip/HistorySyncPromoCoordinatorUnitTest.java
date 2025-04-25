// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.educational_tip;

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

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.cards.HistorySyncPromoCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

/** Test relating to {@link HistorySyncPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class HistorySyncPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private Runnable mRemoveModuleCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    ObservableSupplierImpl<Profile> mProfileSupplier;
    private HistorySyncPromoCoordinator mHistorySyncPromoCoordinator;

    @Before
    public void setUp() {
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mHistorySyncPromoCoordinator =
                new HistorySyncPromoCoordinator(
                        mOnModuleClickedCallback,
                        new CallbackController(),
                        mActionDelegate,
                        mRemoveModuleCallback);
    }

    @Test
    @SmallTest
    public void testCardRemovedOnSignout() {
        mHistorySyncPromoCoordinator.onPrimaryAccountChanged(
                new PrimaryAccountChangeEvent(
                        PrimaryAccountChangeEvent.Type.CLEARED, ConsentLevel.SIGNIN));

        verify(mRemoveModuleCallback).run();
    }
}
