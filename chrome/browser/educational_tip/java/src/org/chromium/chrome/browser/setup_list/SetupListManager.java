// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.chromium.chrome.browser.firstrun.FirstRunStatus.isFirstRunTriggered;

import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Manages the state and logic for the Setup List feature. This class is responsible for determining
 * whether the Setup List is active and caching this state for the duration of the app session. It
 * also maintains the dynamic ranking of modules, moving completed items to the end.
 */
@NullMarked
public class SetupListManager implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static class LazyHolder {
        private static final SetupListManager sInstance = new SetupListManager();
    }

    @Nullable private static SetupListManager sInstanceForTesting;

    private final boolean mIsSetupListActive;
    private final boolean mShouldShowTwoCellLayout;
    private boolean mIsPriming;

    /** Delay constants for the completion animation. */
    public static final int STRIKETHROUGH_DURATION_MS = 500;

    public static final int HIDE_DURATION_MS = 600;

    /** Maximum number of items to show in the setup list. */
    public static final int MAX_SETUP_LIST_ITEMS = 5;

    // Order of modules in the setup list.
    public static final List<Integer> BASE_SETUP_LIST_ORDER =
            Arrays.asList(
                    ModuleType.DEFAULT_BROWSER_PROMO,
                    ModuleType.SIGN_IN_PROMO,
                    ModuleType.ENHANCED_SAFE_BROWSING_PROMO,
                    ModuleType.SAVE_PASSWORDS_PROMO,
                    ModuleType.PASSWORD_CHECKUP_PROMO,
                    ModuleType.ADDRESS_BAR_PLACEMENT_PROMO);
    private static final Set<Integer> BASE_SETUP_LIST_SET = new HashSet<>(BASE_SETUP_LIST_ORDER);

    private List<Integer> mRankedModules = new ArrayList<>();
    private final Map<Integer, Integer> mModuleRankMap = new HashMap<>();
    private final Map<String, Integer> mKeyToModuleMap = new HashMap<>();
    private Set<String> mCompletedKeys = new HashSet<>();
    private @Nullable Profile mProfile;
    private final Set<Integer> mModulesAwaitingCompletionAnimation = new HashSet<>();

    private static final @ModuleType int TWO_CELL_CONTAINER_MODULE_TYPE =
            ModuleType.SETUP_LIST_TWO_CELL_CONTAINER;

    @VisibleForTesting
    static final long SETUP_LIST_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(14);

    @VisibleForTesting
    static final long TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(3);

    @VisibleForTesting
    static final String ADDRESS_BAR_PLACEMENT_PARAM = "include_address_bar_placement";

    @VisibleForTesting static final String PW_MANAGEMENT_PARAM = "include_pw_management";

    @VisibleForTesting
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    SetupListManager() {
        if (!ChromeFeatureList.sAndroidSetupList.isEnabled() || isFirstRunTriggered()) {
            // Only enabled from the second run onwards.
            mIsSetupListActive = false;
            mShouldShowTwoCellLayout = false;
            return;
        }

        for (int moduleType : BASE_SETUP_LIST_ORDER) {
            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            if (prefKey != null) {
                mKeyToModuleMap.put(prefKey, moduleType);
            }
        }

        long setupListFirstShownTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP, -1L);

        if (setupListFirstShownTimestamp != -1L) {
            // If the setup list has been shown before, check if it's within the active window.
            long timeSinceFirstStart = TimeUtils.currentTimeMillis() - setupListFirstShownTimestamp;
            mIsSetupListActive = timeSinceFirstStart < SETUP_LIST_ACTIVE_WINDOW_MILLIS;
            mShouldShowTwoCellLayout =
                    mIsSetupListActive
                            && timeSinceFirstStart >= TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS;
        } else {
            // If the timestamp is not set, this is the first time SetupListManager is
            // instantiated after the first run. Mark the list as active and record the
            // current time as the start of the 14-day window.
            mIsSetupListActive = true;
            mShouldShowTwoCellLayout = false;
            ChromeSharedPreferences.getInstance()
                    .writeLong(
                            ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                            TimeUtils.currentTimeMillis());
        }

        if (mIsSetupListActive) {
            refreshRankedModules();
            ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(this);
        }
    }

    /** Returns the singleton instance of SetupListManager. */
    public static SetupListManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.sInstance;
    }

    /** Returns whether the setup list is active based on the 14-day window. */
    public boolean isSetupListActive() {
        return mIsSetupListActive;
    }

    /** Returns whether the two-cell layout should be shown. This is cached for the session. */
    public boolean shouldShowTwoCellLayout() {
        return mShouldShowTwoCellLayout && mRankedModules.size() >= 2;
    }

    /** Returns the module type list for the two-cell container. */
    public List<Integer> getTwoCellContainerModuleTypes() {
        return List.of(TWO_CELL_CONTAINER_MODULE_TYPE);
    }

    /** Returns the current pre-calculated ranked list of Setup List module types. */
    public List<Integer> getRankedModuleTypes() {
        return mRankedModules;
    }

    /** Returns whether a module type is completed based on the in-memory state. */
    public boolean isModuleCompleted(@ModuleType int moduleType) {
        String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
        return prefKey != null && mCompletedKeys.contains(prefKey);
    }

    /** Returns whether a module is eligible to be shown in the setup list. */
    public boolean isModuleEligible(@ModuleType int moduleType) {
        return mRankedModules.contains(moduleType);
    }

    private boolean checkIsModuleEligible(@ModuleType int moduleType, @Nullable Profile profile) {
        if (moduleType == ModuleType.DEFAULT_BROWSER_PROMO) {
            return !SearchEngineChoiceService.getInstance().isDefaultBrowserPromoSuppressed();
        }

        if (moduleType == ModuleType.SIGN_IN_PROMO
                || moduleType == ModuleType.ENHANCED_SAFE_BROWSING_PROMO) {
            return true;
        }

        if (moduleType == ModuleType.ADDRESS_BAR_PLACEMENT_PROMO) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.ANDROID_SETUP_LIST, ADDRESS_BAR_PLACEMENT_PARAM, true);
        }

        // For modules that require a profile or account state.
        if (profile == null) {
            return false;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        boolean isSignedIn =
                identityManager != null && identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);

        boolean isPwManagementEnabled =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.ANDROID_SETUP_LIST, PW_MANAGEMENT_PARAM, true);

        if (moduleType == ModuleType.SAVE_PASSWORDS_PROMO) {
            return isPwManagementEnabled && isSignedIn;
        }

        if (moduleType == ModuleType.PASSWORD_CHECKUP_PROMO) {
            SyncService syncService = SyncServiceFactory.getForProfile(profile);
            return isPwManagementEnabled
                    && isSignedIn
                    && PasswordManagerHelper.hasChosenToSyncPasswords(syncService);
        }

        return false;
    }

    /** Returns whether the module type belongs to the currently active setup list view. */
    public boolean isSetupListModule(@ModuleType int moduleType) {
        if (!isSetupListActive()) {
            return false;
        }
        return moduleType == TWO_CELL_CONTAINER_MODULE_TYPE || isBaseSetupListModule(moduleType);
    }

    /** Returns whether the given module type is part of the base setup list. */
    public static boolean isBaseSetupListModule(@ModuleType int moduleType) {
        return BASE_SETUP_LIST_SET.contains(moduleType);
    }

    /**
     * Returns the manual rank for a given module type from the pre-calculated rank map, or null if
     * not applicable.
     */
    @Nullable
    public Integer getManualRank(@ModuleType int moduleType) {
        if (!isSetupListActive()) {
            return null;
        }

        if (shouldShowTwoCellLayout() && moduleType == TWO_CELL_CONTAINER_MODULE_TYPE) {
            return 0;
        }

        return mModuleRankMap.get(moduleType);
    }

    /**
     * Reacts to preference changes to incrementally update the in-memory ranked module list. This
     * ensures that completed items are moved to the end of the list immediately.
     */
    @Override
    public void onSharedPreferenceChanged(SharedPreferences prefs, @Nullable String key) {
        if (key == null) return;

        Integer moduleType = mKeyToModuleMap.get(key);
        if (moduleType != null) {
            // If the preference change is for a module being marked as completed, add it to the
            // awaiting animation set.
            if (!mIsPriming && prefs.getBoolean(key, false)) {
                mModulesAwaitingCompletionAnimation.add(moduleType);
            }
            refreshRankedModules();
        }
    }

    /**
     * Checks the actual status of tasks that have external state (e.g. Sign In, Enhanced Safe
     * Browsing) and updates the completion preferences if they are already finished. This should be
     * called before the first ranking to avoid late detection of completed tasks.
     *
     * @param profile The regular profile to check status for.
     */
    public void maybePrimeCompletionStatus(Profile profile) {
        if (!mIsSetupListActive || profile.isOffTheRecord()) {
            return;
        }

        mProfile = profile;
        mIsPriming = true;

        for (int moduleType : BASE_SETUP_LIST_ORDER) {
            if (!isModuleCompleted(moduleType)
                    && SetupListModuleUtils.checkIsTaskCompletedInSystem(moduleType, profile)) {
                SetupListModuleUtils.setModuleCompleted(moduleType);
            }
        }

        mIsPriming = false;
        refreshRankedModules();
    }

    /**
     * Re-calculates the ranked list of modules by partitioning the base order based on completion
     * status. Completed items are moved to the end. The results are cached in memory for fast
     * access by the UI and Magic Stack.
     */
    @VisibleForTesting
    void refreshRankedModules() {
        List<Integer> activeModules = new ArrayList<>();
        List<Integer> completedModules = new ArrayList<>();
        Set<String> completedKeys = new HashSet<>();

        // Partition the base order based on individual boolean completion keys.
        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        for (int moduleType : BASE_SETUP_LIST_ORDER) {
            // Filter out modules that are not eligible if the profile is available.
            if (mProfile != null && !checkIsModuleEligible(moduleType, mProfile)) {
                continue;
            }

            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            if (prefKey != null && chromeSharedPreferences.readBoolean(prefKey, false)) {
                // If the module is completed but still awaiting its animation, keep it in the
                // active section to maintain its rank until the user sees the transition.
                if (mModulesAwaitingCompletionAnimation.contains(moduleType)) {
                    activeModules.add(moduleType);
                } else {
                    completedModules.add(moduleType);
                }
                completedKeys.add(prefKey);
            } else {
                activeModules.add(moduleType);
            }
        }

        List<Integer> combined = new ArrayList<>(activeModules);
        combined.addAll(completedModules);

        // Limit the number of items to show.
        if (combined.size() > MAX_SETUP_LIST_ITEMS) {
            combined = combined.subList(0, MAX_SETUP_LIST_ITEMS);
        }

        mRankedModules = combined;
        mCompletedKeys = completedKeys;

        // Update the rank map for fast lookup.
        mModuleRankMap.clear();
        for (int i = 0; i < combined.size(); i++) {
            mModuleRankMap.put(combined.get(i), i);
        }
    }

    /** Returns whether a module is awaiting its completion animation. */
    public boolean isModuleAwaitingCompletionAnimation(@ModuleType int moduleType) {
        return mModulesAwaitingCompletionAnimation.contains(moduleType);
    }

    /**
     * Called when the completion animation for a module has finished. This moves the module from
     * the active section to the completed section.
     */
    public void onCompletionAnimationFinished(@ModuleType int moduleType) {
        if (mModulesAwaitingCompletionAnimation.remove(moduleType)) {
            refreshRankedModules();
        }
    }

    public static void setInstanceForTesting(@Nullable SetupListManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
