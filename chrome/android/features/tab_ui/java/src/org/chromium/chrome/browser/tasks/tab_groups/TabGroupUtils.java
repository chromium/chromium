// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;

/**
 * Helper class to handle tab groups related utilities.
 */
public class TabGroupUtils {
    private static TabModelSelectorTabObserver sTabModelSelectorTabObserver;
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    public static void maybeShowIPH(@FeatureConstants String featureName, View view,
            @Nullable BottomSheetController bottomSheetController) {
        // For tab group, all three IPHs are valid. For conditional tab strip, the only valid IPH
        // below is TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE.
        if (!TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                && !(TabUiFeatureUtilities.isConditionalTabStripEnabled()
                        && featureName.equals(
                                FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE))) {
            return;
        }
        if (TabUiFeatureUtilities.isLaunchPolishEnabled() && view == null) {
            return;
        }

        @StringRes
        int textId;
        @StringRes
        int accessibilityTextId;
        switch (featureName) {
            case FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE:
                textId = R.string.iph_tab_groups_quickly_compare_pages_text;
                accessibilityTextId = textId;
                break;
            case FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE:
                textId = R.string.iph_tab_groups_tap_to_see_another_tab_text;
                accessibilityTextId =
                        R.string.iph_tab_groups_tap_to_see_another_tab_accessibility_text;
                break;
            case FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE:
                textId = R.string.iph_tab_groups_your_tabs_together_text;
                accessibilityTextId = textId;
                break;
            default:
                assert false;
                return;
        }

        final Tracker tracker =
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        if (!tracker.isInitialized()) return;
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(view);

        TextBubble textBubble = new TextBubble(view.getContext(), view, textId, accessibilityTextId,
                true, rectProvider, ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        textBubble.setDismissOnTouchInteraction(true);
        if (!TabUiFeatureUtilities.isLaunchBugFixEnabled()) {
            textBubble.addOnDismissListener(() -> tracker.dismissed(featureName));
            textBubble.show();
            return;
        }
        if (bottomSheetController == null) return;
        assert featureName.equals(FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE);

        // This observer is added when IPH shows and is removed when IPH is dismissed via user
        // explicitly closing the text bubble.
        BottomSheetObserver bottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == BottomSheetController.SheetState.HIDDEN) {
                    textBubble.show();
                } else {
                    textBubble.dismiss();
                }
            }
        };

        textBubble.addOnDismissListener(() -> {
            // Don't dismiss the feature when the hide is caused by bottom sheet showing.
            if (bottomSheetController.getSheetState() != BottomSheetController.SheetState.HIDDEN) {
                return;
            }
            tracker.dismissed(featureName);
            bottomSheetController.removeObserver(bottomSheetObserver);
        });

        bottomSheetController.addObserver(bottomSheetObserver);
        textBubble.show();
    }

    /**
     * Start a TabModelSelectorTabObserver to show IPH for TabGroups.
     */
    public static void startObservingForCreationIPH() {
        if (sTabModelSelectorTabObserver != null) return;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) return;
        TabModelSelector selector = ((ChromeTabbedActivity) activity).getTabModelSelector();

        sTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (!navigationHandle.isInMainFrame()) return;
                if (tab.isIncognito()) return;
                Integer transition = navigationHandle.pageTransition();
                // Searching from omnibox results in PageTransition.GENERATED.
                if (navigationHandle.isValidSearchFormUrl()
                        || (transition != null
                                && (transition & PageTransition.CORE_MASK)
                                        == PageTransition.GENERATED)) {
                    maybeShowIPH(FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
                            tab.getView(), null);
                    sTabModelSelectorTabObserver.destroy();
                }
            }
        };
    }

    /**
     * This method gets the selected tab of the group where {@code tab} is in.
     * @param selector   The selector that owns the {@code tab}.
     * @param tab        {@link Tab}
     * @return The selected tab of the group which contains the {@code tab}
     */
    public static Tab getSelectedTabInGroupForTab(TabModelSelector selector, Tab tab) {
        TabGroupModelFilter filter = (TabGroupModelFilter) selector.getTabModelFilterProvider()
                                             .getCurrentTabModelFilter();
        return filter.getTabAt(filter.indexOf(tab));
    }

    /**
     * This method gets the index in TabModel of the first tab in {@code tabs}.
     * @param tabModel   The tabModel that owns the {@code tab}.
     * @param tabs       The list of tabs among which we need to find the first tab index.
     * @return The index in TabModel of the first tab in {@code tabs}
     */
    public static int getFirstTabModelIndexForList(TabModel tabModel, List<Tab> tabs) {
        assert tabs != null && tabs.size() != 0;

        return tabModel.indexOf(tabs.get(0));
    }

    /**
     * This method gets the index in TabModel of the last tab in {@code tabs}.
     * @param tabModel   The tabModel that owns the {@code tab}.
     * @param tabs       The list of tabs among which we need to find the last tab index.
     * @return The index in TabModel of the last tab in {@code tabs}
     */
    public static int getLastTabModelIndexForList(TabModel tabModel, List<Tab> tabs) {
        assert tabs != null && tabs.size() != 0;

        return tabModel.indexOf(tabs.get(tabs.size() - 1));
    }

    /**
     * This method stores tab group title with reference to {@code tabRootId}.
     * @param tabRootId   The tab root ID which is used as reference to store group title.
     * @param title       The tab group title to store.
     */
    public static void storeTabGroupTitle(int tabRootId, String title) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().putString(String.valueOf(tabRootId), title).apply();
    }

    /**
     * This method deletes specific stored tab group title with reference to {@code tabRootId}.
     * @param tabRootId  The tab root ID whose related tab group title will be deleted.
     */
    public static void deleteTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches tab group title with related tab group root ID.
     * @param tabRootId  The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the target tab group, default value is null.
     */
    @Nullable
    public static String getTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getSharedPreferences().getString(String.valueOf(tabRootId), null);
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext().getSharedPreferences(
                TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }

    @VisibleForTesting
    public static void triggerAssertionForTesting() {
        assert false;
    }
}
