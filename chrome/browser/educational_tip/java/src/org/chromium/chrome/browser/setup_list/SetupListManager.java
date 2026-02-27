// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.firstrun.FirstRunStatus.isFirstRunTriggered;

import android.content.SharedPreferences;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
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
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.sync.SyncService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
public class SetupListManager
        implements SharedPreferences.OnSharedPreferenceChangeListener, IdentityManager.Observer {
    /** Interface for observing changes to the Setup List state. */
    public interface Observer {
        /** Called when the Setup List's state (eligibility, ranking, or layout) has changed. */
        void onSetupListStateChanged();
    }

    // TODO(crbug.com/469425754): Re-arrange the class in a more meaningful manner.
    /** Defines the mutually exclusive UI layouts for the Setup List. */
    @IntDef({
        SetupListActiveLayout.SINGLE_CELL,
        SetupListActiveLayout.TWO_CELL,
        SetupListActiveLayout.CELEBRATION,
        SetupListActiveLayout.INACTIVE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SetupListActiveLayout {
        int SINGLE_CELL = 0;
        int TWO_CELL = 1;
        int CELEBRATION = 2;
        int INACTIVE = 3;
    }

    private static class LazyHolder {
        private static final SetupListManager sInstance = new SetupListManager();
    }

    @Nullable private static SetupListManager sInstanceForTesting;

    /** Whether the user is within the 14-day active window since the list was first eligible. */
    private boolean mIsTimeWindowActive;

    /** Whether the 3-day threshold for the two-cell layout consolidation has been met. */
    private boolean mIsTwoCellLayoutAllowed;

    /** Delay constants for the completion animation. */
    public static final int STRIKETHROUGH_DURATION_MS = 500;

    public static final int HIDE_DURATION_MS = 600;

    /** Maximum number of items to show in the setup list. */
    public static final int MAX_SETUP_LIST_ITEMS = 5;

    /**
     * The rank offset for setup list items to ensure they are displayed below the single tab
     * resumption module (which takes rank 0) when the Setup List is active.
     */
    public static final int SETUP_LIST_RANK_OFFSET = 1;

    // Order of modules in the setup list.
    public static final List<Integer> BASE_SETUP_LIST_ORDER =
            Arrays.asList(
                    ModuleType.DEFAULT_BROWSER_PROMO,
                    ModuleType.SIGN_IN_PROMO,
                    ModuleType.HISTORY_SYNC_PROMO,
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
    private boolean mHasRegisteredIdentityObserver;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /** The current UI layout phase of the Setup List. */
    private @SetupListActiveLayout int mActiveLayout = SetupListActiveLayout.SINGLE_CELL;

    private static final @ModuleType int TWO_CELL_CONTAINER_MODULE_TYPE =
            ModuleType.SETUP_LIST_TWO_CELL_CONTAINER;

    private static final @ModuleType int CELEBRATORY_PROMO_MODULE_TYPE =
            ModuleType.SETUP_LIST_CELEBRATORY_PROMO;

    @VisibleForTesting
    static final long SETUP_LIST_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(14);

    @VisibleForTesting
    static final long TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(3);

    @VisibleForTesting
    static final String ADDRESS_BAR_PLACEMENT_PARAM = "include_address_bar_placement";

    @VisibleForTesting static final String PW_MANAGEMENT_PARAM = "include_pw_management";

    private static class ModulePartition {
        final List<Integer> mActiveModules = new ArrayList<>();
        final List<Integer> mCompletedModules = new ArrayList<>();
        final Set<String> mCompletedKeys = new HashSet<>();
        boolean mAreAllBaseModulesCompleted = true;
    }

    @VisibleForTesting
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    SetupListManager() {
        initializePrefMapping();
        reconcileState();

        if (mActiveLayout != SetupListActiveLayout.INACTIVE) {
            ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(this);
        }
    }

    private void initializePrefMapping() {
        for (int moduleType : BASE_SETUP_LIST_ORDER) {
            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            if (prefKey != null) {
                mKeyToModuleMap.put(prefKey, moduleType);
            }
        }

        // Include the celebratory promo in the listener map.
        String celebratoryKey =
                SetupListModuleUtils.getCompletionKeyForModule(CELEBRATORY_PROMO_MODULE_TYPE);
        if (celebratoryKey != null) {
            mKeyToModuleMap.put(celebratoryKey, CELEBRATORY_PROMO_MODULE_TYPE);
        }
    }

    /**
     * Re-evaluates the 14-day and 3-day time windows and updates the corresponding status flags.
     * Records the initial timestamp if it hasn't been set yet.
     */
    private void updateTimeWindowStatus() {
        long setupListFirstShownTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP, -1L);

        if (setupListFirstShownTimestamp == -1L) {
            // If the timestamp is not set, this is the first time SetupListManager is
            // instantiated after the first run. Mark the list as active and record the
            // current time as the start of the 14-day window.
            mIsTimeWindowActive = true;
            mIsTwoCellLayoutAllowed = false;
            ChromeSharedPreferences.getInstance()
                    .writeLong(
                            ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                            TimeUtils.currentTimeMillis());
        } else {
            // If the setup list has been shown before, check if it's within the active window.
            long timeSinceFirstStart = TimeUtils.currentTimeMillis() - setupListFirstShownTimestamp;
            mIsTimeWindowActive = timeSinceFirstStart < SETUP_LIST_ACTIVE_WINDOW_MILLIS;
            mIsTwoCellLayoutAllowed =
                    mIsTimeWindowActive
                            && timeSinceFirstStart >= TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS;
        }
    }

    /** Transitions the Setup List to an inactive state and clears all ranked module data. */
    private void setInactive() {
        mActiveLayout = SetupListActiveLayout.INACTIVE;
        mRankedModules.clear();
        mModuleRankMap.clear();
        unregisterObservers();
    }

    private void unregisterObservers() {
        if (mHasRegisteredIdentityObserver && mProfile != null) {
            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(mProfile);
            if (identityManager != null) {
                identityManager.removeObserver(this);
            }
            mHasRegisteredIdentityObserver = false;
        }
    }

    private ModulePartition partitionModules() {
        ModulePartition partition = new ModulePartition();
        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();

        for (int moduleType : BASE_SETUP_LIST_ORDER) {
            // Filter out modules that are not eligible if the profile is available.
            if (mProfile != null && !checkIsModuleEligible(moduleType, mProfile)) {
                continue;
            }

            String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
            boolean isCompleted =
                    prefKey != null && chromeSharedPreferences.readBoolean(prefKey, false);

            if (isCompleted) {
                // If the module is completed but still awaiting its animation, keep it in the
                // active section to maintain its rank until the user sees the transition.
                if (mModulesAwaitingCompletionAnimation.contains(moduleType)) {
                    partition.mActiveModules.add(moduleType);
                    partition.mAreAllBaseModulesCompleted = false;
                } else {
                    partition.mCompletedModules.add(moduleType);
                }
                partition.mCompletedKeys.add(prefKey);
            } else {
                partition.mActiveModules.add(moduleType);
                partition.mAreAllBaseModulesCompleted = false;
            }
        }
        return partition;
    }

    /** Returns the singleton instance of SetupListManager. */
    public static SetupListManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.sInstance;
    }

    private boolean isSetupListAllowed() {
        return ChromeFeatureList.sAndroidSetupList.isEnabled() && !isFirstRunTriggered();
    }

    /**
     * Returns whether the setup list is active. It is active if we are within the 14-day window and
     * the celebratory promo hasn't been completed yet.
     */
    public boolean isSetupListActive() {
        return mActiveLayout != SetupListActiveLayout.INACTIVE;
    }

    /**
     * Returns whether the two-cell layout should be shown based on the active UI phase. This state
     * is determined during {@link #reconcileState()}.
     */
    public boolean shouldShowTwoCellLayout() {
        return mActiveLayout == SetupListActiveLayout.TWO_CELL;
    }

    /** Returns the module type list for the two-cell container. */
    public List<Integer> getTwoCellContainerModuleTypes() {
        return List.of(TWO_CELL_CONTAINER_MODULE_TYPE);
    }

    /** Returns the current pre-calculated ranked list of Setup List module types. */
    public List<Integer> getRankedModuleTypes() {
        return mRankedModules;
    }

    /**
     * Returns whether the celebratory promo should be shown based on the active UI phase. This
     * state is determined during {@link #reconcileState()}.
     */
    public boolean shouldShowCelebratoryPromo() {
        return mActiveLayout == SetupListActiveLayout.CELEBRATION;
    }

    /** Returns whether a module type is completed based on the in-memory state. */
    public boolean isModuleCompleted(@ModuleType int moduleType) {
        String prefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
        return prefKey != null && mCompletedKeys.contains(prefKey);
    }

    /**
     * Returns whether a module is eligible to be shown in the setup list based on the active UI
     * phase.
     */
    public boolean isModuleEligible(@ModuleType int moduleType) {
        switch (mActiveLayout) {
            case SetupListActiveLayout.CELEBRATION:
                return moduleType == CELEBRATORY_PROMO_MODULE_TYPE;
            case SetupListActiveLayout.TWO_CELL:
                return moduleType == TWO_CELL_CONTAINER_MODULE_TYPE;
            case SetupListActiveLayout.SINGLE_CELL:
                return mRankedModules.contains(moduleType);
            case SetupListActiveLayout.INACTIVE:
            default:
                return false;
        }
    }

    private boolean checkIsModuleEligible(@ModuleType int moduleType, @Nullable Profile profile) {
        if (moduleType == CELEBRATORY_PROMO_MODULE_TYPE) {
            return shouldShowCelebratoryPromo();
        }

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

        if (moduleType == ModuleType.HISTORY_SYNC_PROMO) {
            return isSignedIn;
        }

        if (moduleType == ModuleType.PASSWORD_CHECKUP_PROMO) {
            SyncService syncService = SyncServiceFactory.getForProfile(profile);
            return isPwManagementEnabled
                    && isSignedIn
                    && PasswordManagerHelper.hasChosenToSyncPasswords(syncService);
        }

        return false;
    }

    /** Returns whether a given module type is a setup list module. */
    public boolean isSetupListModule(@ModuleType int moduleType) {
        if (!isSetupListActive()) {
            return false;
        }
        return moduleType == TWO_CELL_CONTAINER_MODULE_TYPE
                || moduleType == CELEBRATORY_PROMO_MODULE_TYPE
                || isBaseSetupListModule(moduleType);
    }

    /** Returns whether the given module type is part of the base setup list. */
    public static boolean isBaseSetupListModule(@ModuleType int moduleType) {
        return BASE_SETUP_LIST_SET.contains(moduleType);
    }

    /**
     * Returns the manual rank for a given module type based on the active UI phase, or null if not
     * manually ranked in the current state.
     */
    @Nullable
    public Integer getManualRank(@ModuleType int moduleType) {
        if (!isSetupListActive()) {
            return null;
        }

        switch (mActiveLayout) {
            case SetupListActiveLayout.CELEBRATION:
                return moduleType == CELEBRATORY_PROMO_MODULE_TYPE ? SETUP_LIST_RANK_OFFSET : null;
            case SetupListActiveLayout.TWO_CELL:
                return moduleType == TWO_CELL_CONTAINER_MODULE_TYPE ? SETUP_LIST_RANK_OFFSET : null;
            case SetupListActiveLayout.SINGLE_CELL:
                Integer rank = mModuleRankMap.get(moduleType);
                return rank != null ? rank + SETUP_LIST_RANK_OFFSET : null;
            case SetupListActiveLayout.INACTIVE:
            default:
                return null;
        }
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
            setModuleCompleted(moduleType, /* silent= */ true);
        }
    }

    /** Adds an observer to be notified of Setup List state changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes a previously added observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        @PrimaryAccountChangeEvent.Type
        int eventType = eventDetails.getEventTypeFor(ConsentLevel.SIGNIN);
        if (eventType == PrimaryAccountChangeEvent.Type.SET) {
            setModuleCompleted(ModuleType.SIGN_IN_PROMO, /* silent= */ false);
        } else {
            reconcileState();
        }
        notifyStateChanged();
    }

    private void notifyStateChanged() {
        for (Observer observer : mObservers) {
            observer.onSetupListStateChanged();
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
        if (!isSetupListActive() || profile.isOffTheRecord()) {
            return;
        }

        if (mProfile != profile) {
            unregisterObservers();
            mProfile = profile;
        }

        if (!mHasRegisteredIdentityObserver) {
            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(mProfile);
            assertNonNull(identityManager);
            identityManager.addObserver(this);
            mHasRegisteredIdentityObserver = true;
        }

        for (int moduleType : BASE_SETUP_LIST_ORDER) {
            if (!isModuleCompleted(moduleType)
                    && SetupListModuleUtils.checkIsTaskCompletedInSystem(moduleType, profile)) {
                setModuleCompleted(moduleType, /* silent= */ true);
            }
        }
        reconcileState();
    }

    /**
     * Marks the given module type as completed by setting its individual boolean preference key.
     * This is the sole method responsible for changing the completion state.
     *
     * @param moduleType The module to mark as completed.
     * @param silent Whether to bypass the completion animation. If true, the module is reordered
     *     immediately without animation.
     */
    public void setModuleCompleted(@ModuleType int moduleType, boolean silent) {
        if (isModuleCompleted(moduleType)) return;
        String individualPrefKey = SetupListModuleUtils.getCompletionKeyForModule(moduleType);
        if (individualPrefKey == null) return;

        if (mCompletedKeys.contains(individualPrefKey)) return;

        mCompletedKeys.add(individualPrefKey);
        ChromeSharedPreferences.getInstance().writeBoolean(individualPrefKey, true);

        if (!silent) {
            mModulesAwaitingCompletionAnimation.add(moduleType);
        }
        reconcileState();

        SetupListModuleUtils.recordSetupListItemCompletion(moduleType);
    }

    /**
     * Orchestrates the Setup List state machine. Re-evaluates time windows, checks for feature
     * inactivity, and re-calculates the ranked list of modules and the appropriate UI layout phase.
     * This method is the single source of truth for the manager's state.
     */
    @VisibleForTesting
    void reconcileState() {
        if (!isSetupListAllowed()) {
            setInactive();
            return;
        }

        updateTimeWindowStatus();

        if (!mIsTimeWindowActive) {
            setInactive();
            return;
        }

        // Check if the celebratory promo has already been completed.
        String celebratoryKey =
                SetupListModuleUtils.getCompletionKeyForModule(CELEBRATORY_PROMO_MODULE_TYPE);
        if (celebratoryKey != null
                && ChromeSharedPreferences.getInstance().readBoolean(celebratoryKey, false)) {
            setInactive();
            return;
        }

        ModulePartition partition = partitionModules();
        mCompletedKeys = partition.mCompletedKeys;

        // Calculate the current active layout based on overall completion status.
        if (partition.mAreAllBaseModulesCompleted) {
            mActiveLayout = SetupListActiveLayout.CELEBRATION;
            updateRanking(new ArrayList<>(List.of(CELEBRATORY_PROMO_MODULE_TYPE)));
            return;
        }

        if (mIsTwoCellLayoutAllowed
                && (partition.mActiveModules.size() + partition.mCompletedModules.size()) >= 2) {
            mActiveLayout = SetupListActiveLayout.TWO_CELL;
        } else {
            mActiveLayout = SetupListActiveLayout.SINGLE_CELL;
        }

        List<Integer> combinedModules = new ArrayList<>(partition.mActiveModules);
        combinedModules.addAll(partition.mCompletedModules);
        updateRanking(combinedModules);
    }

    private void updateRanking(List<Integer> combinedModules) {
        // Limit the number of items to show.
        if (combinedModules.size() > MAX_SETUP_LIST_ITEMS) {
            combinedModules = combinedModules.subList(0, MAX_SETUP_LIST_ITEMS);
        }

        mRankedModules = combinedModules;

        // Update the rank map for fast lookup.
        mModuleRankMap.clear();
        for (int i = 0; i < combinedModules.size(); i++) {
            mModuleRankMap.put(combinedModules.get(i), i);
        }
    }

    /** Returns whether a module is awaiting its completion animation. */
    public boolean isModuleAwaitingCompletionAnimation(@ModuleType int moduleType) {
        return mModulesAwaitingCompletionAnimation.contains(moduleType);
    }

    /** Returns the set of all module types currently awaiting completion animation. */
    public Set<Integer> getModulesAwaitingCompletionAnimation() {
        return mModulesAwaitingCompletionAnimation;
    }

    /**
     * Called when the completion animation for a module has finished. This moves the module from
     * the active section to the completed section.
     */
    public void onCompletionAnimationFinished(@ModuleType int moduleType) {
        if (mModulesAwaitingCompletionAnimation.remove(moduleType)) {
            reconcileState();
        }
    }

    public static void setInstanceForTesting(@Nullable SetupListManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
