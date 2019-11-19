// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.speech.RecognizerIntent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A utility {@code class} meant to help determine whether or not certain features are supported by
 * this device.
 *
 * This utility class also contains support for cached feature flags that must take effect on
 * startup before native is initialized but are set via native code. The caching is done in
 * {@link android.content.SharedPreferences}, which is available in Java immediately.
 *
 * When adding a new cached flag, it is common practice to use a static Boolean in this file to
 * track whether the feature is enabled. A static method that returns the static Boolean can
 * then be added to this file allowing client code to query whether the feature is enabled. The
 * first time the method is called, the static Boolean should be set to the corresponding shared
 * preference. After native is initialized, the shared preference will be updated to reflect the
 * native flag value (e.g. the actual experimental feature flag value).
 *
 * When using a cached flag, the static Boolean should be the source of truth for whether the
 * feature is turned on for the current session. As such, always rely on the static Boolean
 * when determining whether the corresponding experimental behavior should be enabled. When
 * querying whether a cached feature is enabled from native, an @CalledByNative method can be
 * exposed in this file to allow feature_utilities.cc to retrieve the cached value.
 *
 * For cached flags that are queried before native is initialized, when a new experiment
 * configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart, when the static Boolean is reset to the newly cached
 * value in shared preferences.
 */
public class FeatureUtilities {
    /**
     * Key for whether DownloadResumptionBackgroundTask should load native in service manager only
     * mode.
     * Default value is false.
     */
    private static final String SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY =
            "service_manager_for_download_resumption";

    /**
     * Key for whether PrefetchBackgroundTask should load native in service manager only mode.
     * Default value is false.
     */
    private static final String SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY =
            "service_manager_for_background_prefetch";

    private static final String INTEREST_FEED_CONTENT_SUGGESTIONS_KEY =
            "interest_feed_content_suggestions";

    /**
     * Whether or not the download auto-resumption is enabled in native.
     * Default value is true.
     */
    private static final String DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY =
            "download_auto_resumption_in_native";

    /**
     * Whether or not the bottom toolbar is enabled.
     * Default value is false.
     */
    private static final String BOTTOM_TOOLBAR_ENABLED_KEY = "bottom_toolbar_enabled";

    /**
     * Whether or not the adaptive toolbar is enabled.
     * Default value is true.
     */
    private static final String ADAPTIVE_TOOLBAR_ENABLED_KEY = "adaptive_toolbar_enabled";

    /**
     * Whether or not the labeled bottom toolbar is enabled.
     * Default value is false.
     */
    private static final String LABELED_BOTTOM_TOOLBAR_ENABLED_KEY =
            "labeled_bottom_toolbar_enabled";

    /**
     * Whether or not night mode is available.
     * Default value is false.
     */
    private static final String NIGHT_MODE_AVAILABLE_KEY = "night_mode_available";

    /**
     * Whether or not night mode should set "light" as the default option.
     * Default value is false.
     */
    private static final String NIGHT_MODE_DEFAULT_TO_LIGHT = "night_mode_default_to_light";

    /**
     * Whether or not night mode is available for custom tabs.
     * Default value is false.
     */
    private static final String NIGHT_MODE_CCT_AVAILABLE_KEY = "night_mode_cct_available";

    /**
     * Whether or not command line on non-rooted devices is enabled.
     * Default value is false.
     */
    private static final String COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY =
            "command_line_on_non_rooted_enabled";

    /**
     * Whether or not the start surface is enabled.
     * Default value is false.
     */
    private static final String START_SURFACE_ENABLED_KEY = "start_surface_enabled";

    /**
     * Whether or not the grid tab switcher is enabled.
     * Default value is false.
     */
    private static final String GRID_TAB_SWITCHER_ENABLED_KEY = "grid_tab_switcher_enabled";

    /**
     * Whether or not the tab group is enabled.
     * Default value is false.
     */
    private static final String TAB_GROUPS_ANDROID_ENABLED_KEY = "tab_group_android_enabled";

    /**
     * Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task prioritization
     * experiment is enabled). Default value is true.
     */
    private static final String PRIORITIZE_BOOTSTRAP_TASKS_KEY = "prioritize_bootstrap_tasks";

    /**
     * Whether warming up network service is enabled.
     * Default value is false.
     */
    private static final String NETWORK_SERVICE_WARM_UP_ENABLED_KEY =
            "network_service_warm_up_enabled";

    /**
     * Key to cache whether immersive ui mode is enabled.
     */
    private static final String IMMERSIVE_UI_MODE_ENABLED = "immersive_ui_mode_enabled";

    /**
     * Key to cache whether
     * {@link ChromeFeatureList#SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT} is enabled.
     */
    private static final String SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT =
            "swap_pixel_format_to_fix_convert_from_translucent";

    /**
     * Whether or not we should directly open the dialer when a click to call notification is
     * received. Default value is false.
     */
    private static final String CLICK_TO_CALL_OPEN_DIALER_DIRECTLY_KEY =
            "click_to_call_open_dialer_directly";

    private static Map<String, Boolean> sFlags = new HashMap<>();
    private static Boolean sHasRecognitionIntentHandler;
    private static String sReachedCodeProfilerTrialGroup;

    /**
     * Determines whether or not the {@link RecognizerIntent#ACTION_WEB_SEARCH} {@link Intent}
     * is handled by any {@link android.app.Activity}s in the system.  The result will be cached for
     * future calls.  Passing {@code false} to {@code useCachedValue} will force it to re-query any
     * {@link android.app.Activity}s that can process the {@link Intent}.
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported.  {@code false} otherwise.
     */
    public static boolean isRecognitionIntentPresent(boolean useCachedValue) {
        ThreadUtils.assertOnUiThread();
        if (sHasRecognitionIntentHandler == null || !useCachedValue) {
            List<ResolveInfo> activities = PackageManagerUtils.queryIntentActivities(
                    new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH), 0);
            sHasRecognitionIntentHandler = !activities.isEmpty();
        }

        return sHasRecognitionIntentHandler;
    }

    /**
     * Records the current custom tab visibility state with native-side feature utilities.
     * @param visible Whether a custom tab is visible.
     */
    public static void setCustomTabVisible(boolean visible) {
        FeatureUtilitiesJni.get().setCustomTabVisible(visible);
    }

    /**
     * Records whether the activity is in multi-window mode with native-side feature utilities.
     * @param isInMultiWindowMode Whether the activity is in Android N multi-window mode.
     */
    public static void setIsInMultiWindowMode(boolean isInMultiWindowMode) {
        FeatureUtilitiesJni.get().setIsInMultiWindowMode(isInMultiWindowMode);
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheNativeFlags() {
        cacheCommandLineOnNonRootedEnabled();
        FirstRunUtils.cacheFirstRunPrefs();
        cacheBottomToolbarEnabled();
        cacheAdaptiveToolbarEnabled();
        cacheLabeledBottomToolbarEnabled();
        cacheNightModeAvailable();
        cacheNightModeDefaultToLight();
        cacheNightModeForCustomTabsAvailable();
        cacheDownloadAutoResumptionEnabledInNative();
        cachePrioritizeBootstrapTasks();
        cacheFeedEnabled();
        cacheNetworkServiceWarmUpEnabled();
        cacheImmersiveUiModeEnabled();
        cacheSwapPixelFormatToFixConvertFromTranslucentEnabled();
        cacheReachedCodeProfilerTrialGroup();
        cacheStartSurfaceEnabled();
        cacheClickToCallOpenDialerDirectlyEnabled();

        if (isHighEndPhone()) cacheGridTabSwitcherEnabled();
        if (isHighEndPhone()) cacheTabGroupsAndroidEnabled();

        // Propagate REACHED_CODE_PROFILER feature value to LibraryLoader. This can't be done in
        // LibraryLoader itself because it lives in //base and can't depend on ChromeFeatureList.
        LibraryLoader.setReachedCodeProfilerEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * Caches flags that are enabled in ServiceManager only mode and must take effect on startup but
     * are set via native code. This function needs to be called in ServiceManager only mode to mark
     * these field trials as active, otherwise histogram data recorded in ServiceManager only mode
     * won't be tagged with their corresponding field trial experiments.
     */
    public static void cacheNativeFlagsForServiceManagerOnlyMode() {
        // TODO(crbug.com/995355): Move other related flags from {@link cacheNativeFlags} to here.
        cacheServiceManagerForDownloadResumption();
        cacheServiceManagerForBackgroundPrefetch();
    }

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public static boolean isTabModelMergingEnabled() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)) {
            return false;
        }
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.M;
    }

    private static void cacheServiceManagerForDownloadResumption() {
        cacheFlag(SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY,
                ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD);
    }

    /**
     * @return if DownloadResumptionBackgroundTask should load native in service manager only mode.
     */
    public static boolean isServiceManagerForDownloadResumptionEnabled() {
        return isFlagEnabled(SERVICE_MANAGER_FOR_DOWNLOAD_RESUMPTION_KEY, false);
    }

    public static void cacheServiceManagerForBackgroundPrefetch() {
        cacheFlag(SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY,
                ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH);
    }

    /**
     * @return if PrefetchBackgroundTask should load native in service manager only mode.
     */
    public static boolean isServiceManagerForBackgroundPrefetchEnabled() {
        return isFlagEnabled(SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH_KEY, false) && isFeedEnabled();
    }

    /**
     * Cache the value of the flag whether or not to use Feed so it can be checked in Java before
     * native is loaded.
     */
    public static void cacheFeedEnabled() {
        cacheFlag(INTEREST_FEED_CONTENT_SUGGESTIONS_KEY,
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS);
    }

    /**
     * @return Whether or not the Feed is enabled (based on the cached value in SharedPrefs).
     */
    public static boolean isFeedEnabled() {
        return isFlagEnabled(INTEREST_FEED_CONTENT_SUGGESTIONS_KEY, false);
    }

    /**
     * @return Whether or not the download auto-resumptions should be enabled in native.
     */
    @CalledByNative
    public static boolean isDownloadAutoResumptionEnabledInNative() {
        return isFlagEnabled(DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY, true);
    }

    /**
     * Cache whether or not the bottom toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheBottomToolbarEnabled() {
        cacheFlag(BOTTOM_TOOLBAR_ENABLED_KEY, ChromeFeatureList.CHROME_DUET);
    }

    /**
     * Cache whether or not the adaptive toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheAdaptiveToolbarEnabled() {
        cacheFlag(ADAPTIVE_TOOLBAR_ENABLED_KEY, ChromeFeatureList.CHROME_DUET_ADAPTIVE);
    }

    /**
     * Cache whether or not the labeled bottom toolbar is enabled so on next startup, the value can
     * be made available immediately.
     */
    public static void cacheLabeledBottomToolbarEnabled() {
        cacheFlag(LABELED_BOTTOM_TOOLBAR_ENABLED_KEY, ChromeFeatureList.CHROME_DUET_LABELED);
    }

    /**
     * Cache whether or not download auto-resumptions are enabled in native so on next startup, the
     * value can be made available immediately.
     */
    private static void cacheDownloadAutoResumptionEnabledInNative() {
        cacheFlag(DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY,
                ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE);
    }

    @VisibleForTesting
    public static void setDownloadAutoResumptionEnabledInNativeForTesting(Boolean value) {
        sFlags.put(DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE_KEY, value);
    }

    /**
     * @return Whether or not the bottom toolbar is enabled.
     */
    public static boolean isBottomToolbarEnabled() {
        // TODO(crbug.com/944228): TabGroupsAndroid and ChromeDuet are incompatible for now.
        return isFlagEnabled(BOTTOM_TOOLBAR_ENABLED_KEY, false)
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext())
                && !isTabGroupsAndroidEnabled();
    }

    /**
     * Set whether the bottom toolbar is enabled for tests. Reset to null at the end of tests.
     */
    @VisibleForTesting
    public static void setIsBottomToolbarEnabledForTesting(Boolean enabled) {
        sFlags.put(BOTTOM_TOOLBAR_ENABLED_KEY, enabled);
    }

    /**
     * @return Whether or not the adaptive toolbar is enabled.
     */
    public static boolean isAdaptiveToolbarEnabled() {
        return isFlagEnabled(ADAPTIVE_TOOLBAR_ENABLED_KEY, true) && isBottomToolbarEnabled()
                && !isGridTabSwitcherEnabled();
    }

    /**
     * @return Whether or not the labeled bottom toolbar is enabled.
     */
    public static boolean isLabeledBottomToolbarEnabled() {
        return isFlagEnabled(LABELED_BOTTOM_TOOLBAR_ENABLED_KEY, false) && isBottomToolbarEnabled();
    }

    /**
     * Cache whether or not night mode is available (i.e. night mode experiment is enabled) so on
     * next startup, the value can be made available immediately.
     */
    public static void cacheNightModeAvailable() {
        boolean available = ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE)
                || (BuildInfo.isAtLeastQ()
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE_FOR_Q));
        SharedPreferencesManager.getInstance().writeBoolean(NIGHT_MODE_AVAILABLE_KEY, available);
    }

    /**
     * @return Whether or not night mode experiment is enabled (i.e. night mode experiment is
     *         enabled).
     */
    public static boolean isNightModeAvailable() {
        return isFlagEnabled(NIGHT_MODE_AVAILABLE_KEY, true);
    }

    /**
     * Toggles whether the night mode experiment is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setNightModeAvailableForTesting(@Nullable Boolean available) {
        sFlags.put(NIGHT_MODE_AVAILABLE_KEY, available);
    }

    /**
     * Cache whether or not to default to the light theme when the night mode feature is enabled.
     */
    public static void cacheNightModeDefaultToLight() {
        // Do not cache on Q (where defaulting to light theme does not apply) or if night mode is
        // not enabled.
        if (BuildInfo.isAtLeastQ()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NIGHT_MODE)) {
            return;
        }

        String lightModeDefaultParam = "default_light_theme";
        boolean lightModeAsDefault = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_NIGHT_MODE, lightModeDefaultParam, true);

        SharedPreferencesManager.getInstance().writeBoolean(
                NIGHT_MODE_DEFAULT_TO_LIGHT, lightModeAsDefault);
    }

    /**
     * @return Whether or not to default to the light theme when the night mode feature is enabled.
     */
    public static boolean isNightModeDefaultToLight() {
        if (BuildInfo.isAtLeastQ()) {
            return false;
        }
        return isFlagEnabled(NIGHT_MODE_DEFAULT_TO_LIGHT, true);
    }

    /**
     * Toggles whether the night mode experiment is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setNightModeDefaultToLightForTesting(@Nullable Boolean available) {
        sFlags.put(NIGHT_MODE_DEFAULT_TO_LIGHT, available);
    }

    /**
     * Cache whether or not night mode is available for custom tabs (i.e. night mode experiment is
     * enabled), so the value is immediately available on next start-up.
     */
    public static void cacheNightModeForCustomTabsAvailable() {
        cacheFlag(NIGHT_MODE_CCT_AVAILABLE_KEY, ChromeFeatureList.ANDROID_NIGHT_MODE_CCT);
    }

    /**
     * @return Whether or not night mode experiment is enabled (i.e. night mode experiment is
     *         enabled) for custom tabs.
     */
    public static boolean isNightModeForCustomTabsAvailable() {
        return isFlagEnabled(NIGHT_MODE_CCT_AVAILABLE_KEY, true);
    }

    /**
     * Toggles whether the night mode for custom tabs experiment is enabled. Must only be used for
     * testing. Should be reset back to NULL after the test has finished.
     */
    public static void setNightModeForCustomTabsAvailableForTesting(Boolean available) {
        sFlags.put(NIGHT_MODE_CCT_AVAILABLE_KEY, available);
    }

    /**
     * Cache whether or not command line is enabled on non-rooted devices.
     */
    private static void cacheCommandLineOnNonRootedEnabled() {
        cacheFlag(COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY,
                ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
    }

    public static boolean isCommandLineOnNonRootedEnabled() {
        return isFlagEnabled(COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY, false);
    }

    /**
     * @return Whether or not the download progress infobar is enabled.
     */
    public static boolean isDownloadProgressInfoBarEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR);
    }

    private static void cacheStartSurfaceEnabled() {
        cacheFlag(START_SURFACE_ENABLED_KEY, ChromeFeatureList.START_SURFACE_ANDROID);
    }

    /**
     * @return Whether the Start Surface is enabled.
     */
    public static boolean isStartSurfaceEnabled() {
        return isFlagEnabled(START_SURFACE_ENABLED_KEY, false);
    }

    private static void cacheGridTabSwitcherEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(GRID_TAB_SWITCHER_ENABLED_KEY,
                !DeviceClassManager.enableAccessibilityLayout()
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID)
                        && TabManagementModuleProvider.getDelegate() != null);
    }

    /**
     * @return Whether the Grid Tab Switcher UI is enabled and available for use.
     */
    public static boolean isGridTabSwitcherEnabled() {
        // TODO(yusufo): AccessibilityLayout check should not be here and the flow should support
        // changing that setting while Chrome is alive.
        // Having Tab Groups or Start implies Grid Tab Switcher.
        return isFlagEnabled(GRID_TAB_SWITCHER_ENABLED_KEY, false) || isTabGroupsAndroidEnabled()
                || isStartSurfaceEnabled();
    }

    /**
     * Toggles whether the Grid Tab Switcher is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setGridTabSwitcherEnabledForTesting(@Nullable Boolean enabled) {
        sFlags.put(GRID_TAB_SWITCHER_ENABLED_KEY, enabled);
    }

    private static void cacheTabGroupsAndroidEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(TAB_GROUPS_ANDROID_ENABLED_KEY,
                !DeviceClassManager.enableAccessibilityLayout()
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)
                        && TabManagementModuleProvider.getDelegate() != null && isHighEndPhone());
    }

    /**
     * @return Whether the tab group feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidEnabled() {
        return isFlagEnabled(TAB_GROUPS_ANDROID_ENABLED_KEY, false);
    }

    /**
     * Toggles whether the Tab Group is enabled for testing. Should be reset back to null after the
     * test has finished.
     */
    @VisibleForTesting
    public static void setTabGroupsAndroidEnabledForTesting(@Nullable Boolean available) {
        sFlags.put(TAB_GROUPS_ANDROID_ENABLED_KEY, available);
    }

    /**
     * Toggles whether the StartSurface is enabled for testing. Should be reset back to null after
     * the test has finished.
     */
    @VisibleForTesting
    public static void setStartSurfaceEnabledForTesting(@Nullable Boolean isEnabled) {
        sFlags.put(START_SURFACE_ENABLED_KEY, isEnabled);
    }

    private static boolean isHighEndPhone() {
        return !SysUtils.isLowEndDevice()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }

    /**
     * @return Whether the tab group ui improvement feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidUiImprovementsEnabled() {
        return isTabGroupsAndroidEnabled()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID);
    }

    /**
     * @return Whether the tab group continuation feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidContinuationEnabled() {
        return isTabGroupsAndroidEnabled()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
    }

    /**
     * @return Whether this device is running Android Go. This is assumed when we're running Android
     * O or later and we're on a low-end device.
     */
    public static boolean isAndroidGo() {
        return SysUtils.isLowEndDevice()
                && android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O;
    }

    /**
     * Cache whether or not bootstrap tasks should be prioritized so on next startup, the value
     * can be made available immediately.
     */
    public static void cachePrioritizeBootstrapTasks() {
        cacheFlag(PRIORITIZE_BOOTSTRAP_TASKS_KEY, ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS);
    }

    /**
     * @return Whether or not bootstrap tasks should be prioritized (i.e. bootstrap task
     *         prioritization experiment is enabled).
     */
    public static boolean shouldPrioritizeBootstrapTasks() {
        return isFlagEnabled(PRIORITIZE_BOOTSTRAP_TASKS_KEY, true);
    }

    /**
     * Cache whether warming up network service process is enabled, so that the value
     * can be made available immediately on next start up.
     */
    private static void cacheNetworkServiceWarmUpEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(NETWORK_SERVICE_WARM_UP_ENABLED_KEY,
                FeatureUtilitiesJni.get().isNetworkServiceWarmUpEnabled());
    }

    /**
     * @return whether warming up network service is enabled.
     */
    public static boolean isNetworkServiceWarmUpEnabled() {
        return isFlagEnabled(NETWORK_SERVICE_WARM_UP_ENABLED_KEY, false);
    }

    private static void cacheImmersiveUiModeEnabled() {
        cacheFlag(IMMERSIVE_UI_MODE_ENABLED, ChromeFeatureList.IMMERSIVE_UI_MODE);
    }

    /**
     * @return Whether immersive ui mode is enabled.
     */
    public static boolean isImmersiveUiModeEnabled() {
        return isFlagEnabled(IMMERSIVE_UI_MODE_ENABLED, false);
    }

    /**
     * Returns whether to use {@link Window#setFormat()} to undo opacity change caused by
     * {@link Activity#convertFromTranslucent()}.
     */
    public static boolean isSwapPixelFormatToFixConvertFromTranslucentEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT, true);
    }

    public static void cacheSwapPixelFormatToFixConvertFromTranslucentEnabled() {
        cacheFlag(SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT);
    }

    /**
     * Cache the value of the flag whether or not to directly open the dialer for click to call.
     */
    public static void cacheClickToCallOpenDialerDirectlyEnabled() {
        cacheFlag(CLICK_TO_CALL_OPEN_DIALER_DIRECTLY_KEY,
                ChromeFeatureList.CLICK_TO_CALL_OPEN_DIALER_DIRECTLY);
    }

    /**
     * @return Whether or not we should directly open dialer for click to call (based on the cached
     *         value in SharedPrefs).
     */
    public static boolean isClickToCallOpenDialerDirectlyEnabled() {
        return isFlagEnabled(CLICK_TO_CALL_OPEN_DIALER_DIRECTLY_KEY, false);
    }

    /**
     * Toggles whether experiment for opening dialer directly in click to call is enabled for
     * testing. Should be reset back to null after the test has finished.
     */
    @VisibleForTesting
    public static void setIsClickToCallOpenDialerDirectlyEnabledForTesting(
            @Nullable Boolean isEnabled) {
        sFlags.put(CLICK_TO_CALL_OPEN_DIALER_DIRECTLY_KEY, isEnabled);
    }

    /**
     * Caches the trial group of the reached code profiler feature to be using on next startup.
     */
    private static void cacheReachedCodeProfilerTrialGroup() {
        // Make sure that the existing value is saved in a static variable before overwriting it.
        if (sReachedCodeProfilerTrialGroup == null) {
            getReachedCodeProfilerTrialGroup();
        }

        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP_KEY,
                FieldTrialList.findFullName(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * @return The trial group of the reached code profiler.
     */
    @CalledByNative
    public static String getReachedCodeProfilerTrialGroup() {
        if (sReachedCodeProfilerTrialGroup == null) {
            sReachedCodeProfilerTrialGroup = SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP_KEY, "");
        }

        return sReachedCodeProfilerTrialGroup;
    }

    private static void cacheFlag(String preferenceName, String featureName) {
        SharedPreferencesManager.getInstance().writeBoolean(
                preferenceName, ChromeFeatureList.isEnabled(featureName));
    }

    private static boolean isFlagEnabled(String preferenceName, boolean defaultValue) {
        Boolean flag = sFlags.get(preferenceName);
        if (flag == null) {
            flag = SharedPreferencesManager.getInstance().readBoolean(preferenceName, defaultValue);
            sFlags.put(preferenceName, flag);
        }
        return flag;
    }

    @NativeMethods
    interface Natives {
        void setCustomTabVisible(boolean visible);
        void setIsInMultiWindowMode(boolean isInMultiWindowMode);
        boolean isNetworkServiceWarmUpEnabled();
    }
}
