// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is a utility class for managing features related to returning to Chrome after haven't used
 * Chrome for a while.
 */
public final class ReturnToChromeUtil {
    private static final String TAG = "TabSwitcherOnReturn";

    @VisibleForTesting
    public static final long INVALID_DECISION_TIMESTAMP = -1L;
    public static final long MILLISECONDS_PER_DAY = TimeUtils.SECONDS_PER_DAY * 1000;

    @VisibleForTesting
    public static final String TAB_SWITCHER_ON_RETURN_MS_PARAM = "tab_switcher_on_return_time_ms";
    public static final IntCachedFieldTrialParameter TAB_SWITCHER_ON_RETURN_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_SWITCHER_ON_RETURN, TAB_SWITCHER_ON_RETURN_MS_PARAM, -1);

    private ReturnToChromeUtil() {}

    /**
     * Determine if we should show the tab switcher on returning to Chrome.
     *   Returns true if enough time has elapsed since the app was last backgrounded.
     *   The threshold time in milliseconds is set by experiment "enable-tab-switcher-on-return"
     *
     * @param lastBackgroundedTimeMillis The last time the application was backgrounded. Set in
     *                                   ChromeTabbedActivity::onStopWithNative
     * @return true if past threshold, false if not past threshold or experiment cannot be loaded.
     */
    public static boolean shouldShowTabSwitcher(final long lastBackgroundedTimeMillis) {
        int tabSwitcherAfterMillis = TAB_SWITCHER_ON_RETURN_MS.getValue();

        if (lastBackgroundedTimeMillis == -1) {
            // No last background timestamp set, use control behavior unless "immediate" was set.
            return tabSwitcherAfterMillis == 0;
        }

        if (tabSwitcherAfterMillis < 0) {
            // If no value for experiment, use control behavior.
            return false;
        }

        long expirationTime = lastBackgroundedTimeMillis + tabSwitcherAfterMillis;

        return System.currentTimeMillis() > expirationTime;
    }

    /**
     * Check whether we should show Start Surface as the home page. This is used for all cases
     * except initial tab creation, which uses {@link
     * ReturnToChromeUtil#isStartSurfaceEnabled(Context)}.
     *
     * @return Whether Start Surface should be shown as the home page.
     * @param context The activity context
     */
    public static boolean shouldShowStartSurfaceAsTheHomePage(Context context) {
        return isStartSurfaceEnabled(context)
                && !StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START.getValue();
    }

    /**
     * @return Whether we should show Start Surface as the home page on phone. Start surface
     *         hasn't been enabled on tablet yet.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePageOnPhone(
            Context context, boolean isTablet) {
        return !isTablet && shouldShowStartSurfaceAsTheHomePage(context);
    }

    /**
     * Check whether Start Surface is enabled. It includes checks of:
     * 1) whether home page is enabled and whether it is Chrome' home page url;
     * 2) whether Start surface is enabled with current accessibility settings;
     * 3) whether it is on phone.
     * @param context The activity context.
     */
    public static boolean isStartSurfaceEnabled(Context context) {
        return false;
    }

    /**
     * @param tabModelSelector The tab model selector.
     * @return the total tab count, and works before native initialization.
     */
    public static int getTotalTabCount(TabModelSelector tabModelSelector) {
        if (!tabModelSelector.isTabStateInitialized()) {
            return SharedPreferencesManager.getInstance().readInt(
                           ChromePreferenceKeys.REGULAR_TAB_COUNT)
                    + SharedPreferencesManager.getInstance().readInt(
                            ChromePreferenceKeys.INCOGNITO_TAB_COUNT);
        }

        return tabModelSelector.getTotalTabCount();
    }

    /**
     * Returns whether grid Tab switcher or the Start surface should be shown at startup.
     */
    public static boolean shouldShowOverviewPageOnStart(Context context, Intent intent,
            TabModelSelector tabModelSelector, ChromeInactivityTracker inactivityTracker) {
        String intentUrl = IntentHandler.getUrlFromIntent(intent);
        // If Chrome is launched by tapping the New Tab Item from the launch icon and
        // {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled, a new Tab with omnibox focused will be
        // shown on Startup.
        if (IntentHandler.shouldIntentShowNewTabOmniboxFocused(intent)) return false;

        // If user launches Chrome by tapping the app icon, the intentUrl is NULL;
        // If user taps the "New Tab" item from the app icon, the intentUrl will be chrome://newtab,
        // and UrlUtilities.isCanonicalizedNTPUrl(intentUrl) returns true.
        // If user taps the "New Incognito Tab" item from the app icon, skip here and continue the
        // following checks.
        if (UrlUtilities.isCanonicalizedNTPUrl(intentUrl)
                && ReturnToChromeUtil.shouldShowStartSurfaceAsTheHomePage(context)
                && !intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false)) {
            return true;
        }

        boolean isStartSurfaceEnabled = ReturnToChromeUtil.isStartSurfaceEnabled(context);

        // If Start surface is enabled and there's no tab existing, handle the initial tab creation.
        if (isStartSurfaceEnabled && IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeUtil.getTotalTabCount(tabModelSelector) <= 0) {
            return true;
        }

        // Checks whether to hide Start surface when last visited tab is a search result page.
        if (isStartSurfaceEnabled && shouldHideStartSurfaceWhenLastVisitedTabIsSRP()) return false;

        // Checks whether to show the Start surface / grid Tab switcher due to feature flag
        // TAB_SWITCHER_ON_RETURN_MS.
        long lastBackgroundedTimeMillis = inactivityTracker.getLastBackgroundedTimeMs();
        boolean tabSwitcherOnReturn = IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeUtil.shouldShowTabSwitcher(lastBackgroundedTimeMillis);

        // If the overview page won't be shown on startup, stops here.
        if (!tabSwitcherOnReturn) return false;

        if (isStartSurfaceEnabled) {
            if (StartSurfaceConfiguration.CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP.getValue()) {
                // We only check the sync status when flag CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP
                // and the Start surface are both enabled.
                return ReturnToChromeUtil.isPrimaryAccountSync();
            } else if (!TextUtils.isEmpty(
                               StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue())) {
                return ReturnToChromeUtil.userBehaviourSupported();
            }
        }

        // If Start surface is disable and should show the Grid tab switcher at startup, or flag
        // CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP and behavioural targeting flag aren't enabled,
        // return true here.
        return true;
    }

    /**
     * Returns whether user has a primary account with syncing on.
     */
    @VisibleForTesting
    public static boolean isPrimaryAccountSync() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.PRIMARY_ACCOUNT_SYNC, false);
    }

    /**
     * Returns whether to show the Start surface at startup based on whether user has done the
     * targeted behaviour.
     */
    public static boolean userBehaviourSupported() {
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        long nextDecisionTimestamp =
                manager.readLong(ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS,
                        INVALID_DECISION_TIMESTAMP);
        boolean noPreviousHistory = nextDecisionTimestamp == INVALID_DECISION_TIMESTAMP;
        // If this is the first time we make a decision, don't show the Start surface at startup.
        if (noPreviousHistory) {
            resetTargetBehaviourAndNextDecisionTime(false, null);
            return false;
        }

        boolean previousResult = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.START_SHOW_ON_STARTUP, false);
        // Returns the current decision before the next decision timestamp.
        if (System.currentTimeMillis() < nextDecisionTimestamp) {
            return previousResult;
        }

        // Shows the start surface st startup for a period of time if the behaviour tests return
        // positive, otherwise, hides it.
        String behaviourType = StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue();
        // The behaviour type strings can contain
        // 1. A feature name, in which case the prefs with the click counts are used to make
        //    decision.
        // 2. Just "all", in which case the threshold is applied to any of the feature usage.
        // 3. Prefixed: "model_<feature>", in which case we still use the click count prefs for
        //    making decision, but also record a comparison histogram with result from segmentation
        //    platform.
        // 4. Just "model", in which case we do not use click count prefs and instead just use the
        //    result from segmentation.
        boolean shouldAskModel = behaviourType.startsWith("model");
        boolean shouldUseCodeResult = !behaviourType.equals("model");
        String key = getBehaviourType(behaviourType);
        boolean resetAllCounts = false;
        int threshold = StartSurfaceConfiguration.USER_CLICK_THRESHOLD.getValue();

        boolean resultFromCode = false;
        boolean resultFromModel = false;

        if (shouldUseCodeResult) {
            if (TextUtils.equals(
                        "all", StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue())) {
                resultFromCode =
                        manager.readInt(ChromePreferenceKeys.TAP_MV_TILES_COUNT) >= threshold
                        || manager.readInt(ChromePreferenceKeys.TAP_FEED_CARDS_COUNT) >= threshold
                        || manager.readInt(ChromePreferenceKeys.OPEN_NEW_TAB_PAGE_COUNT)
                                >= threshold
                        || manager.readInt(ChromePreferenceKeys.OPEN_RECENT_TABS_COUNT) >= threshold
                        || manager.readInt(ChromePreferenceKeys.OPEN_HISTORY_COUNT) >= threshold;
                resetAllCounts = true;
            } else {
                assert key != null;
                int clicks = manager.readInt(key, 0);
                resultFromCode = clicks >= threshold;
            }
        }
        if (shouldAskModel) {
            // When segmentation is not ready with results yet, use the result from click count
            // prefs. If we do not have that too, then we do not want to switch the current behavior
            // frequently, so use the existing setting to show start or not.
            boolean defaultResult = shouldUseCodeResult ? resultFromCode : previousResult;
            resultFromModel = getBehaviourResultFromSegmentation(defaultResult);
            if (shouldUseCodeResult) {
                // Record a comparison between segmentation and hard coded logic when code result
                // should be used.
                recordSegmentationResultComparison(resultFromCode);
            } else {
                // Clear all the prefs state, the result does not matter in this case.
                resetAllCounts = true;
            }
        }

        boolean showStartOnStartup = shouldUseCodeResult ? resultFromCode : resultFromModel;
        if (resetAllCounts) {
            resetTargetBehaviourAndNextDecisionTimeForAllCounts(showStartOnStartup);
        } else {
            assert key != null;
            resetTargetBehaviourAndNextDecisionTime(showStartOnStartup, key);
        }
        return showStartOnStartup;
    }

    /**
     * Returns the ChromePreferenceKeys of the type to record in the SharedPreference.
     * @param behaviourType: the type of targeted behaviour.
     */
    private static @Nullable String getBehaviourType(String behaviourType) {
        switch (behaviourType) {
            case "mv_tiles":
            case "model_mv_tiles":
                return ChromePreferenceKeys.TAP_MV_TILES_COUNT;
            case "feeds":
            case "model_feeds":
                return ChromePreferenceKeys.TAP_FEED_CARDS_COUNT;
            case "open_new_tab":
            case "model_open_new_tab":
                return ChromePreferenceKeys.OPEN_NEW_TAB_PAGE_COUNT;
            case "open_history":
            case "model_open_history":
                return ChromePreferenceKeys.OPEN_HISTORY_COUNT;
            case "open_recent_tabs":
            case "model_open_recent_tabs":
                return ChromePreferenceKeys.OPEN_RECENT_TABS_COUNT;
            default:
                // Valid when the type is "model" when the decision is made by segmentation model.
                return null;
        }
    }

    private static void resetTargetBehaviourAndNextDecisionTime(
            boolean showStartOnStartup, @Nullable String behaviourTypeKey) {
        resetTargetBehaviourAndNextDecisionTimeImpl(showStartOnStartup);

        if (behaviourTypeKey != null) {
            SharedPreferencesManager.getInstance().removeKey(behaviourTypeKey);
        }
    }

    private static void resetTargetBehaviourAndNextDecisionTimeForAllCounts(
            boolean showStartOnStartup) {
        resetTargetBehaviourAndNextDecisionTimeImpl(showStartOnStartup);
        resetAllCounts();
    }

    private static void resetTargetBehaviourAndNextDecisionTimeImpl(boolean showStartOnStartup) {
        long nextDecisionTime = System.currentTimeMillis();

        if (showStartOnStartup) {
            nextDecisionTime += MILLISECONDS_PER_DAY
                    * StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP.getValue();
        } else {
            nextDecisionTime += MILLISECONDS_PER_DAY
                    * StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD.getValue();
        }
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.START_SHOW_ON_STARTUP, showStartOnStartup);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.START_NEXT_SHOW_ON_STARTUP_DECISION_MS, nextDecisionTime);
    }

    private static void resetAllCounts() {
        SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.TAP_MV_TILES_COUNT);
        SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.TAP_FEED_CARDS_COUNT);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OPEN_NEW_TAB_PAGE_COUNT);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OPEN_RECENT_TABS_COUNT);
        SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.OPEN_HISTORY_COUNT);
    }

    // Constants with ShowChromeStartSegmentationResult in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ShowChromeStartSegmentationResult.UNINITIALIZED,
            ShowChromeStartSegmentationResult.DONT_SHOW, ShowChromeStartSegmentationResult.SHOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShowChromeStartSegmentationResult {
        int UNINITIALIZED = 0;
        int DONT_SHOW = 1;
        int SHOW = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Returns whether to show Start surface based on segmentation result. When unavailable, returns
     * default value given.
     */
    private static boolean getBehaviourResultFromSegmentation(boolean defaultResult) {
        @ShowChromeStartSegmentationResult
        int resultEnum = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT);
        RecordHistogram.recordEnumeratedHistogram(
                "Startup.Android.ShowChromeStartSegmentationResult", resultEnum,
                ShowChromeStartSegmentationResult.NUM_ENTRIES);
        switch (resultEnum) {
            case ShowChromeStartSegmentationResult.DONT_SHOW:
                return false;
            case ShowChromeStartSegmentationResult.SHOW:
                return true;

            case ShowChromeStartSegmentationResult.UNINITIALIZED:
            default:
                return defaultResult;
        }
    }

    // Constants with ShowChromeStartSegmentationResultComparison in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ShowChromeStartSegmentationResultComparison.UNINITIALIZED,
            ShowChromeStartSegmentationResultComparison.SEGMENTATION_ENABLED_LOGIC_ENABLED,
            ShowChromeStartSegmentationResultComparison.SEGMENTATION_ENABLED_LOGIC_DISABLED,
            ShowChromeStartSegmentationResultComparison.SEGMENTATION_DISABLED_LOGIC_ENABLED,
            ShowChromeStartSegmentationResultComparison.SEGMENTATION_DISABLED_LOGIC_DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface ShowChromeStartSegmentationResultComparison {
        int UNINITIALIZED = 0;
        int SEGMENTATION_ENABLED_LOGIC_ENABLED = 1;
        int SEGMENTATION_ENABLED_LOGIC_DISABLED = 2;
        int SEGMENTATION_DISABLED_LOGIC_ENABLED = 3;
        int SEGMENTATION_DISABLED_LOGIC_DISABLED = 4;
        int NUM_ENTRIES = 5;
    }

    /*
     * Records UMA to compare the result of segmentation platform with hard coded logics.
     */
    private static void recordSegmentationResultComparison(boolean existingResult) {
        @ShowChromeStartSegmentationResult
        int segmentationResult = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT);
        @ShowChromeStartSegmentationResultComparison
        int comparison = ShowChromeStartSegmentationResultComparison.UNINITIALIZED;
        switch (segmentationResult) {
            case ShowChromeStartSegmentationResult.UNINITIALIZED:
                comparison = ShowChromeStartSegmentationResultComparison.UNINITIALIZED;
                break;
            case ShowChromeStartSegmentationResult.SHOW:
                comparison = existingResult ? ShowChromeStartSegmentationResultComparison
                                                      .SEGMENTATION_ENABLED_LOGIC_ENABLED
                                            : ShowChromeStartSegmentationResultComparison
                                                      .SEGMENTATION_ENABLED_LOGIC_DISABLED;
                break;
            case ShowChromeStartSegmentationResult.DONT_SHOW:
                comparison = existingResult ? ShowChromeStartSegmentationResultComparison
                                                      .SEGMENTATION_DISABLED_LOGIC_ENABLED
                                            : ShowChromeStartSegmentationResultComparison
                                                      .SEGMENTATION_DISABLED_LOGIC_DISABLED;
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Startup.Android.ShowChromeStartSegmentationResultComparison", comparison,
                ShowChromeStartSegmentationResultComparison.NUM_ENTRIES);
    }

    /**
     * Called when a targeted behaviour happens. It may increase the count if the corresponding
     * behaviour targeting type is set.
     */
    @VisibleForTesting
    public static void onUIClicked(String chromePreferenceKey) {
        String type = StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue();
        if (TextUtils.isEmpty(type)
                || (!TextUtils.equals("all", type)
                        && !TextUtils.equals(getBehaviourType(type), chromePreferenceKey))) {
            return;
        }

        int currentCount = SharedPreferencesManager.getInstance().readInt(chromePreferenceKey, 0);
        SharedPreferencesManager.getInstance().writeInt(chromePreferenceKey, currentCount + 1);
    }

    /**
     * Called when the "New Tab" from menu or "+" button is clicked. The count is only recorded when
     * the behavioural targeting is enabled on the Start surface.
     */
    public static void onNewTabOpened() {
        onUIClicked(ChromePreferenceKeys.OPEN_NEW_TAB_PAGE_COUNT);
    }

    /**
     * Returns whether Start surface should be hidden when last visited tab is a search result page.
     */
    private static boolean shouldHideStartSurfaceWhenLastVisitedTabIsSRP() {
        return StartSurfaceConfiguration.HIDE_START_WHEN_LAST_VISITED_TAB_IS_SRP.getValue()
                && SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false);
    }
}
