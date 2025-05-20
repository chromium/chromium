// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Provides information about the signals of cards in the educational tip module. */
@NullMarked
public class EducationalTipCardProviderSignalHandler {
    /** Creates an instance of InputContext. */
    @VisibleForTesting
    static InputContext createInputContext(
            @ModuleType int moduleType,
            EducationTipModuleActionDelegate actionDelegate,
            Profile profile,
            Tracker tracker) {
        InputContext inputContext = new InputContext();
        inputContext.addEntry(
                "is_user_signed_in", ProcessedValue.fromFloat(isUserSignedIn(profile)));
        switch (moduleType) {
            case ModuleType.DEFAULT_BROWSER_PROMO:
                inputContext.addEntry(
                        "should_show_non_role_manager_default_browser_promo",
                        ProcessedValue.fromFloat(
                                shouldShowNonRoleManagerDefaultBrowserPromo(actionDelegate)));
                inputContext.addEntry(
                        "has_default_browser_promo_shown_in_other_surface",
                        ProcessedValue.fromFloat(
                                hasDefaultBrowserPromoShownInOtherSurface(tracker)));
                return inputContext;
            case ModuleType.TAB_GROUP_PROMO:
                inputContext.addEntry(
                        "tab_group_exists",
                        ProcessedValue.fromFloat(tabGroupExists(actionDelegate)));
                inputContext.addEntry(
                        "number_of_tabs",
                        ProcessedValue.fromFloat(getCurrentTabCount(actionDelegate)));
                return inputContext;
            case ModuleType.TAB_GROUP_SYNC_PROMO:
                inputContext.addEntry(
                        "synced_tab_group_exists",
                        ProcessedValue.fromFloat(syncedTabGroupExists(profile)));
                return inputContext;
            case ModuleType.QUICK_DELETE_PROMO:
                return inputContext;
            case ModuleType.HISTORY_SYNC_PROMO:
                inputContext.addEntry(
                        "is_eligible_to_history_opt_in",
                        ProcessedValue.fromFloat(isEligibleToHistoryOptIn(profile)));
                return inputContext;
            default:
                assert false : "Card type not supported!";
                return inputContext;
        }
    }

    /**
     * @see DefaultBrowserPromoUtils#shouldShowNonRoleManagerPromo(Context), returns a value of 1.0f
     *     to indicate that a default browser promo, other than the Role Manager Promo, should be
     *     displayed. If not, it returns 0.0f.
     */
    private static float shouldShowNonRoleManagerDefaultBrowserPromo(
            EducationTipModuleActionDelegate actionDelegate) {
        return DefaultBrowserPromoUtils.getInstance()
                                .shouldShowNonRoleManagerPromo(actionDelegate.getContext())
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
                ? 1.0f
                : 0.0f;
    }

    /**
     * Returns a value of 1.0f to signify that the default browser promotion has been displayed
     * within the past 7 days on a platform other than the current one, such as through settings,
     * messages, or alternative NTPs. If the promotion has not been shown within this timeframe, the
     * function returns 0.0f.
     */
    private static float hasDefaultBrowserPromoShownInOtherSurface(Tracker tracker) {
        return tracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK)
                ? 0.0f
                : 1.0f;
    }

    /**
     * Returns a value of 1.0f if a tab group exists within either the normal or incognito TabModel.
     * Otherwise, it returns 0.0f.
     */
    private static float tabGroupExists(EducationTipModuleActionDelegate actionDelegate) {
        TabGroupModelFilterProvider provider =
                actionDelegate.getTabModelSelector().getTabGroupModelFilterProvider();
        TabGroupModelFilter normalFilter =
                provider.getTabGroupModelFilter(/* isIncognito= */ false);
        assumeNonNull(normalFilter);

        TabGroupModelFilter incognitoFilter =
                provider.getTabGroupModelFilter(/* isIncognito= */ true);
        assumeNonNull(incognitoFilter);

        int groupCount = normalFilter.getTabGroupCount() + incognitoFilter.getTabGroupCount();
        return groupCount > 0 ? 1.0f : 0.0f;
    }

    /** Returns the total number of tabs across both regular and incognito browsing modes. */
    private static float getCurrentTabCount(EducationTipModuleActionDelegate actionDelegate) {
        TabModelSelector tabModelSelector = actionDelegate.getTabModelSelector();

        if (tabModelSelector.isTabStateInitialized()) {
            TabModel normalModel = tabModelSelector.getModel(/* incognito= */ false);
            TabModel incognitoModel = tabModelSelector.getModel(/* incognito= */ true);
            return normalModel.getCount() + incognitoModel.getCount();
        }

        return actionDelegate.getTabCountForRelaunchFromSharedPrefs();
    }

    /** Returns a value of 1.0f if a synced tab group exists. Otherwise, it returns 0.0f. */
    private static float syncedTabGroupExists(Profile profile) {
        @Nullable TabGroupSyncService tabGroupSyncService = null;
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) {
            tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        }

        if (tabGroupSyncService == null) {
            return 0.0f;
        }

        int syncedGroupCount = tabGroupSyncService.getAllGroupIds().length;
        return syncedGroupCount > 0 ? 1.0f : 0.0f;
    }

    /**
     * Returns a value of 1.0f if the user is eligible to history sync. Otherwise, it returns 0.0f.
     */
    private static float isEligibleToHistoryOptIn(Profile profile) {
        if (assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile))
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            HistorySyncHelper helper = HistorySyncHelper.getForProfile(profile);
            return helper.shouldSuppressHistorySync() || helper.isDeclinedOften() ? 0.0f : 1.0f;
        }

        return 0.0f;
    }

    /** Returns a value of 1.0f if the user has signed in. Otherwise, it returns 0.0f. */
    private static float isUserSignedIn(Profile profile) {
        if (assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile))
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return 1.0f;
        }

        return 0.0f;
    }
}
