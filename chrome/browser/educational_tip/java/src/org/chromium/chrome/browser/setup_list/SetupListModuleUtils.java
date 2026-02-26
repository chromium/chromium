// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserStateProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Utilities for setup list modules. */
@NullMarked
public class SetupListModuleUtils {
    @VisibleForTesting
    public static final String HISTOGRAM_SETUP_LIST_PREFIX = "MagicStack.Clank.SetupList.";

    @Nullable private static List<Integer> sRankedModuleTypesForTesting;

    /**
     * Returns a ranked list of module types supported by the setup list. The order of modules in
     * this list defines their ranking, with lower indices indicating higher priority (e.g., index 0
     * is rank 1, index 1 is rank 2, etc.).
     */
    public static List<Integer> getRankedModuleTypes() {
        if (sRankedModuleTypesForTesting != null) {
            return sRankedModuleTypesForTesting;
        }
        return SetupListManager.getInstance().getRankedModuleTypes();
    }

    /** Returns the module type list for the two-cell container. */
    public static List<Integer> getTwoCellContainerModuleTypes() {
        return SetupListManager.getInstance().getTwoCellContainerModuleTypes();
    }

    /** Returns the list of module types to be registered with the framework. */
    public static List<Integer> getModuleTypesForRegistration(boolean showTwoCell) {
        List<Integer> modules = new ArrayList<>(SetupListManager.BASE_SETUP_LIST_ORDER);
        modules.addAll(getTwoCellContainerModuleTypes());
        modules.add(ModuleType.SETUP_LIST_CELEBRATORY_PROMO);
        return modules;
    }

    /** Returns whether the setup list is active based on the 14-day window. */
    public static boolean isSetupListActive() {
        return SetupListManager.getInstance().isSetupListActive();
    }

    /** Returns whether the two-cell layout should be shown based on the 3-day window. */
    public static boolean shouldShowTwoCellLayout() {
        return SetupListManager.getInstance().shouldShowTwoCellLayout();
    }

    /**
     * Returns the manual rank for the given module type, or null if it shouldn't be manually
     * ranked.
     */
    @Nullable
    public static Integer getManualRank(@ModuleType int moduleType) {
        return SetupListManager.getInstance().getManualRank(moduleType);
    }

    /** Returns whether the module type belongs to the currently active setup list view. */
    public static boolean isSetupListModule(@ModuleType int moduleType) {
        return SetupListManager.getInstance().isSetupListModule(moduleType);
    }

    /** Returns whether the given module is completed, using the optimized manager state. */
    public static boolean isModuleCompleted(@ModuleType int moduleType) {
        return SetupListManager.getInstance().isModuleCompleted(moduleType);
    }

    /** Returns whether the given module is eligible, using the optimized manager state. */
    public static boolean isModuleEligible(@ModuleType int moduleType) {
        return SetupListManager.getInstance().isModuleEligible(moduleType);
    }

    /**
     * Marks the given module type as completed by setting its individual boolean preference key.
     * The {@link SetupListManager} will observe this change and update the ranking automatically.
     *
     * @param moduleType The module to mark as completed.
     * @param silent Whether to bypass the completion animation. If true, the module is reordered
     *     immediately.
     */
    public static void setModuleCompleted(@ModuleType int moduleType, boolean silent) {
        SetupListManager.getInstance().setModuleCompleted(moduleType, silent);
    }

    @Nullable
    public static String getCompletionKeyForModule(@ModuleType int type) {
        if (SetupListManager.isBaseSetupListModule(type)
                || type == ModuleType.SETUP_LIST_CELEBRATORY_PROMO) {
            return ChromePreferenceKeys.SETUP_LIST_COMPLETED_KEY_PREFIX.createKey(
                    String.valueOf(type));
        }
        return null;
    }

    /** Returns whether the module is awaiting its completion animation. */
    public static boolean isModuleAwaitingCompletionAnimation(@ModuleType int moduleType) {
        return SetupListManager.getInstance().isModuleAwaitingCompletionAnimation(moduleType);
    }

    /** Signals that the completion animation for a module has finished. */
    public static void finishCompletionAnimation(@ModuleType int moduleType) {
        SetupListManager.getInstance().onCompletionAnimationFinished(moduleType);
    }

    /**
     * Checks the actual status of tasks that have external state (e.g. Sign In, Enhanced Safe
     * Browsing) based on the system state.
     */
    public static boolean checkIsTaskCompletedInSystem(
            @ModuleType int moduleType, Profile profile) {
        switch (moduleType) {
            case ModuleType.DEFAULT_BROWSER_PROMO:
                return !new DefaultBrowserStateProvider().shouldShowPromo();
            case ModuleType.SIGN_IN_PROMO:
                IdentityManager identityManager =
                        IdentityServicesProvider.get().getIdentityManager(profile);
                return identityManager != null
                        && identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
            case ModuleType.ENHANCED_SAFE_BROWSING_PROMO:
                return new SafeBrowsingBridge(profile).getSafeBrowsingState()
                        == SafeBrowsingState.ENHANCED_PROTECTION;
            case ModuleType.HISTORY_SYNC_PROMO:
                SyncService syncService = SyncServiceFactory.getForProfile(profile);
                return syncService != null
                        && syncService
                                .getSelectedTypes()
                                .containsAll(
                                        Set.of(
                                                UserSelectableType.HISTORY,
                                                UserSelectableType.TABS));
            default:
                return false;
        }
    }

    /** Resets the completion status of all Setup List modules to incomplete for testing. */
    public static void resetAllModuleCompletionForTesting() {
        List<Integer> modules = new ArrayList<>(SetupListManager.BASE_SETUP_LIST_ORDER);
        modules.add(ModuleType.SETUP_LIST_CELEBRATORY_PROMO);
        for (int moduleType : modules) {
            String individualPrefKey = getCompletionKeyForModule(moduleType);
            if (individualPrefKey != null) {
                ChromeSharedPreferences.getInstance().writeBoolean(individualPrefKey, false);
            }
        }
    }

    public static void setRankedModuleTypesForTesting(List<Integer> rankedModuleTypes) {
        sRankedModuleTypesForTesting = rankedModuleTypes;
    }

    /**
     * Updates an {@link ImageView}'s resource with a fade-out then fade-in animation.
     *
     * @param imageView The ImageView to animate.
     * @param iconResId The new image resource ID.
     */
    public static void updateIconWithAnimation(ImageView imageView, int iconResId) {
        int duration = SetupListManager.STRIKETHROUGH_DURATION_MS / 2;
        imageView.animate().cancel();
        imageView.setAlpha(1f);
        imageView
                .animate()
                .alpha(0.5f)
                .setDuration(duration)
                .withEndAction(
                        () -> {
                            imageView.setImageResource(iconResId);
                            imageView.animate().alpha(1f).setDuration(duration).start();
                        })
                .start();
    }

    /** Records a feature-level impression for the Setup List. */
    public static void recordSetupListImpression() {
        RecordUserAction.record("MobileNTP.SetupList.Impression");
    }

    /** Records a feature-level click for the Setup List. */
    public static void recordSetupListClick() {
        RecordUserAction.record("MobileNTP.SetupList.Click");
    }

    /**
     * Records an impression for a specific Setup List item.
     *
     * @param moduleType The type of Setup List item.
     * @param isCompleted Whether the item is in the completed state.
     */
    public static void recordSetupListItemImpression(
            @ModuleType int moduleType, boolean isCompleted) {
        String name =
                HISTOGRAM_SETUP_LIST_PREFIX
                        + "ItemImpression."
                        + (isCompleted ? "Completed" : "Active");
        RecordHistogram.recordEnumeratedHistogram(name, moduleType, ModuleType.NUM_ENTRIES);
    }

    /**
     * Records a click for a specific Setup List item.
     *
     * @param moduleType The type of Setup List item.
     */
    public static void recordSetupListItemClick(@ModuleType int moduleType) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_SETUP_LIST_PREFIX + "ItemClick.Active",
                moduleType,
                ModuleType.NUM_ENTRIES);
    }

    /**
     * Records the completion of a specific Setup List task.
     *
     * @param moduleType The type of Setup List task.
     */
    public static void recordSetupListItemCompletion(@ModuleType int moduleType) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_SETUP_LIST_PREFIX + "ItemCompletion", moduleType, ModuleType.NUM_ENTRIES);
    }
}
