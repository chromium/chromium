// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Common hub feature utils for public use. */
public class HubFieldTrial {
    private static final String FLOATING_ACTION_BUTTON_PARAM = "floating_action_button";
    public static final BooleanCachedFieldTrialParameter FLOATING_ACTION_BUTTON =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_HUB, FLOATING_ACTION_BUTTON_PARAM, false);

    private static final String PANE_SWITCHER_USES_TEXT_PARAM = "pane_switcher_uses_text";
    public static final BooleanCachedFieldTrialParameter PANE_SWITCHER_USES_TEXT =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_HUB, PANE_SWITCHER_USES_TEXT_PARAM, false);

    private static final String SUPPORTS_OTHER_TABS_PARAM = "supports_other_tabs";
    public static final BooleanCachedFieldTrialParameter SUPPORTS_OTHER_TABS =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_HUB, SUPPORTS_OTHER_TABS_PARAM, false);

    private static final String SUPPORTS_BOOKMARKS_PARAM = "supports_bookmarks";
    public static final BooleanCachedFieldTrialParameter SUPPORTS_BOOKMARKS =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_HUB, SUPPORTS_BOOKMARKS_PARAM, false);

    private static final String SUPPORTS_SEARCH_PARAM = "supports_search";
    public static final BooleanCachedFieldTrialParameter SUPPORTS_SEARCH =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_HUB, SUPPORTS_SEARCH_PARAM, false);

    /** Returns whether the hub is enabled. */
    public static boolean isHubEnabled() {
        return ChromeFeatureList.sAndroidHub.isEnabled();
    }

    /**
     * Returns whether the primary action on a pane should be shown in a floating action button.
     * When false the button will be in part of the toolbar.
     */
    public static boolean usesFloatActionButton() {
        return FLOATING_ACTION_BUTTON.getValue();
    }

    /** Returns whether the UI to switch between panes is using text names or icons. */
    public static boolean doesPaneSwitcherUseText() {
        return PANE_SWITCHER_USES_TEXT.getValue();
    }

    /** Returns whether the hub should display an tabs from other devices pane. */
    public static boolean supportsOtherTabs() {
        return SUPPORTS_OTHER_TABS.getValue();
    }

    /** Returns whether the hub should display a bookmarks pane. */
    public static boolean supportsBookmarks() {
        return SUPPORTS_BOOKMARKS.getValue();
    }

    /** Returns whether the hub has a search for content across all panes. */
    public static boolean supportsSearch() {
        return SUPPORTS_SEARCH.getValue();
    }
}
