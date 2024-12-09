// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;

import java.util.HashSet;

/**
 * Provides information about the signals of cards in the educational tip module.
 *
 * <p>This class serves as a single educational tip module's cards signals' logic gateway.
 */
public class EducationalTipCardProviderSignalHandler {
    /**
     * A list includes all card types (excluding the default browser promo card) that have been
     * displayed to the user during the current session.
     */
    private final HashSet<Integer> mVisibleCardList;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static EducationalTipCardProviderSignalHandler sInstance =
                new EducationalTipCardProviderSignalHandler();
    }

    /** Returns the singleton instance of EducationalTipCardProviderSignalHandler. */
    public static EducationalTipCardProviderSignalHandler getInstance() {
        return EducationalTipCardProviderSignalHandler.LazyHolder.sInstance;
    }

    EducationalTipCardProviderSignalHandler() {
        mVisibleCardList = new HashSet<>();
    }

    /** Creates an instance of InputContext. */
    @VisibleForTesting
    InputContext createInputContext(
            EducationTipModuleActionDelegate actionDelegate, Tracker tracker) {
        InputContext inputContext = new InputContext();
        inputContext.addEntry(
                "should_show_non_role_manager_default_browser_promo",
                ProcessedValue.fromFloat(
                        shouldShowNonRoleManagerDefaultBrowserPromo(actionDelegate)));
        inputContext.addEntry(
                "has_default_browser_promo_shown_in_other_surface",
                ProcessedValue.fromFloat(hasDefaultBrowserPromoShownInOtherSurface(tracker)));
        inputContext.addEntry(
                "tab_group_exists", ProcessedValue.fromFloat(tabGroupExists(actionDelegate)));
        inputContext.addEntry(
                "number_of_tabs", ProcessedValue.fromFloat(getCurrentTabCount(actionDelegate)));
        return inputContext;
    }

    /**
     * @see DefaultBrowserPromoUtils#shouldShowNonRoleManagerPromo(Context), returns a value of 1.0f
     *     to indicate that a default browser promo, other than the Role Manager Promo, should be
     *     displayed. If not, it returns 0.0f.
     */
    private float shouldShowNonRoleManagerDefaultBrowserPromo(
            EducationTipModuleActionDelegate actionDelegate) {
        return DefaultBrowserPromoUtils.getInstance()
                        .shouldShowNonRoleManagerPromo(actionDelegate.getContext())
                ? 1.0f
                : 0.0f;
    }

    /**
     * Returns a value of 1.0f to signify that the default browser promotion has been displayed
     * within the past 7 days on a platform other than the current one, such as through settings,
     * messages, or alternative NTPs. If the promotion has not been shown within this timeframe, the
     * function returns 0.0f.
     */
    private float hasDefaultBrowserPromoShownInOtherSurface(Tracker tracker) {
        return tracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK)
                ? 0.0f
                : 1.0f;
    }

    /**
     * Returns a value of 1.0f if a tab group exists within either the normal or incognito TabModel.
     * Otherwise, it returns 0.0f.
     */
    private float tabGroupExists(EducationTipModuleActionDelegate actionDelegate) {
        TabGroupModelFilterProvider provider =
                actionDelegate.getTabModelSelector().getTabGroupModelFilterProvider();
        TabGroupModelFilter normalFilter =
                provider.getTabGroupModelFilter(/* isIncognito= */ false);
        TabGroupModelFilter incognitoFilter =
                provider.getTabGroupModelFilter(/* isIncognito= */ true);
        int groupCount = normalFilter.getTabGroupCount() + incognitoFilter.getTabGroupCount();
        return groupCount > 0 ? 1.0f : 0.0f;
    }

    /** Returns the total number of tabs across both regular and incognito browsing modes. */
    private float getCurrentTabCount(EducationTipModuleActionDelegate actionDelegate) {
        TabModelSelector tabModelSelector = actionDelegate.getTabModelSelector();
        TabModel normalModel = tabModelSelector.getModel(/* incognito= */ false);
        TabModel incognitoModel = tabModelSelector.getModel(/* incognito= */ true);
        return normalModel.getCount() + incognitoModel.getCount();
    }

    /**
     * Returns true if this is the first time the card is displayed to the user in the current
     * session and the event should be recorded.
     */
    boolean shouldNotifyCardShownPerSession(@EducationalTipCardType int cardType) {
        // Ensure that the default browser promo card does not trigger this function.
        assert cardType < EducationalTipCardType.NUM_ENTRIES && cardType > 0;

        if (mVisibleCardList.contains(cardType)) {
            return false;
        }

        return mVisibleCardList.add(cardType);
    }

    static void setInstanceForTesting(EducationalTipCardProviderSignalHandler testInstance) {
        var oldInstance = EducationalTipCardProviderSignalHandler.LazyHolder.sInstance;
        EducationalTipCardProviderSignalHandler.LazyHolder.sInstance = testInstance;
        ResettersForTesting.register(
                () -> EducationalTipCardProviderSignalHandler.LazyHolder.sInstance = oldInstance);
    }
}
