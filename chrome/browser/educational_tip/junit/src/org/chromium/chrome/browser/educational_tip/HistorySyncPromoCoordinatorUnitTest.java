// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.educational_tip.cards.HistorySyncPromoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/** Test relating to {@link HistorySyncPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
})
public class HistorySyncPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private Runnable mRemoveModuleCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SyncService mSyncService;
    @Mock private SetupListManager mSetupListManager;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSignInCoordinator;
    private NonNullObservableSupplier<Profile> mProfileSupplier;

    private HistorySyncPromoCoordinator mHistorySyncPromoCoordinator;

    @Before
    public void setUp() {
        mProfileSupplier = ObservableSuppliers.createNonNull(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        SetupListManager.setInstanceForTesting(mSetupListManager);
        when(mActionDelegate.createBottomSheetSigninAndHistorySyncCoordinator(
                        any(), eq(SigninAccessPoint.HISTORY_SYNC_EDUCATIONAL_TIP)))
                .thenReturn(mSignInCoordinator);

        mHistorySyncPromoCoordinator =
                new HistorySyncPromoCoordinator(
                        mOnModuleClickedCallback,
                        new CallbackController(),
                        mActionDelegate,
                        mRemoveModuleCallback);
    }

    @Test
    @SmallTest
    public void testGetCardImage_SetupList() {
        when(mSetupListManager.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)).thenReturn(true);
        assertEquals(
                R.drawable.setup_list_history_sync_promo_logo,
                mHistorySyncPromoCoordinator.getCardImage());
    }

    @Test
    @SmallTest
    public void testGetCardImage_Default() {
        when(mSetupListManager.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)).thenReturn(false);
        assertEquals(
                R.drawable.history_sync_promo_logo, mHistorySyncPromoCoordinator.getCardImage());
    }

    @Test
    @SmallTest
    public void testSyncStateChanged_SetupList_MarksCompleteOnly() {
        when(mSetupListManager.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)).thenReturn(true);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        mHistorySyncPromoCoordinator.syncStateChanged();

        // Should mark complete, but NOT call remove.
        // The animation will be triggered by onShown() or on return from UI.
        verify(mSetupListManager)
                .setModuleCompleted(ModuleType.HISTORY_SYNC_PROMO, /* silent= */ true);
        verify(mRemoveModuleCallback, never()).run();
    }

    @Test
    @SmallTest
    public void testSyncStateChanged_Default_RemovesModule() {
        when(mSetupListManager.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)).thenReturn(false);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        mHistorySyncPromoCoordinator.syncStateChanged();

        // Should call remove, NOT mark complete.
        verify(mRemoveModuleCallback).run();
        String expectedKey =
                SetupListModuleUtils.getCompletionKeyForModule(ModuleType.HISTORY_SYNC_PROMO);
        assertFalse(ChromeSharedPreferences.getInstance().readBoolean(expectedKey, false));
    }

    @Test
    @SmallTest
    public void testCardRemovedOnSignout() {
        mHistorySyncPromoCoordinator.onPrimaryAccountChanged(
                new PrimaryAccountChangeEvent(
                        PrimaryAccountChangeEvent.Type.CLEARED, ConsentLevel.SIGNIN));

        verify(mRemoveModuleCallback).run();
    }

    @Test
    @SmallTest
    public void testCardRemovedOnWhenHistorySyncUpdated() {
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        mHistorySyncPromoCoordinator.syncStateChanged();

        verify(mRemoveModuleCallback).run();
    }

    @Test
    @SmallTest
    public void testCardNotRemovedOnWhenOtherSyncTypeUpdated() {
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.BOOKMARKS));

        mHistorySyncPromoCoordinator.syncStateChanged();

        verify(mRemoveModuleCallback, never()).run();
    }

    @Test
    @SmallTest
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testOnCardClicked_legacy() {
        mHistorySyncPromoCoordinator.onCardClicked();

        verify(mActionDelegate).showHistorySyncOptInLegacy(eq(mRemoveModuleCallback));
        verify(mOnModuleClickedCallback).run();
        verify(mSignInCoordinator, never()).startSigninFlow(any());
    }

    @Test
    @SmallTest
    public void testOnCardClicked() {
        mHistorySyncPromoCoordinator.onCardClicked();

        verify(mActionDelegate).createHistorySyncBottomSheetConfig();
        verify(mSignInCoordinator).startSigninFlow(any());
        verify(mOnModuleClickedCallback).run();
        verify(mActionDelegate, never()).showHistorySyncOptInLegacy(any());
    }
}
