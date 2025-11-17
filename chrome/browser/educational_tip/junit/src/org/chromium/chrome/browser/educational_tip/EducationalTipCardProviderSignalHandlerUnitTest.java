// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Context;

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

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
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
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Unit tests for {@link EducationalTipCardProviderSignalHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EducationalTipCardProviderSignalHandlerUnitTest {
    private static final String SYNC_ID = "sync_id";
    private static final @TabId int TAB_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private Tracker mTracker;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilter mNormalFilter;
    @Mock private TabGroupModelFilter mIncognitoFilter;
    @Mock private TabModel mNormalModel;
    @Mock private TabModel mIncognitoModel;
    @Mock private TabGroupModelFilterProvider mProvider;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mMockTabGroupSyncService;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(mContext);
        when(mActionDelegate.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getTabGroupModelFilterProvider()).thenReturn(mProvider);
        when(mProvider.getTabGroupModelFilter(/* isIncognito= */ false)).thenReturn(mNormalFilter);
        when(mProvider.getTabGroupModelFilter(/* isIncognito= */ true))
                .thenReturn(mIncognitoFilter);
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mNormalModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoModel);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        TabGroupSyncServiceFactory.setForTesting(mMockTabGroupSyncService);
        TrackerFactory.setTrackerForTests(mTracker);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.EDUCATIONAL_TIP_MODULE,
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2
    })
    public void testCreateInputContext_DefaultBrowserPromoCard() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        InputContext inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(3, inputContext.getSizeForTesting());

        // Test signal "should_show_non_role_manager_default_browser_promo".
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(mContext))
                .thenReturn(true);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(
                1,
                inputContext.getEntryValue("should_show_non_role_manager_default_browser_promo")
                        .floatValue,
                0.01);

        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(mContext))
                .thenReturn(false);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(
                0,
                inputContext.getEntryValue("should_show_non_role_manager_default_browser_promo")
                        .floatValue,
                0.01);

        // Test signal "has_default_browser_promo_shown_in_other_surface".
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(
                0,
                inputContext.getEntryValue("has_default_browser_promo_shown_in_other_surface")
                        .floatValue,
                0.01);

        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(false);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(
                1,
                inputContext.getEntryValue("has_default_browser_promo_shown_in_other_surface")
                        .floatValue,
                0.01);

        when(mTracker.isInitialized()).thenReturn(false);
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(
                0,
                inputContext.getEntryValue("has_default_browser_promo_shown_in_other_surface")
                        .floatValue,
                0.01);

        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP,
                        TimeUtils.currentTimeMillis());
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(
                1,
                inputContext.getEntryValue("has_default_browser_promo_shown_in_other_surface")
                        .floatValue,
                0.01);

        // Test signal "is_user_signed_in".
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(0, inputContext.getEntryValue("is_user_signed_in").floatValue, 0.01);

        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.DEFAULT_BROWSER_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(1, inputContext.getEntryValue("is_user_signed_in").floatValue, 0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testCreateInputContext_TabGroupPromoCard_TabGroupExists() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isReparentingInProgress()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);

        InputContext inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(3, inputContext.getSizeForTesting());

        when(mNormalFilter.getTabGroupCount()).thenReturn(0);
        when(mIncognitoFilter.getTabGroupCount()).thenReturn(0);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(0, inputContext.getEntryValue("tab_group_exists").floatValue, 0.01);

        when(mNormalFilter.getTabGroupCount()).thenReturn(5);
        when(mIncognitoFilter.getTabGroupCount()).thenReturn(6);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(1, inputContext.getEntryValue("tab_group_exists").floatValue, 0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testCreateInputContext_TabGroupPromoCard_NumberOfTabs() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());

        InputContext inputContext;

        // Test cases when tab state is already initialized.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isReparentingInProgress()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(0);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(0, inputContext.getEntryValue("number_of_tabs").floatValue, 0.01);

        when(mNormalModel.getCount()).thenReturn(5);
        when(mIncognitoModel.getCount()).thenReturn(0);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(5, inputContext.getEntryValue("number_of_tabs").floatValue, 0.01);

        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(10);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(10, inputContext.getEntryValue("number_of_tabs").floatValue, 0.01);

        when(mNormalModel.getCount()).thenReturn(10);
        when(mIncognitoModel.getCount()).thenReturn(10);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(20, inputContext.getEntryValue("number_of_tabs").floatValue, 0.01);

        // Test cases when tab state is not initialized.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        when(mActionDelegate.getTabCountForRelaunchFromSharedPrefs()).thenReturn(10);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(10, inputContext.getEntryValue("number_of_tabs").floatValue, 0.01);

        when(mActionDelegate.getTabCountForRelaunchFromSharedPrefs()).thenReturn(15);
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(15, inputContext.getEntryValue("number_of_tabs").floatValue, 0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EDUCATIONAL_TIP_MODULE})
    public void testCreateInputContext_TabGroupSyncPromoCard() {
        assertTrue(ChromeFeatureList.sEducationalTipModule.isEnabled());
        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});

        InputContext inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_SYNC_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(2, inputContext.getSizeForTesting());

        // Test signal "synced_tab_group_exists".
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_SYNC_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(1, inputContext.getEntryValue("synced_tab_group_exists").floatValue, 0.01);

        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_SYNC_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(0, inputContext.getEntryValue("synced_tab_group_exists").floatValue, 0.01);

        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        inputContext =
                EducationalTipCardProviderSignalHandler.createInputContext(
                        ModuleType.TAB_GROUP_SYNC_PROMO, mActionDelegate, mProfile, mTracker);
        assertEquals(0, inputContext.getEntryValue("synced_tab_group_exists").floatValue, 0.01);
    }
}
