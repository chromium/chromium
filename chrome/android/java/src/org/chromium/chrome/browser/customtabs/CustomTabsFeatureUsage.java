// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.BitSet;

/** Records a histogram that tracks usage of all the CCT features of interest. */
public class CustomTabsFeatureUsage {
    @VisibleForTesting
    public static final String CUSTOM_TABS_FEATURE_USAGE_HISTOGRAM = "CustomTabs.FeatureUsage";

    // NOTE: This must be kept in sync with the definition |CustomTabsFeatureUsed|
    // in tools/metrics/histograms/enums.xml.
    @IntDef({
        CustomTabsFeature.CTF_SESSIONS,
        CustomTabsFeature.EXTRA_ACTION_BUTTON_BUNDLE,
        CustomTabsFeature.EXTRA_TINT_ACTION_BUTTON,
        CustomTabsFeature.EXTRA_INITIAL_BACKGROUND_COLOR,
        CustomTabsFeature.EXTRA_ENABLE_BACKGROUND_INTERACTION,
        CustomTabsFeature.EXTRA_CLOSE_BUTTON_ICON,
        CustomTabsFeature.EXTRA_CLOSE_BUTTON_POSITION,
        CustomTabsFeature.CTF_DARK,
        CustomTabsFeature.CTF_LIGHT,
        CustomTabsFeature.EXTRA_COLOR_SCHEME,
        CustomTabsFeature.CTF_SYSTEM,
        CustomTabsFeature.EXTRA_DISABLE_DOWNLOAD_BUTTON,
        CustomTabsFeature.EXTRA_DISABLE_STAR_BUTTON,
        CustomTabsFeature.EXTRA_EXIT_ANIMATION_BUNDLE,
        CustomTabsFeature.EXPERIMENT_IDS,
        CustomTabsFeature.EXTRA_OPEN_NEW_INCOGNITO_TAB,
        CustomTabsFeature.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX,
        CustomTabsFeature.EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE,
        CustomTabsFeature.EXTRA_BROWSER_LAUNCH_SOURCE,
        CustomTabsFeature.EXTRA_MEDIA_VIEWER_URL,
        CustomTabsFeature.EXTRA_MENU_ITEMS,
        CustomTabsFeature.EXTRA_CALLING_ACTIVITY_PACKAGE,
        CustomTabsFeature.CTF_PACKAGE_NAME,
        CustomTabsFeature.EXTRA_TOOLBAR_CORNER_RADIUS_DP,
        CustomTabsFeature.CTF_PARTIAL,
        CustomTabsFeature.EXTRA_REMOTEVIEWS_PENDINGINTENT,
        CustomTabsFeature.CTF_READER_MODE,
        CustomTabsFeature.EXTRA_REMOTEVIEWS_VIEW_IDS,
        CustomTabsFeature.EXTRA_REMOTEVIEWS,
        CustomTabsFeature.EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR,
        CustomTabsFeature.EXTRA_SCREEN_ORIENTATION,
        CustomTabsFeature.CTF_SENT_BY_CHROME,
        CustomTabsFeature.EXTRA_KEEP_ALIVE,
        CustomTabsFeature.EXTRA_DEFAULT_SHARE_MENU_ITEM,
        CustomTabsFeature.EXTRA_SHARE_STATE,
        CustomTabsFeature.EXTRA_TITLE_VISIBILITY_STATE,
        CustomTabsFeature.EXTRA_TOOLBAR_ITEMS,
        CustomTabsFeature.EXTRA_TRANSLATE_LANGUAGE,
        CustomTabsFeature.EXTRA_DISPLAY_MODE,
        CustomTabsFeature.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY,
        CustomTabsFeature.EXTRA_ADDITIONAL_TRUSTED_ORIGINS,
        CustomTabsFeature.EXTRA_ENABLE_URLBAR_HIDING,
        CustomTabsFeature.EXTRA_AUTO_TRANSLATE_LANGUAGE,
        CustomTabsFeature.EXTRA_INTENT_FEATURE_OVERRIDES,
        CustomTabsFeature.CTF_PARTIAL_SIDE_SHEET,
        CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP,
        CustomTabsFeature.EXTRA_INITIAL_ACTIVITY_WIDTH_PX,
        CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION,
        CustomTabsFeature.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION,
        CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE,
        CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_POSITION,
        CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_SLIDE_IN_BEHAVIOR,
        CustomTabsFeature.EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION,
        CustomTabsFeature.EXTRA_ACTIVITY_SCROLL_CONTENT_RESIZE,
        CustomTabsFeature.EXTRA_ENABLE_EPHEMERAL_BROWSING,
        CustomTabsFeature.EXTRA_ENABLE_GOOGLE_BOTTOM_BAR,
        CustomTabsFeature.EXTRA_GOOGLE_BOTTOM_BAR_BUTTONS,
        CustomTabsFeature.EXTRA_NETWORK,
        CustomTabsFeature.EXTRA_LAUNCH_AUTH_TAB,
        CustomTabsFeature.EXTRA_REDIRECT_SCHEME,
        CustomTabsFeature.EXTRA_HTTPS_REDIRECT_HOST,
        CustomTabsFeature.EXTRA_HTTPS_REDIRECT_PATH,
        CustomTabsFeature.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabsFeature {
        /** Special enum for the start of a session. */
        int CTF_SESSIONS = 0;

        int EXTRA_ACTION_BUTTON_BUNDLE = 1;
        int EXTRA_TINT_ACTION_BUTTON = 2;
        int EXTRA_INITIAL_BACKGROUND_COLOR = 3;
        int EXTRA_ENABLE_BACKGROUND_INTERACTION = 4;
        int EXTRA_CLOSE_BUTTON_ICON = 5;
        int EXTRA_CLOSE_BUTTON_POSITION = 6;
        int CTF_DARK = 7;
        int CTF_LIGHT = 8;
        int EXTRA_COLOR_SCHEME = 9;
        int CTF_SYSTEM = 10;
        int EXTRA_DISABLE_DOWNLOAD_BUTTON = 11;
        int EXTRA_DISABLE_STAR_BUTTON = 12;
        int EXTRA_EXIT_ANIMATION_BUNDLE = 13;
        int EXPERIMENT_IDS = 14;
        int EXTRA_OPEN_NEW_INCOGNITO_TAB = 15;
        int EXTRA_INITIAL_ACTIVITY_HEIGHT_PX = 16;
        int EXTRA_ENABLE_EMBEDDED_MEDIA_EXPERIENCE = 17;
        int EXTRA_BROWSER_LAUNCH_SOURCE = 18;
        int EXTRA_MEDIA_VIEWER_URL = 19;
        int EXTRA_MENU_ITEMS = 20;
        int EXTRA_CALLING_ACTIVITY_PACKAGE = 21;
        int CTF_PACKAGE_NAME = 22;
        int EXTRA_TOOLBAR_CORNER_RADIUS_DP = 23;
        int CTF_PARTIAL = 24;
        int EXTRA_REMOTEVIEWS_PENDINGINTENT = 25;
        int CTF_READER_MODE = 26;
        int EXTRA_REMOTEVIEWS_VIEW_IDS = 27;
        int EXTRA_REMOTEVIEWS = 28;
        int EXTRA_ACTIVITY_HEIGHT_RESIZE_BEHAVIOR = 29;
        int EXTRA_SCREEN_ORIENTATION = 30;
        int CTF_SENT_BY_CHROME = 31;
        int EXTRA_KEEP_ALIVE = 32;
        int EXTRA_DEFAULT_SHARE_MENU_ITEM = 33;
        int EXTRA_SHARE_STATE = 34;
        int EXTRA_TITLE_VISIBILITY_STATE = 35;
        int EXTRA_TOOLBAR_ITEMS = 36;
        int EXTRA_TRANSLATE_LANGUAGE = 37;
        int EXTRA_DISPLAY_MODE = 38;
        int EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY = 39;
        int EXTRA_ADDITIONAL_TRUSTED_ORIGINS = 40;
        int EXTRA_ENABLE_URLBAR_HIDING = 41;
        int EXTRA_AUTO_TRANSLATE_LANGUAGE = 42;
        int EXTRA_INTENT_FEATURE_OVERRIDES = 43;
        int CTF_PARTIAL_SIDE_SHEET = 44;
        int EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP = 45;
        int EXTRA_INITIAL_ACTIVITY_WIDTH_PX = 46;
        int EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION = 47;
        int EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION = 48;
        int EXTRA_ACTIVITY_SIDE_SHEET_DECORATION_TYPE = 49;
        int EXTRA_ACTIVITY_SIDE_SHEET_POSITION = 50;
        int EXTRA_ACTIVITY_SIDE_SHEET_SLIDE_IN_BEHAVIOR = 51;
        int EXTRA_ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION = 52;
        int EXTRA_ACTIVITY_SCROLL_CONTENT_RESIZE = 54;
        int EXTRA_ENABLE_EPHEMERAL_BROWSING = 55;
        int EXTRA_ENABLE_GOOGLE_BOTTOM_BAR = 56;
        int EXTRA_GOOGLE_BOTTOM_BAR_BUTTONS = 57;
        int EXTRA_NETWORK = 58;
        int EXTRA_LAUNCH_AUTH_TAB = 59;
        int EXTRA_REDIRECT_SCHEME = 60;
        int EXTRA_HTTPS_REDIRECT_HOST = 61;
        int EXTRA_HTTPS_REDIRECT_PATH = 62;

        /** Total count of entries. */
        int COUNT = 63;
    }

    // Whether flag-enabled or not.
    private boolean mIsEnabled;

    /** Tracks whether we have written each enum or not. */
    private BitSet mUsed = new BitSet(CustomTabsFeature.COUNT);

    /** Tracks the usage of Chrome Custom Tabs in a single large histogram. */
    public CustomTabsFeatureUsage() {
        mIsEnabled = isEnabled();
    }

    /** @return whether this feature is enabled or not. */
    static boolean isEnabled() {
        return ChromeFeatureList.sCctFeatureUsage.isEnabled();
    }

    /** Logs the usage of the given feature, if enabled. */
    void log(@CustomTabsFeature int feature) {
        if (!mIsEnabled) return;

        logInternal(feature);
        // Make sure we've logged a Session.
        // This ensures no feature can have a higher usage that SESSIONS.
        logInternal(CustomTabsFeature.CTF_SESSIONS);
    }

    /** Logs the given feature in a histogram unless it has already be logged. */
    private void logInternal(@CustomTabsFeature int feature) {
        // Ensure each feature is logged, and marked as used, at most once.
        if (mUsed.get(feature)) return;

        mUsed.set(feature);
        RecordHistogram.recordEnumeratedHistogram(
                CUSTOM_TABS_FEATURE_USAGE_HISTOGRAM, feature, CustomTabsFeature.COUNT);
    }
}
