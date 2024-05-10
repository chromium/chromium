// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import androidx.annotation.AnyThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.cached_flags.CachedFieldTrialParameter;
import org.chromium.base.cached_flags.CachedFlag;
import org.chromium.base.cached_flags.CachedFlagUtils;
import org.chromium.base.cached_flags.CachedFlagsSafeMode;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.JankTrackerExperiment;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.notifications.chime.ChimeFeatures;
import org.chromium.chrome.browser.omaha.VersionNumberGetter;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuidePushNotificationManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.omnibox.OmniboxFeatures;

import java.util.ArrayList;
import java.util.List;

/** Caches the flags that Chrome might require before native is loaded in a later next run. */
public class ChromeCachedFlags {
    private static final ChromeCachedFlags INSTANCE = new ChromeCachedFlags();

    private boolean mIsFinishedCachingNativeFlags;

    /**
     * A list of field trial parameters that will be cached when starting minimal browser mode. See
     * {@link #cacheMinimalBrowserFlags()}.
     */
    private static final List<CachedFieldTrialParameter> MINIMAL_BROWSER_FIELD_TRIALS =
            List.of(
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

        CachedFlagUtils.cacheNativeFlags(
                ChromeFeatureList.sFlagsCachedFullBrowser, OmniboxFeatures.getFieldTrialsToCache());
        cacheAdditionalNativeFlags();

        List<CachedFieldTrialParameter> fieldTrialsToCache =
                List.of(
                        BackPressManager.TAB_HISTORY_RECOVER,
                        ChimeFeatures.ALWAYS_REGISTER,
                        CustomTabIntentDataProvider.AUTO_TRANSLATE_ALLOW_ALL_FIRST_PARTIES,
                        CustomTabIntentDataProvider.AUTO_TRANSLATE_PACKAGE_NAME_ALLOWLIST,
                        CustomTabIntentDataProvider.THIRD_PARTIES_DEFAULT_POLICY,
                        CustomTabIntentDataProvider.DENYLIST_ENTRIES,
                        CustomTabIntentDataProvider.ALLOWLIST_ENTRIES,
                        CustomTabIntentDataProvider.OMNIBOX_ALLOWED_PACKAGE_NAMES,
                        DseNewTabUrlManager.EEA_COUNTRY_ONLY,
                        DseNewTabUrlManager.SKIP_EEA_COUNTRY_CHECK,
                        DseNewTabUrlManager.SWAP_OUT_NTP,
                        GoogleBottomBarCoordinator.GOOGLE_BOTTOM_BAR_PARAM_BUTTON_LIST,
                        HubFieldTrial.FLOATING_ACTION_BUTTON,
                        HubFieldTrial.PANE_SWITCHER_USES_TEXT,
                        HubFieldTrial.SUPPORTS_OTHER_TABS,
                        HubFieldTrial.SUPPORTS_SEARCH,
                        HubFieldTrial.SUPPORTS_BOOKMARKS,
                        JankTrackerExperiment.JANK_TRACKER_DELAYED_START_MS,
                        MinimizedFeatureUtils.ICON_VARIANT,
                        MinimizedFeatureUtils.MANUFACTURER_EXCLUDE_LIST,
                        MultiWindowUtils.BACK_TO_BACK_CTA_CREATION_TIMESTAMP_DIFF_THRESHOLD_MS,
                        OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE,
                        ShoppingPersistedTabDataService
                                .SKIP_SHOPPING_PERSISTED_TAB_DATA_DELAYED_INITIALIZATION,
                        StartSurfaceConfiguration.IS_DOODLE_SUPPORTED,
                        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS,
                        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS,
                        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL,
                        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT,
                        StartSurfaceConfiguration
                                .SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS,
                        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS,
                        StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB,
                        StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START,
                        StartSurfaceConfiguration.START_SURFACE_OPEN_START_AS_HOMEPAGE,
                        StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT,
                        StartSurfaceConfiguration.LOGO_POLISH_LARGE_SIZE,
                        StartSurfaceConfiguration.LOGO_POLISH_MEDIUM_SIZE,
                        TabManagementFieldTrial.DELAY_TEMP_STRIP_TIMEOUT_MS,
                        HomeModulesMetricsUtils.HOME_MODULES_SHOW_ALL_MODULES,
                        HomeModulesMetricsUtils.HOME_MODULES_COMBINE_TABS,
                        TabResumptionModuleUtils.TAB_RESUMPTION_MAX_TILES_NUMBER,
                        TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_SEE_MORE,
                        TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE,
                        TabResumptionModuleUtils.TAB_RESUMPTION_V2,
                        TabStateFileManager.MIGRATE_STALE_TABS_CACHED_PARAM,
                        TabUiFeatureUtilities.ANIMATION_START_TIMEOUT_MS,
                        TabUiFeatureUtilities.ZOOMING_MIN_MEMORY,
                        TabUiFeatureUtilities.SKIP_SLOW_ZOOMING,
                        TabUiFeatureUtilities.DISABLE_STRIP_TO_CONTENT_DD,
                        TabUiFeatureUtilities.DISABLE_STRIP_TO_STRIP_DD,
                        TabUiFeatureUtilities.DISABLE_STRIP_TO_STRIP_DIFF_MODEL_DD,
                        TabUiFeatureUtilities.DISABLE_DRAG_TO_NEW_INSTANCE_DD,
                        TabUiFeatureUtilities.ENABLE_NON_SPLIT_MODE_TAB_DRAG_MANUFACTURER_ALLOWLIST,
                        ToolbarFeatures.DTC_TRANSITION_THRESHOLD_DP,
                        ToolbarFeatures.USE_TOOLBAR_BG_COLOR_FOR_STRIP_TRANSITION_SCRIM,
                        VersionNumberGetter.MIN_SDK_VERSION);

        tryToCatchMissingParameters(
                fieldTrialsToCache, OmniboxFeatures.getFieldTrialParamsToCache());
        CachedFlagUtils.cacheFieldTrialParameters(
                fieldTrialsToCache, OmniboxFeatures.getFieldTrialParamsToCache());

        CachedFlagsSafeMode.getInstance().onEndCheckpoint();
        mIsFinishedCachingNativeFlags = true;
    }

    private void tryToCatchMissingParameters(
            List<CachedFieldTrialParameter>... listsOfParamsToTest) {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        var paramsToTest = new ArrayList<CachedFieldTrialParameter>();
        for (List<CachedFieldTrialParameter> list : listsOfParamsToTest) {
            paramsToTest.addAll(list);
        }

        // All instances of CachedFieldTrialParameter should be manually passed to
        // CachedFeatureFlags.cacheFieldTrialParameters(). The following checking is a best-effort
        // attempt to try to catch accidental omissions. It cannot replace the list because some
        // instances might not be instantiated if the classes they belong to are not accessed yet.
        List<String> omissions = new ArrayList<>();
        for (CachedFieldTrialParameter trial : CachedFieldTrialParameter.getAllInstances()) {
            if (paramsToTest.contains(trial)) continue;
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
        // Propagate the BACKGROUND_THREAD_POOL feature value to LibraryLoader.
        LibraryLoader.setBackgroundThreadPoolEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.BACKGROUND_THREAD_POOL));

        // Propagate the CACHE_ACTIVITY_TASKID feature value to ApplicationStatus.
        ApplicationStatus.setCachingEnabled(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CACHE_ACTIVITY_TASKID));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void cacheMinimalBrowserFlagsTimeFromNativeTime() {
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                        System.currentTimeMillis());
    }

    public static long getLastCachedMinimalBrowserFlagsTimeMillis() {
        return ChromeSharedPreferences.getInstance()
                .readLong(
                        ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                        0);
    }

    @CalledByNative
    @AnyThread
    static boolean isEnabled(String featureName) {
        CachedFlag cachedFlag = ChromeFeatureList.sAllCachedFlags.get(featureName);
        assert cachedFlag != null;

        return cachedFlag.isEnabled();
    }
}
