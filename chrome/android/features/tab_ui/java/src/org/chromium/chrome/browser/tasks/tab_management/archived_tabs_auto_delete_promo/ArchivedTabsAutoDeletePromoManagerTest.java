// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** Unit tests for {@link ArchivedTabsAutoDeletePromoManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabsAutoDeletePromoManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private TabArchiveSettings mMockTabArchiveSettings;
    private SettableNonNullObservableSupplier<Integer> mArchivedTabCountSupplier;
    private ArchivedTabsAutoDeletePromoManager mManager;

    @Before
    public void setUp() {
        mArchivedTabCountSupplier = ObservableSuppliers.createNonNull(0);
    }

    /** Sets up all conditions for the promo. */
    private void setupAllConditionsForPromo(
            boolean decisionMade,
            boolean archiveEnabled,
            boolean autoDeleteEnabled,
            int archiveCount) {
        when(mMockTabArchiveSettings.getAutoDeleteDecisionMade()).thenReturn(decisionMade);
        when(mMockTabArchiveSettings.getArchiveEnabled()).thenReturn(archiveEnabled);
        when(mMockTabArchiveSettings.isAutoDeleteEnabled()).thenReturn(autoDeleteEnabled);
        mArchivedTabCountSupplier.set(archiveCount);
    }

    /** Helper to create the manager with common default mock setups for conditions. */
    private void createManager(
            boolean decisionMade,
            boolean archiveEnabled,
            boolean autoDeleteEnabled,
            int archiveCount) {
        setupAllConditionsForPromo(decisionMade, archiveEnabled, autoDeleteEnabled, archiveCount);
        mManager =
                new ArchivedTabsAutoDeletePromoManager(
                        ApplicationProvider.getApplicationContext(),
                        mMockBottomSheetController,
                        mMockTabArchiveSettings,
                        mArchivedTabCountSupplier);
    }

    @Test
    public void testTryToShowPromo_AllConditionsMet_ShowsPromo() {
        createManager(
                /* decisionMade= */ false,
                /* archiveEnabled= */ true,
                /* autoDeleteEnabled= */ false,
                /* archiveCount= */ 1);

        mManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        verify(mMockBottomSheetController)
                .requestShowContent(any(ArchivedTabsAutoDeletePromoSheetContent.class), eq(true));
    }

    @Test
    public void testTryToShowPromo_DecisionMadeElsewhere_NoPromo() {
        createManager(
                /* decisionMade= */ true,
                /* archiveEnabled= */ true,
                /* autoDeleteEnabled= */ false,
                /* archiveCount= */ 1);

        mManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testTryToShowPromo_ArchivingDisabled_NoPromo() {
        createManager(
                /* decisionMade= */ false,
                /* archiveEnabled= */ false,
                /* autoDeleteEnabled= */ false,
                /* archiveCount= */ 1);

        mManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testTryToShowPromo_AutoDeleteEnabled_NoPromo() {
        createManager(
                /* decisionMade= */ false,
                /* archiveEnabled= */ true,
                /* autoDeleteEnabled= */ true,
                /* archiveCount= */ 1);

        mManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testTryToShowPromo_NoArchivedTabs_NoPromo() {
        createManager(
                /* decisionMade= */ false,
                /* archiveEnabled= */ true,
                /* autoDeleteEnabled= */ false,
                /* archiveCount= */ 0);

        mManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testDestroy_CoordinatorDestroyed() {
        createManager(
                /* decisionMade= */ false,
                /* archiveEnabled= */ true,
                /* autoDeleteEnabled= */ false,
                /* archiveCount= */ 1);
        when(mMockBottomSheetController.requestShowContent(
                        any(ArchivedTabsAutoDeletePromoSheetContent.class), eq(true)))
                .thenReturn(true);

        mManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        mManager.destroy();

        verify(mMockBottomSheetController)
                .hideContent(
                        any(ArchivedTabsAutoDeletePromoSheetContent.class),
                        eq(false),
                        eq(StateChangeReason.NONE));
    }

    @Test
    public void testDestroy_CoordinatorNeverCreated() {
        createManager(
                /* decisionMade= */ false,
                /* archiveEnabled= */ true,
                /* autoDeleteEnabled= */ false,
                /* archiveCount= */ 1);

        mManager.destroy();

        verify(mMockBottomSheetController, never()).hideContent(any(), anyBoolean(), anyInt());
    }
}
