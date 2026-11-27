// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Unit tests for {@link EducationalTipCardProviderSignalHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class EducationalTipCardProviderSignalHandlerUnitTest {
    private static final @TabId int TAB_ID = 1;
    private static final float FLOAT_ASSERT_DELTA = 0.01f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EducationTipModuleActionDelegate mActionDelegate;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private Tracker mTracker;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mNormalModel;
    @Mock private TabModel mIncognitoModel;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mMockTabGroupSyncService;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private NtpCustomizationPolicyManager mNtpCustomizationPolicyManager;

    @Before
    public void setUp() {
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
        TabGroupSyncServiceFactory.setForTesting(mMockTabGroupSyncService);
        TrackerFactory.setTrackerForTests(mTracker);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(false);
        NtpCustomizationPolicyManager.setInstanceForTesting(mNtpCustomizationPolicyManager);
    }

    @Test
    public void testCreateInputContext_DefaultBrowserPromoCard() {
        Context context = ApplicationProvider.getApplicationContext();
        when(mActionDelegate.getContext()).thenReturn(context);
        assertEquals(3, createInputContext(ModuleType.DEFAULT_BROWSER_PROMO).getSizeForTesting());

        String nonRoleManagerSignal = "should_show_non_role_manager_default_browser_promo";
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(context)).thenReturn(true);
        assertFloatSignal(
                ModuleType.DEFAULT_BROWSER_PROMO, nonRoleManagerSignal, /* expected= */ 1);

        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(context))
                .thenReturn(false);
        assertFloatSignal(
                ModuleType.DEFAULT_BROWSER_PROMO, nonRoleManagerSignal, /* expected= */ 0);

        String promoShownSignal = "has_default_browser_promo_shown_in_other_surface";
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);
        assertFloatSignal(ModuleType.DEFAULT_BROWSER_PROMO, promoShownSignal, /* expected= */ 0);

        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(false);
        assertFloatSignal(ModuleType.DEFAULT_BROWSER_PROMO, promoShownSignal, /* expected= */ 1);

        when(mTracker.isInitialized()).thenReturn(false);
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP);
        assertFloatSignal(ModuleType.DEFAULT_BROWSER_PROMO, promoShownSignal, /* expected= */ 0);

        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.EDUCATIONAL_TIP_LAST_DEFAULT_BROWSER_PROMO_TIMESTAMP,
                        TimeUtils.currentTimeMillis());
        assertFloatSignal(ModuleType.DEFAULT_BROWSER_PROMO, promoShownSignal, /* expected= */ 1);

        String signedInSignal = "is_user_signed_in";
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(false);
        assertFloatSignal(ModuleType.DEFAULT_BROWSER_PROMO, signedInSignal, /* expected= */ 0);

        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(true);
        assertFloatSignal(ModuleType.DEFAULT_BROWSER_PROMO, signedInSignal, /* expected= */ 1);
    }

    @Test
    public void testCreateInputContext_TabGroupPromoCard_TabGroupExists() {
        String signalKey = "tab_group_exists";
        setupTabStateInitialized();
        assertEquals(3, createInputContext(ModuleType.TAB_GROUP_PROMO).getSizeForTesting());

        when(mNormalModel.getTabGroupCount()).thenReturn(0);
        when(mIncognitoModel.getTabGroupCount()).thenReturn(0);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 0);

        when(mNormalModel.getTabGroupCount()).thenReturn(5);
        when(mIncognitoModel.getTabGroupCount()).thenReturn(6);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 1);
    }

    @Test
    public void testCreateInputContext_TabGroupPromoCard_NumberOfTabs() {
        String signalKey = "number_of_tabs";
        setupTabStateInitialized();

        // Test cases when tab state is already initialized.
        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(0);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 0);

        when(mNormalModel.getCount()).thenReturn(5);
        when(mIncognitoModel.getCount()).thenReturn(0);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 5);

        when(mNormalModel.getCount()).thenReturn(0);
        when(mIncognitoModel.getCount()).thenReturn(10);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 10);

        when(mNormalModel.getCount()).thenReturn(10);
        when(mIncognitoModel.getCount()).thenReturn(10);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 20);

        // Test cases when tab state is not initialized.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        when(mActionDelegate.getTabCountForRelaunchFromPersistentStore()).thenReturn(10);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 10);

        when(mActionDelegate.getTabCountForRelaunchFromPersistentStore()).thenReturn(15);
        assertFloatSignal(ModuleType.TAB_GROUP_PROMO, signalKey, /* expected= */ 15);
    }

    @Test
    public void testCreateInputContext_TabGroupSyncPromoCard() {
        String signalKey = "synced_tab_group_exists";
        String syncId = "sync_id";
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {syncId});
        assertEquals(2, createInputContext(ModuleType.TAB_GROUP_SYNC_PROMO).getSizeForTesting());
        assertFloatSignal(ModuleType.TAB_GROUP_SYNC_PROMO, signalKey, /* expected= */ 1);

        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        assertFloatSignal(ModuleType.TAB_GROUP_SYNC_PROMO, signalKey, /* expected= */ 0);

        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {syncId});
        assertFloatSignal(ModuleType.TAB_GROUP_SYNC_PROMO, signalKey, /* expected= */ 0);
    }

    @Test
    public void testCreateInputContext_NtpThemePromoCard() {
        when(mNtpCustomizationPolicyManager.isNtpCustomBackgroundEnabled()).thenReturn(true);
        when(mActionDelegate.supportCustomizedNtpTheme()).thenReturn(true);
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.CHROME_COLOR);
        NtpCustomizationUtils.setThemeTipBottomSheetShownTimestampToSharedPreference(
                /* timestamp= */ 100L);
        assertNtpThemePromoSignals(/* expected= */ true);

        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        when(mActionDelegate.supportCustomizedNtpTheme()).thenReturn(false);
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(NtpBackgroundType.DEFAULT);
        assertNtpThemePromoSignals(/* expected= */ false);
    }

    @Test
    public void testCreateInputContext_TipsNotificationsPromoCard() {
        String signalKey = "is_eligible_to_tips_opt_in";
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE);
        assertEquals(
                2, createInputContext(ModuleType.TIPS_NOTIFICATIONS_PROMO).getSizeForTesting());
        assertFloatSignal(ModuleType.TIPS_NOTIFICATIONS_PROMO, signalKey, /* expected= */ 0);

        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE, /* value= */ true);
        assertFloatSignal(ModuleType.TIPS_NOTIFICATIONS_PROMO, signalKey, /* expected= */ 1);
    }

    @Test
    public void testCreateInputContext_FuseboxPromoCard() {
        String signalKey = "is_fusebox_eligible";
        when(mActionDelegate.isFuseboxGuidedTourEligible()).thenReturn(true);
        InputContext inputContext = createInputContext(ModuleType.FUSEBOX_PROMO);
        assertEquals(2, inputContext.getSizeForTesting());
        assertTrue(inputContext.getEntryValue(signalKey).booleanValue);

        when(mActionDelegate.isFuseboxGuidedTourEligible()).thenReturn(false);
        assertFalse(
                createInputContext(ModuleType.FUSEBOX_PROMO).getEntryValue(signalKey).booleanValue);
    }

    /** Creates an {@link InputContext} for the given {@code moduleType}. */
    private InputContext createInputContext(@ModuleType int moduleType) {
        return EducationalTipCardProviderSignalHandler.createInputContext(
                moduleType, mActionDelegate, mProfile, mTracker);
    }

    /** Asserts that the float signal for {@code signalKey} matches {@code expected}. */
    private void assertFloatSignal(@ModuleType int moduleType, String signalKey, float expected) {
        assertEquals(
                expected,
                createInputContext(moduleType).getEntryValue(signalKey).floatValue,
                FLOAT_ASSERT_DELTA);
    }

    /** Configures {@link #mTabModelSelector} as initialized with a valid current tab. */
    private void setupTabStateInitialized() {
        when(mActionDelegate.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mNormalModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isReparentingInProgress()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
    }

    /** Asserts all three boolean signals for {@link ModuleType#NTP_THEME_PROMO}. */
    private void assertNtpThemePromoSignals(boolean expected) {
        InputContext inputContext = createInputContext(ModuleType.NTP_THEME_PROMO);
        assertEquals(4, inputContext.getSizeForTesting());
        assertEquals(
                expected, inputContext.getEntryValue("support_customized_ntp_theme").booleanValue);
        assertEquals(
                expected, inputContext.getEntryValue("has_customized_ntp_background").booleanValue);
        assertEquals(
                expected,
                inputContext.getEntryValue("has_theme_tip_bottom_sheet_been_shown").booleanValue);
    }
}
