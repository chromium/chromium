// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import androidx.annotation.AnyThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.FieldTrialList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingController;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.CachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFlag;
import org.chromium.chrome.browser.flags.CachedFlagUtils;
import org.chromium.chrome.browser.flags.CachedFlagsSafeMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.chime.ChimeFeatures;
import org.chromium.chrome.browser.omaha.VersionNumberGetter;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuidePushNotificationManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsFeatureHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

import java.util.ArrayList;
import java.util.List;

/**
 * Caches the flags that Chrome might require before native is loaded in a later next run.
 */
public class ChromeCachedFlags {
    private static final ChromeCachedFlags INSTANCE = new ChromeCachedFlags();

    private boolean mIsFinishedCachingNativeFlags;

    private static String sReachedCodeProfilerTrialGroup;

    /**
     * A list of field trial parameters that will be cached when starting minimal browser mode. See
     * {@link #cacheMinimalBrowserFlags()}.
     */
    private static final List<CachedFieldTrialParameter> MINIMAL_BROWSER_FIELD_TRIALS = List.of(
            // This is used by CustomTabsConnection implementation, which does not
            // necessarily start chrome.
            CustomTabActivity.EXPERIMENTS_FOR_AGSA_PARAMS);

    /**
     * @return The {@link ChromeCachedFlags} singleton.
     */
    public static ChromeCachedFlags getInstance() {
        return INSTANCE;
    }

    /**
     * Caches flags that are needed by Activities that launch before the native library is loaded
     * and stores them in SharedPreferences. Because this function is called during launch after the
     * library has loaded, they won't affect the next launch until Chrome is restarted.
     */
    public void cacheNativeFlags() {
        if (mIsFinishedCachingNativeFlags) return;
        FirstRunUtils.cacheFirstRunPrefs();

        CachedFlagUtils.cacheNativeFlags(ChromeFeatureList.sFlagsCachedFullBrowser);
        cacheAdditionalNativeFlags();

        //clang-format off
        List<CachedFieldTrialParameter> fieldTrialsToCache = List.of(
                BrandingController.BRANDING_CADENCE_MS,
                BrandingController.MAX_BLANK_TOOLBAR_TIMEOUT_MS,
                BrandingController.USE_TEMPORARY_STORAGE,
                BrandingController.ANIMATE_TOOLBAR_ICON_TRANSITION, ChimeFeatures.ALWAYS_REGISTER,
                FeedPlaceholderLayout.ENABLE_INSTANT_START_ANIMATION,
                OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE,
                OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_MERGE_CLIPBOARD_ON_NTP,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLER_MARGINS,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS,
                CustomTabIntentDataProvider.AUTO_TRANSLATE_ALLOW_ALL_FIRST_PARTIES,
                CustomTabIntentDataProvider.AUTO_TRANSLATE_PACKAGE_NAME_ALLOWLIST,
                CustomTabIntentDataProvider.THIRD_PARTIES_DEFAULT_POLICY,
                CustomTabIntentDataProvider.DENYLIST_ENTRIES,
                CustomTabIntentDataProvider.ALLOWLIST_ENTRIES,
                WarmupManager.SPARE_TAB_INITIALIZE_RENDERER,
                RestoreTabsFeatureHelper.RESTORE_TABS_PROMO_SKIP_FEATURE_ENGAGEMENT,
                StartSurfaceConfiguration.IS_DOODLE_SUPPORTED,
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS,
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS,
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL,
                StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER,
                StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT,
                StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS,
                StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS,
                StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB,
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY,
                StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START,
                StartSurfaceConfiguration.START_SURFACE_OPEN_START_AS_HOMEPAGE,
                StartSurfaceConfiguration.SURFACE_POLISH_OMNIBOX_COLOR,
                StartSurfaceConfiguration.SURFACE_POLISH_MOVE_DOWN_LOGO,
                StartSurfaceConfiguration.SURFACE_POLISH_LESS_BRAND_SPACE,
                StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT,
                StartSurfaceConfiguration.SURFACE_POLISH_USE_MAGIC_SPACE,
                TabUiFeatureUtilities.ZOOMING_MIN_MEMORY, TabUiFeatureUtilities.SKIP_SLOW_ZOOMING,
                TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR,
                TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_BUTTON_STYLE,
                TabManagementFieldTrial.DELAY_TEMP_STRIP_TIMEOUT_MS,
                TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO,
                TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED,
                VersionNumberGetter.MIN_SDK_VERSION,
                MinimizeAppAndCloseTabBackPressHandler.SYSTEM_BACK,
                BackPressManager.TAB_HISTORY_RECOVER);
        // clang-format on
        tryToCatchMissingParameters(fieldTrialsToCache);
        CachedFlagUtils.cacheFieldTrialParameters(fieldTrialsToCache);

        CachedFlagsSafeMode.getInstance().onEndCheckpoint();
        mIsFinishedCachingNativeFlags = true;
    }

    private void tryToCatchMissingParameters(List<CachedFieldTrialParameter> listed) {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        // All instances of CachedFieldTrialParameter should be manually passed to
        // CachedFeatureFlags.cacheFieldTrialParameters(). The following checking is a best-effort
        // attempt to try to catch accidental omissions. It cannot replace the list because some
        // instances might not be instantiated if the classes they belong to are not accessed yet.
        List<String> omissions = new ArrayList<>();
        for (CachedFieldTrialParameter trial : CachedFieldTrialParameter.getAllInstances()) {
            if (listed.contains(trial)) continue;
            if (MINIMAL_BROWSER_FIELD_TRIALS.contains(trial)) continue;
            omissions.add(trial.getFeatureName() + ":" + trial.getParameterName());
        }
        assert omissions.isEmpty()
            : "The following trials are not correctly cached: "
                + TextUtils.join(", ", omissions);
    }

    /**
     * Caches flags that are enabled in minimal browser mode and must take effect on startup but
     * are set via native code. This function needs to be called in minimal browser mode to mark
     * these field trials as active, otherwise histogram data recorded in minimal browser mode
     * won't be tagged with their corresponding field trial experiments.
     */
    public void cacheMinimalBrowserFlags() {
        cacheMinimalBrowserFlagsTimeFromNativeTime();
        CachedFlagUtils.cacheNativeFlags(ChromeFeatureList.sFlagsCachedInMinimalBrowser);
        CachedFlagUtils.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }

    /**
     * Caches a predetermined list of flags that must take effect on startup but are set via native
     * code.
     *
     * Do not add new simple boolean flags here, add them to {@link #cacheNativeFlags} instead.
     */
    public static void cacheAdditionalNativeFlags() {
        cacheReachedCodeProfilerTrialGroup();

        // Propagate REACHED_CODE_PROFILER feature value to LibraryLoader. This can't be done in
        // LibraryLoader itself because it lives in //base and can't depend on ChromeFeatureList.
        LibraryLoader.setReachedCodeProfilerEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.REACHED_CODE_PROFILER),
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.REACHED_CODE_PROFILER, "sampling_interval_us", 0));

        // Similarly, propagate the BACKGROUND_THREAD_POOL feature value to LibraryLoader.
        LibraryLoader.setBackgroundThreadPoolEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.BACKGROUND_THREAD_POOL));

        // Propagate the CACHE_ACTIVITY_TASKID feature value to ApplicationStatus.
        ApplicationStatus.setCachingEnabled(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CACHE_ACTIVITY_TASKID));
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
                ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP,
                FieldTrialList.findFullName(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * @return The trial group of the reached code profiler.
     */
    @CalledByNative
    public static String getReachedCodeProfilerTrialGroup() {
        if (sReachedCodeProfilerTrialGroup == null) {
            sReachedCodeProfilerTrialGroup = SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP, "");
        }

        return sReachedCodeProfilerTrialGroup;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void cacheMinimalBrowserFlagsTimeFromNativeTime() {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                System.currentTimeMillis());
    }

    public static long getLastCachedMinimalBrowserFlagsTimeMillis() {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS, 0);
    }

    @CalledByNative
    @AnyThread
    static boolean isEnabled(String featureName) {
        CachedFlag cachedFlag = ChromeFeatureList.sAllCachedFlags.get(featureName);
        assert cachedFlag != null;

        return cachedFlag.isEnabled();
    }
}
