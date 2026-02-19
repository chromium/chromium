// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceClientAndroid;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.regional_capabilities.RegionalProgram;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Test relating to {@link SetupListManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
@Features.EnableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
public class SetupListManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingBridgeJni;
    @Mock private RegionalCapabilitiesServiceClientAndroid mRegionalServiceClient;
    @Mock private SyncService mSyncService;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperJni;
    @Mock private SearchEngineChoiceService mSearchEngineChoiceService;

    private SharedPreferencesManager mSharedPreferencesManager;
    private static final long ONE_MINUTE_IN_MILLIS = TimeUnit.MINUTES.toMillis(1);

    @Before
    public void setUp() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        SetupListModuleUtils.resetAllModuleCompletionForTesting();
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP);

        RegionalCapabilitiesServiceClientAndroid.setInstanceForTests(mRegionalServiceClient);
        when(mRegionalServiceClient.getDeviceProgram()).thenReturn(RegionalProgram.DEFAULT);

        SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
        when(mSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(false);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);

        SafeBrowsingBridgeJni.setInstanceForTesting(mSafeBrowsingBridgeJni);
        when(mSafeBrowsingBridgeJni.getSafeBrowsingState(any()))
                .thenReturn(SafeBrowsingState.STANDARD_PROTECTION);

        SyncServiceFactory.setInstanceForTesting(mSyncService);
        PasswordManagerHelperJni.setInstanceForTesting(mPasswordManagerHelperJni);
    }

    @Test
    @SmallTest
    public void testEligibility_DefaultBrowser() {
        // Case 1: Ineligible (Search Engine Choice Suppression)
        when(mSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(true);
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.DEFAULT_BROWSER_PROMO));

        // Case 2: Eligible
        when(mSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(false);
        manager.maybePrimeCompletionStatus(mProfile);
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.DEFAULT_BROWSER_PROMO));
    }

    @Test
    @SmallTest
    public void testEligibility_AccountDependentPromos() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();

        // Initially signed out: Save Passwords and Password Checkup should be hidden.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        manager.maybePrimeCompletionStatus(mProfile);
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.PASSWORD_CHECKUP_PROMO));

        // Sign in: Save Passwords and History Sync should appear. Password Checkup needs sync.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mPasswordManagerHelperJni.hasChosenToSyncPasswords(any())).thenReturn(false);
        manager.maybePrimeCompletionStatus(mProfile);
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.HISTORY_SYNC_PROMO));
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.PASSWORD_CHECKUP_PROMO));

        // Enable Password Sync: Password Checkup should now appear.
        when(mPasswordManagerHelperJni.hasChosenToSyncPasswords(any())).thenReturn(true);
        manager.maybePrimeCompletionStatus(mProfile);
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.HISTORY_SYNC_PROMO));
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.PASSWORD_CHECKUP_PROMO));

        // Sign out: All should disappear.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        manager.maybePrimeCompletionStatus(mProfile);
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.HISTORY_SYNC_PROMO));
        assertFalse(manager.getRankedModuleTypes().contains(ModuleType.PASSWORD_CHECKUP_PROMO));
    }

    @Test
    @SmallTest
    public void testPriming_SignInCompletedInSystem() {
        // Mock user as signed out initially.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        // Sign in promo should be eligible and NOT completed.
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.SIGN_IN_PROMO));
        assertFalse(manager.isModuleCompleted(ModuleType.SIGN_IN_PROMO));

        // Mock user as signed in.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);

        // Re-prime status.
        manager.maybePrimeCompletionStatus(mProfile);

        // Sign in should now be identified as completed via priming.
        assertTrue(manager.isModuleCompleted(ModuleType.SIGN_IN_PROMO));
    }

    @Test
    @SmallTest
    public void testPriming_HistorySyncCompletedInSystem() {
        // Mock user as signed in but with history sync NOT yet completed in system.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of());

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        // History sync should be eligible and NOT completed.
        assertTrue(manager.getRankedModuleTypes().contains(ModuleType.HISTORY_SYNC_PROMO));
        assertFalse(manager.isModuleCompleted(ModuleType.HISTORY_SYNC_PROMO));

        // Mock history and tabs sync as completed in the system.
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));

        // Re-prime the status.
        manager.maybePrimeCompletionStatus(mProfile);

        // History sync should now be identified as completed via the priming logic.
        assertTrue(manager.isModuleCompleted(ModuleType.HISTORY_SYNC_PROMO));

        // Because it's completed silently during priming, it shouldn't be awaiting animation.
        assertFalse(manager.isModuleAwaitingCompletionAnimation(ModuleType.HISTORY_SYNC_PROMO));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
    public void testSetupList_ReturnFalseWhenFeatureDisabled() {
        // Re-create instance after feature flag is disabled.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
        assertFalse(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));
    }

    @Test
    @SmallTest
    public void testSetupList_ReturnFalseDuringFirstRun() {
        FirstRunStatus.setFirstRunTriggeredForTesting(true);
        // Re-create instance after FirstRunStatus is set.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
        assertFalse(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_TrueAndSetsTimestampWhenNotSet() {
        // Ensure the timestamp is not set initially.
        assertFalse(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));

        // Re-create instance.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
        // Check that the timestamp is now set.
        assertTrue(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));
        assertEquals(
                TimeUtils.currentTimeMillis(),
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP, -1L));
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsTrueWithinActiveWindow() {
        // Set the timestamp to be within the active window.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS - ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
    }

    @Test
    @SmallTest
    public void testTwoCellLayout_InActiveWithinThreeDays() {
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS - ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
    }

    @Test
    @SmallTest
    public void testTwoCellLayout_ActiveAfterThreeDays() {
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
        assertTrue(SetupListManager.getInstance().shouldShowTwoCellLayout());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseOutsideActiveWindow() {
        // Set the timestamp to be outside the active window.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
    }

    @Test
    @SmallTest
    public void testGetRankedModuleTypes_ReordersAfterAnimation() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile); // Ensure we have a primed list

        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertFalse("Ranked modules should not be empty", rankedModules.isEmpty());

        // Initially, pick the first item.
        int firstModuleType = rankedModules.get(0);
        String prefKey = SetupListModuleUtils.getCompletionKeyForModule(firstModuleType);

        // Mark the first item as completed.
        mSharedPreferencesManager.writeBoolean(prefKey, true);

        // Notify manager of the change.
        manager.onSharedPreferenceChanged(ContextUtils.getAppSharedPreferences(), prefKey);

        // The item should STILL be at its initial position because it's awaiting animation.
        assertEquals(firstModuleType, (int) manager.getRankedModuleTypes().get(0));
        assertEquals(0, (int) manager.getManualRank(firstModuleType));
        assertTrue(manager.isModuleAwaitingCompletionAnimation(firstModuleType));

        manager.onCompletionAnimationFinished(firstModuleType);

        // Now the item should be at the end of the list and its rank updated.
        rankedModules = manager.getRankedModuleTypes();
        int expectedRank = rankedModules.size() - 1;
        assertEquals(firstModuleType, (int) rankedModules.get(expectedRank));
        assertEquals(expectedRank, (int) manager.getManualRank(firstModuleType));
        assertFalse(manager.isModuleAwaitingCompletionAnimation(firstModuleType));
    }

    @Test
    @SmallTest
    public void testRefresh_MaxLimit() {
        // Mock 6 eligible promos.
        // SIGN_IN will be automatically detected as completed because user is signed in.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        // Ensure other account-dependent promos are eligible.
        when(mPasswordManagerHelperJni.hasChosenToSyncPasswords(any())).thenReturn(true);
        when(mRegionalServiceClient.getDeviceProgram()).thenReturn(RegionalProgram.DEFAULT);

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertEquals(SetupListManager.MAX_SETUP_LIST_ITEMS, rankedModules.size());

        // The SIGN_IN_PROMO should be pushed out of the Top set because there are 5 other
        // active eligible items that take priority.
        assertFalse(
                "Completed Sign In should be pushed out",
                rankedModules.contains(ModuleType.SIGN_IN_PROMO));
    }

    @Test
    @SmallTest
    public void testArm1_AddressBarFocus() {
        // Arm 1: Address Bar Focus (Address Bar enabled, PW Management disabled)
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.ANDROID_SETUP_LIST)
                .param(SetupListManager.ADDRESS_BAR_PLACEMENT_PARAM, true)
                .param(SetupListManager.PW_MANAGEMENT_PARAM, false)
                .apply();

        // Mock eligible promos.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mPasswordManagerHelperJni.hasChosenToSyncPasswords(any())).thenReturn(true);
        when(mSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(true);

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertTrue(rankedModules.contains(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO));
        assertFalse(rankedModules.contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertFalse(rankedModules.contains(ModuleType.PASSWORD_CHECKUP_PROMO));
    }

    @Test
    @SmallTest
    public void testArm2_PasswordCheckupFocus() {
        // Arm 2: PW Management Focus (Address Bar disabled, PW Management enabled)
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.ANDROID_SETUP_LIST)
                .param(SetupListManager.ADDRESS_BAR_PLACEMENT_PARAM, false)
                .param(SetupListManager.PW_MANAGEMENT_PARAM, true)
                .apply();

        // Mock eligible promos.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mPasswordManagerHelperJni.hasChosenToSyncPasswords(any())).thenReturn(true);
        when(mSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(true);

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertFalse(rankedModules.contains(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO));
        assertTrue(rankedModules.contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertTrue(rankedModules.contains(ModuleType.PASSWORD_CHECKUP_PROMO));
    }

    @Test
    @SmallTest
    public void testArm3_BothEnabled() {
        // Arm 3: Both enabled
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.ANDROID_SETUP_LIST)
                .param(SetupListManager.ADDRESS_BAR_PLACEMENT_PARAM, true)
                .param(SetupListManager.PW_MANAGEMENT_PARAM, true)
                .apply();

        // Mock eligible promos.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        when(mPasswordManagerHelperJni.hasChosenToSyncPasswords(any())).thenReturn(true);
        when(mSearchEngineChoiceService.isDefaultBrowserPromoSuppressed()).thenReturn(true);

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertTrue(rankedModules.contains(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO));
        assertTrue(rankedModules.contains(ModuleType.SAVE_PASSWORDS_PROMO));
        assertTrue(rankedModules.contains(ModuleType.PASSWORD_CHECKUP_PROMO));
    }
}
