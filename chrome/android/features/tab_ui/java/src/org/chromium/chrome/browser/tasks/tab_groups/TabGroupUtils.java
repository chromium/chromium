// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
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

/** Helper class to handle tab groups related utilities. */
public class TabGroupUtils {
    private static TabModelSelectorTabObserver sTabModelSelectorTabObserver;

    public static void maybeShowIPH(
            Profile profile,
            @FeatureConstants String featureName,
            View view,
            @Nullable BottomSheetController bottomSheetController) {
        if (view == null) return;

        @StringRes int textId;
        @StringRes int accessibilityTextId;
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
            default:
                assert false;
                return;
        }

        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.isInitialized()) return;
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(view);

        TextBubble textBubble =
                new TextBubble(
                        view.getContext(),
                        view,
                        textId,
                        accessibilityTextId,
                        true,
                        rectProvider,
                        ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        textBubble.setDismissOnTouchInteraction(true);
        if (bottomSheetController == null) return;
        assert featureName.equals(FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE);

        // This observer is added when IPH shows and is removed when IPH is dismissed via user
        // explicitly closing the text bubble.
        BottomSheetObserver bottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        if (newState == BottomSheetController.SheetState.HIDDEN) {
                            textBubble.show();
                        } else {
                            textBubble.dismiss();
                        }
                    }
                };

        textBubble.addOnDismissListener(
                () -> {
                    // Don't dismiss the feature when the hide is caused by bottom sheet showing.
                    if (bottomSheetController.getSheetState()
                            != BottomSheetController.SheetState.HIDDEN) {
                        return;
                    }
                    tracker.dismissed(featureName);
                    bottomSheetController.removeObserver(bottomSheetObserver);
                });

        bottomSheetController.addObserver(bottomSheetObserver);
        textBubble.show();
    }

    /** Start a TabModelSelectorTabObserver to show IPH for TabGroups. */
    public static void startObservingForCreationIPH() {
        if (sTabModelSelectorTabObserver != null) return;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) return;
        TabModelSelector selector = ((ChromeTabbedActivity) activity).getTabModelSelector();

        sTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(selector) {
                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        if (tab.isIncognito()) return;
                        if (!navigationHandle.hasCommitted()) return;

                        // Searching from omnibox results in PageTransition.GENERATED.
                        if (navigationHandle.isValidSearchFormUrl()
                                || (navigationHandle.pageTransition() & PageTransition.CORE_MASK)
                                        == PageTransition.GENERATED) {
                            maybeShowIPH(
                                    tab.getProfile(),
                                    FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
                                    tab.getView(),
                                    null);
                            sTabModelSelectorTabObserver.destroy();
                        }
                    }
                };
    }

    /**
     * This method gets the selected tab of the group where {@code tab} is in.
     *
     * @param filter The filter that owns the {@code tab}.
     * @param tab The {@link Tab}.
     * @return The selected tab of the group which contains the {@code tab}
     */
    public static Tab getSelectedTabInGroupForTab(TabGroupModelFilter filter, Tab tab) {
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

    public static void triggerAssertionForTesting() {
        assert false;
    }
}
