// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Test relating to {@link EducationalTipModuleBuilder} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class EducationalTipModuleBuilderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<ModuleProvider> mBuildCallback;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilter mNormalFilter;
    @Mock private TabGroupModelFilter mIncognitoFilter;
    @Mock private TabModel mNormalModel;
    @Mock private TabModel mIncognitoModel;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SetupListManager mSetupListManager;
    @Mock private BottomSheetController mBottomSheetController;

    private EducationalTipModuleBuilder mModuleBuilder;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(context);
        SetupListManager.setInstanceForTesting(mSetupListManager);
        when(mSetupListManager.isSetupListActive()).thenReturn(false);
        when(mSetupListManager.getManualRank(anyInt())).thenReturn(null);
        when(mActionDelegate.getProfileSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(mProfile));
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);
        when(mActionDelegate.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getTabGroupModelFilter(/* isIncognito= */ false))
                .thenReturn(mNormalFilter);
        when(mTabModelSelector.getTabGroupModelFilter(/* isIncognito= */ true))
                .thenReturn(mIncognitoFilter);
        when(mNormalFilter.getTabGroupCount()).thenReturn(0);
        when(mIncognitoFilter.getTabGroupCount()).thenReturn(0);
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mNormalModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoModel);
        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(0);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mModuleBuilder =
                new EducationalTipModuleBuilder(ModuleType.QUICK_DELETE_PROMO, mActionDelegate);
    }

    @Test
    @SmallTest
    public void testCreateView_CelebratoryPromoUsesCustomLayout() {
        EducationalTipModuleBuilder celebratoryBuilder =
                new EducationalTipModuleBuilder(
                        ModuleType.SETUP_LIST_CELEBRATORY_PROMO, mActionDelegate);
        ViewGroup view = celebratoryBuilder.createView(null);
        assertEquals(R.id.setup_list_celebratory_promo_layout, view.getId());
    }

    @Test
    @SmallTest
    public void testCreateView_RegularPromoUsesStandardLayout() {
        EducationalTipModuleBuilder regularBuilder =
                new EducationalTipModuleBuilder(ModuleType.QUICK_DELETE_PROMO, mActionDelegate);
        ViewGroup view = regularBuilder.createView(null);
        assertEquals(R.id.educational_tip_module_layout, view.getId());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER})
    public void testBuildEducationalTipModule_Eligible() {
        assertTrue(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER})
    @DisableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_CARD})
    public void testBuildEducationalTipDefaultBrowserModule_NotEligible() {
        EducationalTipModuleBuilder moduleBuilderForDefaultBrowser =
                new EducationalTipModuleBuilder(ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate);

        assertFalse(moduleBuilderForDefaultBrowser.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, never()).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER,
    })
    public void testBuildEducationalTipTabGroupSyncModule_Eligible() {
        EducationalTipModuleBuilder moduleBuilderForTabGroupSync =
                new EducationalTipModuleBuilder(ModuleType.TAB_GROUP_SYNC_PROMO, mActionDelegate);

        assertTrue(moduleBuilderForTabGroupSync.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER})
    public void testCreateInputContext() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.isModuleEligible(anyInt())).thenReturn(true);
        EducationalTipModuleBuilder moduleBuilderForDefaultBrowserPromo =
                new EducationalTipModuleBuilder(ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate);
        InputContext inputContextForTest = moduleBuilderForDefaultBrowserPromo.createInputContext();
        assertNotNull(
                inputContextForTest.getEntryValue(
                        "should_show_non_role_manager_default_browser_promo"));
        assertNotNull(
                inputContextForTest.getEntryValue(
                        "has_default_browser_promo_shown_in_other_surface"));
        assertNotNull(inputContextForTest.getEntryValue("is_user_signed_in"));
        assertNull(inputContextForTest.getEntryValue("tab_group_exists"));
        assertNull(inputContextForTest.getEntryValue("number_of_tabs"));

        EducationalTipModuleBuilder moduleBuilderForTabGroupPromo =
                new EducationalTipModuleBuilder(ModuleType.TAB_GROUP_PROMO, mActionDelegate);
        inputContextForTest = moduleBuilderForTabGroupPromo.createInputContext();
        assertNull(
                inputContextForTest.getEntryValue(
                        "should_show_non_role_manager_default_browser_promo"));
        assertNull(
                inputContextForTest.getEntryValue(
                        "has_default_browser_promo_shown_in_other_surface"));
        assertNotNull(inputContextForTest.getEntryValue("tab_group_exists"));
        assertNotNull(inputContextForTest.getEntryValue("number_of_tabs"));
        assertNotNull(inputContextForTest.getEntryValue("is_user_signed_in"));
    }

    @Test
    @SmallTest
    public void testIsEligible_SetupList_StrictlyFollowsManager() {
        // Mock Setup List module.
        int setupListModule = ModuleType.SIGN_IN_PROMO;
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.isSetupListModule(setupListModule)).thenReturn(true);
        EducationalTipModuleBuilder builder =
                new EducationalTipModuleBuilder(setupListModule, mActionDelegate);

        // Case 1: Manager says eligible.
        when(mSetupListManager.isModuleEligible(setupListModule)).thenReturn(true);
        assertTrue(builder.isEligible());

        // Case 2: Manager says ineligible.
        when(mSetupListManager.isModuleEligible(setupListModule)).thenReturn(false);
        assertFalse(builder.isEligible());
    }

    @Test
    @SmallTest
    public void testIsEligible_RegularTip_RequiresProfile() {
        // Mock regular Educational Tip module.
        int regularTipModule = ModuleType.QUICK_DELETE_PROMO;
        EducationalTipModuleBuilder builder =
                new EducationalTipModuleBuilder(regularTipModule, mActionDelegate);

        // Case 1: Profile is null. Implementation currently returns true for non-setup modules.
        when(mActionDelegate.getProfileSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(null));
        assertTrue(builder.isEligible());

        // Case 2: Profile is present.
        when(mActionDelegate.getProfileSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(mProfile));
        assertTrue(builder.isEligible());
    }

    @Test
    @SmallTest
    public void testGetManualRank_ReturnsRankForSetupListModuleWhenActive() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);
        when(mSetupListManager.getManualRank(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO)).thenReturn(0);
        when(mSetupListManager.getManualRank(ModuleType.ENHANCED_SAFE_BROWSING_PROMO))
                .thenReturn(1);
        when(mSetupListManager.getManualRank(ModuleType.SIGN_IN_PROMO)).thenReturn(2);
        when(mSetupListManager.getManualRank(ModuleType.SAVE_PASSWORDS_PROMO)).thenReturn(3);
        when(mSetupListManager.getManualRank(ModuleType.PASSWORD_CHECKUP_PROMO)).thenReturn(4);

        EducationalTipModuleBuilder builder1 =
                new EducationalTipModuleBuilder(
                        ModuleType.ADDRESS_BAR_PLACEMENT_PROMO, mActionDelegate);
        Integer manualOrder1 = builder1.getManualRank();
        assertNotNull(manualOrder1);
        assertEquals(0, manualOrder1.intValue()); // ADDRESS_BAR_PLACEMENT_PROMO is at index 0

        EducationalTipModuleBuilder builder2 =
                new EducationalTipModuleBuilder(
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO, mActionDelegate);
        Integer manualOrder2 = builder2.getManualRank();
        assertNotNull(manualOrder2);
        assertEquals(1, manualOrder2.intValue()); // ENHANCED_SAFE_BROWSING_PROMO is at index 1

        EducationalTipModuleBuilder builder3 =
                new EducationalTipModuleBuilder(ModuleType.SIGN_IN_PROMO, mActionDelegate);
        Integer manualOrder3 = builder3.getManualRank();
        assertNotNull(manualOrder3);
        assertEquals(2, manualOrder3.intValue()); // SIGN_IN_PROMO is at index 2

        EducationalTipModuleBuilder builder4 =
                new EducationalTipModuleBuilder(ModuleType.SAVE_PASSWORDS_PROMO, mActionDelegate);
        Integer manualOrder4 = builder4.getManualRank();
        assertNotNull(manualOrder4);
        assertEquals(3, manualOrder4.intValue()); // SAVE_PASSWORDS_PROMO is at index 3

        EducationalTipModuleBuilder builder5 =
                new EducationalTipModuleBuilder(ModuleType.PASSWORD_CHECKUP_PROMO, mActionDelegate);
        Integer manualOrder5 = builder5.getManualRank();
        assertNotNull(manualOrder5);
        assertEquals(4, manualOrder5.intValue()); // PASSWORD_CHECKUP_PROMO is at index 4
    }

    @Test
    @SmallTest
    public void testGetManualRank_ReturnsEmptyForNonSetupListModuleWhenActive() {
        when(mSetupListManager.isSetupListActive()).thenReturn(true);

        EducationalTipModuleBuilder builder =
                new EducationalTipModuleBuilder(ModuleType.QUICK_DELETE_PROMO, mActionDelegate);
        assertNull(builder.getManualRank());
    }

    @Test
    @SmallTest
    public void testGetManualRank_ReturnsEmptyWhenSetupListInactive() {
        EducationalTipModuleBuilder builder =
                new EducationalTipModuleBuilder(
                        ModuleType.ENHANCED_SAFE_BROWSING_PROMO, mActionDelegate);
        assertNull(builder.getManualRank());
    }
}
