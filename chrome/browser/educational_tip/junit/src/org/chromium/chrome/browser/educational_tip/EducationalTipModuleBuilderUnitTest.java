// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
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
    @Mock private ObservableSupplier<Profile> mProfileSupplier;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilterProvider mProvider;
    @Mock private TabGroupModelFilter mNormalFilter;
    @Mock private TabGroupModelFilter mIncognitoFilter;
    @Mock private TabModel mNormalModel;
    @Mock private TabModel mIncognitoModel;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManagerMock;

    private EducationalTipModuleBuilder mModuleBuilder;

    @Before
    public void setUp() {
        when(mActionDelegate.getProfileSupplier()).thenReturn(mProfileSupplier);
        when(mProfileSupplier.hasValue()).thenReturn(true);
        when(mProfileSupplier.get()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);
        when(mActionDelegate.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getTabGroupModelFilterProvider()).thenReturn(mProvider);
        when(mProvider.getTabGroupModelFilter(/* isIncognito= */ false)).thenReturn(mNormalFilter);
        when(mProvider.getTabGroupModelFilter(/* isIncognito= */ true))
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

        mModuleBuilder =
                new EducationalTipModuleBuilder(ModuleType.QUICK_DELETE_PROMO, mActionDelegate);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testBuildEducationalTipModule_NotEligible() {
        assertFalse(ChromeFeatureList.sEducationalTipModule.isEnabled());

        assertFalse(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback, never()).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.EDUCATIONAL_TIP_MODULE,
        ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER
    })
    public void testBuildEducationalTipModule_Eligible() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        assertTrue(mModuleBuilder.build(mModuleDelegate, mBuildCallback));
        verify(mBuildCallback).onResult(any(ModuleProvider.class));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.EDUCATIONAL_TIP_MODULE,
        ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER
    })
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
        ChromeFeatureList.EDUCATIONAL_TIP_MODULE,
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
    @EnableFeatures({
        ChromeFeatureList.EDUCATIONAL_TIP_MODULE,
        ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER
    })
    public void testCreateInputContext() {
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
}
