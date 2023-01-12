// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;

/**
 * Self-documenting feature class for bookmarks.
 *
 * <p>{@link ChromeFeatureList#BOOKMARKS_REFRESH}
 * <ul>
 * <li>{@code add_bookmark_in_app_menu}: boolean; show "Bookmark" as a standalone item in the app
 * menu. Default: {@code false}</li>
 * <li>{@code edit_bookmark_in_app_menu}: boolean; show "Edit Bookmark" in the app menu. Default:
 * {@code false}</li>
 * <li>{@code bookmark_visuals_enabled}: boolean; refresh the visual looks of the bookmarks
 * manager. Default: {@code false}</li>
 * <li>{@code bookmarks_refresh_min_version}: boolean; see {@link #VERSION}.</li>
 * </ul>
 */
public class BookmarkFeatures {
    /**
     * An indicator of the code version specific to bookmarks and the reading list. It allows
     * feature flags to be individually enabled without changing the {@code min_version} for the
     * whole study.
     *
     * <p>Feature flag params using the {@code VERSION} value:
     * <ul>
     * <li>{@code bookmarks_improved_save_flow_min_version} - {@link
     * ChromeFeatureList#BOOKMARKS_IMPROVED_SAVE_FLOW}
     * <li>{@code bookmarks_refresh_min_version} - {@link ChromeFeatureList#BOOKMARKS_REFRESH}
     * <li>{@code bookmark_compact_visuals_enabled} - {@link ChromeFeatureList#BOOKMARKS_REFRESH}
     * <li>{@code bookmark_visuals_enabled} - {@link ChromeFeatureList#BOOKMARKS_REFRESH}
     * </ul>
     *
     * <p>These parameters allow to control for cases where a significant bug fix or change of param
     * semantics was merged into release, and prior versions of the code are not to be enabled. In
     * such case the patch increments the {@code VERSION}, while experiment sets {@code
     * *_min_version} to set a lower bound on existing implementation with the fix.
     */
    static final int VERSION = 0;

    private static final boolean BOOKMARK_VISUALS_ENABLED_DEFAULT = false;
    private static final boolean BOOKMARK_COMPACT_VISUALS_ENABLED_DEFAULT = false;
    private static final boolean IMPROVED_SAVE_FLOW_AUTODISMISS_ENABLED_DEFAULT = true;
    private static final int IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT = 6000;

    static final String BOOKMARK_VISUALS_ENABLED = "bookmark_visuals_enabled";
    static final String BOOKMARK_COMPACT_VISUALS_ENABLED = "bookmark_compact_visuals_enabled";
    static final String AUTODISMISS_ENABLED_PARAM_NAME = "autodismiss_enabled";
    static final String AUTODISMISS_LENGTH_PARAM_NAME = "autodismiss_length_ms";

    private static final MutableFlagWithSafeDefault sAndroidImprovedBookmarksFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS, false);

    public static boolean isImprovedSaveFlowEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                           ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW,
                           "bookmarks_improved_save_flow_min_version", 0)
                <= VERSION;
    }

    public static boolean isImprovedSaveFlowAutodismissEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW,
                        AUTODISMISS_ENABLED_PARAM_NAME,
                        IMPROVED_SAVE_FLOW_AUTODISMISS_ENABLED_DEFAULT);
    }

    public static int getImprovedSaveFlowAutodismissTimeMs() {
        if (!FeatureList.isInitialized()) return IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW, AUTODISMISS_LENGTH_PARAM_NAME,
                IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT);
    }

    public static boolean isBookmarksRefreshEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_REFRESH)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                           ChromeFeatureList.BOOKMARKS_REFRESH, "bookmarks_refresh_min_version", 0)
                <= VERSION;
    }

    public static boolean isBookmarkMenuItemAsDedicatedRowEnabled() {
        return isBookmarksRefreshEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BOOKMARKS_REFRESH, "bookmark_in_app_menu", false);
    }

    public static boolean isBookmarksVisualRefreshEnabled() {
        return isBookmarksRefreshEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BOOKMARKS_REFRESH, BOOKMARK_VISUALS_ENABLED,
                        BOOKMARK_VISUALS_ENABLED_DEFAULT);
    }

    /** The compact visual refresh is built on top of the base one. */
    public static boolean isCompactBookmarksVisualRefreshEnabled() {
        return isBookmarksVisualRefreshEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BOOKMARKS_REFRESH, BOOKMARK_COMPACT_VISUALS_ENABLED,
                        BOOKMARK_COMPACT_VISUALS_ENABLED_DEFAULT);
    }

    /**
     * More visual changes to the bookmarks surfaces, with more thumbnails and a focus on search
     * instead of folders/hierarchy.
     */
    public static boolean isAndroidImprovedBookmarksEnabled() {
        return sAndroidImprovedBookmarksFlag.isEnabled();
    }
}
