// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

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
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
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
    public void testModuleCompletion_Silent() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        int moduleType = ModuleType.DEFAULT_BROWSER_PROMO;
        assertFalse(manager.isModuleCompleted(moduleType));
        assertTrue(
                manager.getRankedModuleTypes().indexOf(moduleType) == 0); // Should be at the start

        manager.setModuleCompleted(moduleType, /* silent= */ true);

        assertTrue(manager.isModuleCompleted(moduleType));
        assertFalse(manager.isModuleAwaitingCompletionAnimation(moduleType));
        // Check it moved to the end immediately
        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertEquals(moduleType, (int) rankedModules.get(rankedModules.size() - 1));

        // Test double write
        int oldSize = rankedModules.size();
        manager.setModuleCompleted(moduleType, /* silent= */ true);
        assertEquals(oldSize, manager.getRankedModuleTypes().size());
    }

    @Test
    @SmallTest
    public void testModuleCompletion_WithAnimation() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        int moduleType = ModuleType.DEFAULT_BROWSER_PROMO;
        assertFalse(manager.isModuleCompleted(moduleType));
        assertTrue(manager.getRankedModuleTypes().indexOf(moduleType) == 0);

        // Mark for animation
        manager.setModuleCompleted(moduleType, /* silent= */ false);
        assertTrue(manager.isModuleCompleted(moduleType));
        assertTrue(manager.isModuleAwaitingCompletionAnimation(moduleType));
        assertEquals(moduleType, (int) manager.getRankedModuleTypes().get(0)); // Still at start

        // Finish animation
        manager.onCompletionAnimationFinished(moduleType);
        assertFalse(manager.isModuleAwaitingCompletionAnimation(moduleType));
        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertEquals(moduleType, (int) rankedModules.get(rankedModules.size() - 1)); // Moved to end

        // Test double write after animation
        int oldSize = rankedModules.size();
        manager.setModuleCompleted(moduleType, /* silent= */ false);
        assertEquals(oldSize, manager.getRankedModuleTypes().size());
        assertFalse(manager.isModuleAwaitingCompletionAnimation(moduleType));
    }

    @Test
    @SmallTest
    public void testMaybePrimeCompletionStatus() {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();

        assertFalse(manager.isModuleCompleted(ModuleType.SIGN_IN_PROMO));
        manager.maybePrimeCompletionStatus(mProfile);

        assertTrue(manager.isModuleCompleted(ModuleType.SIGN_IN_PROMO));
        assertFalse(
                manager.isModuleAwaitingCompletionAnimation(
                        ModuleType.SIGN_IN_PROMO)); // Silently completed
    }

    @Test
    @SmallTest
    public void testReconcileState_MaxLimit() {
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
    public void testGetManualRank_WithOffset() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        manager.maybePrimeCompletionStatus(mProfile);

        List<Integer> rankedModules = manager.getRankedModuleTypes();
        int firstModule = rankedModules.get(0);

        // Verify that the rank of the first item is equal to the offset.
        assertEquals(
                SetupListManager.SETUP_LIST_RANK_OFFSET, (int) manager.getManualRank(firstModule));

        // Verify that it returns null when inactive.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis() - SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS);
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertNull(SetupListManager.getInstance().getManualRank(firstModule));
    }

    @Test
    @SmallTest
    public void testCelebratoryPromo_ShownWhenAllBaseModulesCompleted() {
        // Mark all base modules as completed BEFORE creating the manager.
        for (int moduleType : SetupListManager.BASE_SETUP_LIST_ORDER) {
            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            if (prefKey != null) {
                mSharedPreferencesManager.writeBoolean(prefKey, true);
            }
        }

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();

        assertTrue(manager.isSetupListActive());
        assertTrue(manager.shouldShowCelebratoryPromo());
        List<Integer> rankedModules = manager.getRankedModuleTypes();
        assertEquals(1, rankedModules.size());
        assertEquals(ModuleType.SETUP_LIST_CELEBRATORY_PROMO, (int) rankedModules.get(0));
    }

    @Test
    @SmallTest
    public void testSetupListInactive_AfterCelebratoryPromoCompleted() {
        // 1. Mark all base modules as completed BEFORE creating the manager -> Celebration state.
        for (int moduleType : SetupListManager.BASE_SETUP_LIST_ORDER) {
            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            if (prefKey != null) {
                mSharedPreferencesManager.writeBoolean(prefKey, true);
            }
        }
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        assertTrue(manager.isSetupListActive());
        assertTrue(manager.shouldShowCelebratoryPromo());

        // 2. Mark celebratory promo as completed.
        String celebratoryKey =
                SetupListModuleUtils.getCompletionKeyForModule(
                        ModuleType.SETUP_LIST_CELEBRATORY_PROMO);
        mSharedPreferencesManager.writeBoolean(celebratoryKey, true);

        // 3. Reconcile -> Should become INACTIVE.
        manager.reconcileState();
        assertFalse(manager.isSetupListActive());
        assertFalse(manager.shouldShowCelebratoryPromo());
        assertTrue(manager.getRankedModuleTypes().isEmpty());
    }

    @Test
    @SmallTest
    public void testReconcileState_HandlesAllTransitions() {
        // Start: Fresh installation (after first run).
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();

        // Phase 1: SINGLE_CELL (0-3 days).
        assertTrue(manager.isSetupListActive());
        assertFalse(manager.shouldShowTwoCellLayout());
        assertFalse(manager.shouldShowCelebratoryPromo());

        // Phase 2: TWO_CELL (after 3 days).
        mFakeTime.advanceMillis(
                SetupListManager.TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        manager.reconcileState();
        assertTrue(manager.isSetupListActive());
        assertTrue(manager.shouldShowTwoCellLayout());

        // Phase 3: CELEBRATION (all tasks done).
        // Use a loop that also calls onCompletionAnimationFinished to simulate real-time
        // completion.
        for (int moduleType : SetupListManager.BASE_SETUP_LIST_ORDER) {
            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            mSharedPreferencesManager.writeBoolean(prefKey, true);
            manager.onCompletionAnimationFinished(moduleType);
        }
        assertTrue(manager.isSetupListActive());
        assertTrue(manager.shouldShowCelebratoryPromo());
        assertFalse(manager.shouldShowTwoCellLayout());

        // Phase 4: INACTIVE (celebration dismissed or window expired).
        String celebratoryKey =
                SetupListModuleUtils.getCompletionKeyForModule(
                        ModuleType.SETUP_LIST_CELEBRATORY_PROMO);
        mSharedPreferencesManager.writeBoolean(celebratoryKey, true);
        manager.reconcileState();
        assertFalse(manager.isSetupListActive());
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

    @Test
    @SmallTest
    public void testOnPrimaryAccountChanged_CompletesSignIn() {
        // 1. Setup: User is signed out.
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);

        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();

        // Capture the observer registered by the manager.
        ArgumentCaptor<IdentityManager.Observer> observerCaptor =
                ArgumentCaptor.forClass(IdentityManager.Observer.class);
        manager.maybePrimeCompletionStatus(mProfile);
        verify(mIdentityManager).addObserver(observerCaptor.capture());
        IdentityManager.Observer observer = observerCaptor.getValue();

        assertFalse(manager.isModuleCompleted(ModuleType.SIGN_IN_PROMO));

        // 2. Act: Trigger a sign-in event via the captured observer.
        PrimaryAccountChangeEvent event =
                new PrimaryAccountChangeEvent(
                        PrimaryAccountChangeEvent.Type.SET, ConsentLevel.SIGNIN);
        observer.onPrimaryAccountChanged(event);

        // 3. Assert: Sign-In should be completed and awaiting animation.
        assertTrue(manager.isModuleCompleted(ModuleType.SIGN_IN_PROMO));
        assertTrue(manager.isModuleAwaitingCompletionAnimation(ModuleType.SIGN_IN_PROMO));
    }

    @Test
    @SmallTest
    public void testObserver_NotifiedOnPrimaryAccountChanged() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        SetupListManager manager = SetupListManager.getInstance();
        SetupListManager.Observer observer = mock(SetupListManager.Observer.class);
        manager.addObserver(observer);

        // Sign-in event
        PrimaryAccountChangeEvent signInEvent =
                new PrimaryAccountChangeEvent(
                        PrimaryAccountChangeEvent.Type.SET, ConsentLevel.SIGNIN);
        manager.onPrimaryAccountChanged(signInEvent);
        verify(observer).onSetupListStateChanged();

        // Sign-out event
        PrimaryAccountChangeEvent signOutEvent =
                new PrimaryAccountChangeEvent(
                        PrimaryAccountChangeEvent.Type.CLEARED, ConsentLevel.SIGNIN);
        manager.onPrimaryAccountChanged(signOutEvent);
        verify(observer, times(2)).onSetupListStateChanged();
    }
}
