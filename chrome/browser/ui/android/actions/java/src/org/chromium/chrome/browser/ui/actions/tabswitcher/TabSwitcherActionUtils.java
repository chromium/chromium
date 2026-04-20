// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.actions.IphIntent;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Helper utility methods for the Tab Switcher action. */
@NullMarked
public class TabSwitcherActionUtils {

    private static final long XR_AUTO_DISMISS_TIMEOUT_MS = 5000;

    private TabSwitcherActionUtils() {}

    /**
     * Returns whether the data sharing feature is enabled. This method is needed because {@link
     * TabUiUtils#isDataSharingFunctionalityEnabled()} breaks dependency rules.
     */
    public static boolean isDataSharingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_JOIN_ONLY);
    }

    /**
     * Returns an {@link IphIntent} for the update notification IPH.
     *
     * @param dotInfo Information about the tab model dot to use for formatting the IPH message.
     */
    public static IphIntent getUpdateNotificationIphIntent(TabModelDotInfo dotInfo) {
        return new IphIntent.Builder(FeatureConstants.TAB_GROUP_SHARE_UPDATE_FEATURE)
                .setStringResId(R.string.tab_group_update_iph_text)
                .setAccessibilityResId(R.string.tab_group_update_iph_text)
                .setStringArgs(dotInfo.tabGroupTitle)
                .setHighlightConfig(new IphIntent.HighlightConfig(HighlightShape.CIRCLE, false))
                .build();
    }

    /**
     * Returns an {@link IphIntent} for the declutter IPH.
     *
     * @param onShowCallback The callback to run when the IPH is shown.
     * @param onDismissCallback The callback to run when the IPH is dismissed.
     */
    public static IphIntent getDeclutterIphIntent(
            Runnable onShowCallback, Runnable onDismissCallback) {
        return new IphIntent.Builder(FeatureConstants.ANDROID_TAB_DECLUTTER_FEATURE)
                .setStringResId(R.string.iph_android_tab_declutter_text_with_tab_groups)
                .setAccessibilityResId(R.string.iph_android_tab_declutter_accessibility_text)
                .setHighlightConfig(new IphIntent.HighlightConfig(HighlightShape.CIRCLE, true))
                .setOnShowCallback(onShowCallback)
                .setOnDismissCallback(onDismissCallback)
                .build();
    }

    /**
     * Returns an {@link IphIntent} for the versioning IPH, which is shown to users who have updated
     * Chrome and regained access to a shared tab group.
     */
    public static IphIntent getVersioningIphIntent() {
        return new IphIntent.Builder(FeatureConstants.TAB_GROUP_SHARE_VERSION_UPDATE_FEATURE)
                .setStringResId(
                        R.string.collaboration_shared_tab_groups_available_again_iph_message)
                .setAccessibilityResId(
                        R.string.collaboration_shared_tab_groups_available_again_iph_message)
                .setHighlightConfig(new IphIntent.HighlightConfig(HighlightShape.CIRCLE, false))
                .build();
    }

    /** Returns an {@link IphIntent} for the switch out of incognito IPH. */
    public static IphIntent getSwitchOutOfIncognitoIphIntent() {
        return new IphIntent.Builder(FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO)
                .setStringResId(R.string.iph_tab_switcher_switch_out_of_incognito_text)
                .setAccessibilityResId(
                        R.string.iph_tab_switcher_switch_out_of_incognito_accessibility_text)
                .setHighlightConfig(new IphIntent.HighlightConfig(HighlightShape.CIRCLE, true))
                .build();
    }

    /** Returns an {@link IphIntent} for the switch into incognito IPH. */
    public static IphIntent getSwitchIntoIncognitoIphIntent() {
        return new IphIntent.Builder(FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO)
                .setStringResId(R.string.iph_tab_switcher_switch_into_incognito_text)
                .setAccessibilityResId(
                        R.string.iph_tab_switcher_switch_into_incognito_accessibility_text)
                .setHighlightConfig(new IphIntent.HighlightConfig(HighlightShape.CIRCLE, true))
                .build();
    }

    /** Returns an {@link IphIntent} for the tab switcher button IPH. */
    public static IphIntent getTabSwitcherButtonIphIntent() {
        return new IphIntent.Builder(FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE)
                .setStringResId(R.string.iph_tab_switcher_text)
                .setAccessibilityResId(R.string.iph_tab_switcher_accessibility_text)
                .setHighlightConfig(new IphIntent.HighlightConfig(HighlightShape.CIRCLE, true))
                .build();
    }

    /** Returns an {@link IphIntent} for the XR tab switcher button IPH. */
    public static IphIntent getXrIphIntent() {
        return new IphIntent.Builder(FeatureConstants.IPH_TAB_SWITCHER_XR)
                .setStringResId(R.string.iph_tab_switcher_xr)
                .setAccessibilityResId(R.string.iph_tab_switcher_xr)
                .setAutoDismissTimeoutMs(XR_AUTO_DISMISS_TIMEOUT_MS)
                .setEnableSnoozeMode(true)
                .build();
    }
}
