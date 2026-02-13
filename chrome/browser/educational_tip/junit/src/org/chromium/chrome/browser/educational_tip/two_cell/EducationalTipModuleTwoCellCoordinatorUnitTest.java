// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link EducationalTipModuleTwoCellCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleTwoCellCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final @ModuleDelegate.ModuleType int MODULE_TYPE =
            SetupListModuleUtils.getTwoCellContainerModuleTypes().get(0);

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private Context mContext;
    private NonNullObservableSupplier<Profile> mProfileSupplier;
    private EducationalTipModuleTwoCellCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        mProfileSupplier = ObservableSuppliers.createNonNull(mProfile);
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
    }

    @Test
    @SmallTest
    public void testShowModule() {
        List<Integer> rankedModules =
                Arrays.asList(
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                        ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);
        SetupListModuleUtils.setRankedModuleTypesForTesting(rankedModules);
        mCoordinator =
                new EducationalTipModuleTwoCellCoordinator(
                        MODULE_TYPE, mModuleDelegate, mActionDelegate);

        mCoordinator.showModule();

        verify(mModuleDelegate)
                .onDataReady(
                        eq(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER),
                        mPropertyModelCaptor.capture());

        PropertyModel model = mPropertyModelCaptor.getValue();
        // Module Title
        assertEquals(
                mContext.getString(R.string.educational_tip_module_title),
                model.get(EducationalTipModuleTwoCellProperties.MODULE_TITLE));

        // Item 1 (Enhanced Safe Browsing)
        assertEquals(
                mContext.getString(R.string.educational_tip_enhanced_safe_browsing_title),
                model.get(EducationalTipModuleTwoCellProperties.ITEM_1_TITLE));
        assertEquals(
                mContext.getString(R.string.educational_tip_enhanced_safe_browsing_description),
                model.get(EducationalTipModuleTwoCellProperties.ITEM_1_DESCRIPTION));
        assertEquals(
                R.drawable.enhanced_safe_browsing_promo_logo,
                model.get(EducationalTipModuleTwoCellProperties.ITEM_1_ICON).intValue());
        assertNotNull(model.get(EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER));

        // Item 2 (Address Bar Placement)
        assertEquals(
                mContext.getString(R.string.educational_tip_address_bar_placement_title),
                model.get(EducationalTipModuleTwoCellProperties.ITEM_2_TITLE));
        assertEquals(
                mContext.getString(R.string.educational_tip_address_bar_placement_description),
                model.get(EducationalTipModuleTwoCellProperties.ITEM_2_DESCRIPTION));
        assertEquals(
                R.drawable.address_bar_placement_promo_logo,
                model.get(EducationalTipModuleTwoCellProperties.ITEM_2_ICON).intValue());
        assertNotNull(model.get(EducationalTipModuleTwoCellProperties.ITEM_2_CLICK_HANDLER));
    }

    @Test
    @SmallTest
    public void testBottomSheetObserver_AddedAndRemoved() {
        mCoordinator =
                new EducationalTipModuleTwoCellCoordinator(
                        MODULE_TYPE, mModuleDelegate, mActionDelegate);
        verify(mBottomSheetController).addObserver(any());
        mCoordinator.hideModule();
        verify(mBottomSheetController).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testOnClick_ReportsContainerType() {
        List<Integer> rankedModules =
                Arrays.asList(
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                        ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);
        SetupListModuleUtils.setRankedModuleTypesForTesting(rankedModules);
        mCoordinator =
                new EducationalTipModuleTwoCellCoordinator(
                        MODULE_TYPE, mModuleDelegate, mActionDelegate);

        mCoordinator.showModule();
        verify(mModuleDelegate).onDataReady(eq(MODULE_TYPE), mPropertyModelCaptor.capture());

        // Simulate click on Slot 1.
        mPropertyModelCaptor
                .getValue()
                .get(EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER)
                .run();

        // verify it reports MODULE_TYPE (container), not ENHANCED_SAFE_BROWSING_PROMO (item).
        // This is the fix for the NullPointerException crash.
        verify(mModuleDelegate).onModuleClicked(MODULE_TYPE);
    }

    @Test
    @SmallTest
    public void testOnClick_MarksItemComplete() {
        List<Integer> rankedModules =
                Arrays.asList(
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                        ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);
        SetupListModuleUtils.setRankedModuleTypesForTesting(rankedModules);
        mCoordinator =
                new EducationalTipModuleTwoCellCoordinator(
                        MODULE_TYPE, mModuleDelegate, mActionDelegate);

        mCoordinator.showModule();
        verify(mModuleDelegate).onDataReady(eq(MODULE_TYPE), mPropertyModelCaptor.capture());

        // Simulate click on Slot 1 (Enhanced Safe Browsing).
        mPropertyModelCaptor
                .getValue()
                .get(EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER)
                .run();

        // verify it marks the specific item (not the container) as complete.
        // This is what triggers the internal reordering.
        String expectedKey =
                SetupListModuleUtils.getCompletionKeyForModule(
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO);
        assertTrue(ChromeSharedPreferences.getInstance().readBoolean(expectedKey, false));
    }

    @Test
    @SmallTest
    public void testGetModuleType() {
        mCoordinator =
                new EducationalTipModuleTwoCellCoordinator(
                        MODULE_TYPE, mModuleDelegate, mActionDelegate);
        assertEquals(ModuleType.SETUP_LIST_TWO_CELL_CONTAINER, mCoordinator.getModuleType());
    }
}
