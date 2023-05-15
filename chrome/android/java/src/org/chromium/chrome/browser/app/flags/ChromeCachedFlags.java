// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingController;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.CachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFlag;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.chime.ChimeFeatures;
import org.chromium.chrome.browser.omaha.VersionNumberGetter;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuidePushNotificationManager;
import org.chromium.chrome.browser.tab.state.FilePersistedTabDataStorage;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

import java.util.ArrayList;
import java.util.List;

/**
 * Caches the flags that Chrome might require before native is loaded in a later next run.
 */
public class ChromeCachedFlags {
    private boolean mIsFinishedCachingNativeFlags;

    private static final ChromeCachedFlags INSTANCE = new ChromeCachedFlags();

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

        // clang-format off
        List<CachedFlag> featuresToCache = List.of(ChromeFeatureList.sAppMenuMobileSiteOption,
                ChromeFeatureList.sBackGestureActivityTabProvider,
                ChromeFeatureList.sBackGestureRefactorAndroid,
                ChromeFeatureList.sBaselineGm3SurfaceColors,
                ChromeFeatureList.sBottomSheetGtsSupport,
                ChromeFeatureList.sCctAutoTranslate,
                ChromeFeatureList.sCctBottomBarSwipeUpGesture,
                ChromeFeatureList.sCctBrandTransparency,
                ChromeFeatureList.sCctFeatureUsage,
                ChromeFeatureList.sCctIncognito,
                ChromeFeatureList.sCctIncognitoAvailableToThirdParty,
                ChromeFeatureList.sCctIntentFeatureOverrides,
                ChromeFeatureList.sCctRemoveRemoteViewIds,
                ChromeFeatureList.sCctResizable90MaximumHeight,
                ChromeFeatureList.sCctResizableForThirdParties,
                ChromeFeatureList.sCctResizableSideSheet,
                ChromeFeatureList.sCctResizableSideSheetDiscoverFeedSettings,
                ChromeFeatureList.sCctResizableSideSheetForThirdParties,
                ChromeFeatureList.sCctRetainableStateInMemory,
                ChromeFeatureList.sCctToolbarCustomizations,
                ChromeFeatureList.sCloseTabSuggestions,
                ChromeFeatureList.sCloseTabSaveTabList,
                ChromeFeatureList.sCommandLineOnNonRooted,
                ChromeFeatureList.sCriticalPersistedTabData,
                ChromeFeatureList.sDelayTempStripRemoval,
                ChromeFeatureList.sDiscoverMultiColumn,
                ChromeFeatureList.sTabStripRedesign,
                ChromeFeatureList.sEarlyLibraryLoad,
                ChromeFeatureList.sFeedLoadingPlaceholder,
                ChromeFeatureList.sFoldableJankFix,
                ChromeFeatureList.sHideNonDisplayableAccountEmail,
                ChromeFeatureList.sIncognitoReauthenticationForAndroid,
                ChromeFeatureList.sInstanceSwitcher,
                ChromeFeatureList.sInstantStart,
                ChromeFeatureList.sInterestFeedV2,
                ChromeFeatureList.sOmniboxMatchToolbarAndStatusBarColor,
                ChromeFeatureList.sOmniboxModernizeVisualUpdate,
                ChromeFeatureList.sOmniboxMostVisitedTilesAddRecycledViewPool,
                ChromeFeatureList.sOptimizationGuidePushNotifications,
                ChromeFeatureList.sPaintPreviewDemo,
                ChromeFeatureList.sQueryTiles,
                ChromeFeatureList.sQueryTilesOnStart,
                ChromeFeatureList.sShouldIgnoreIntentSkipInternalCheck,
                ChromeFeatureList.sSpareTab,
                ChromeFeatureList.sStartSurfaceAndroid,
                ChromeFeatureList.sStartSurfaceDisabledFeedImprovement,
                ChromeFeatureList.sStartSurfaceReturnTime,
                ChromeFeatureList.sStartSurfaceRefactor,
                ChromeFeatureList.sStartSurfaceOnTablet,
                ChromeFeatureList.sStartSurfaceWithAccessibility,
                ChromeFeatureList.sStoreHoursAndroid,
                ChromeFeatureList.sSwapPixelFormatToFixConvertFromTranslucent,
                ChromeFeatureList.sTabGridLayoutAndroid,
                ChromeFeatureList.sTabGroupsAndroid,
                ChromeFeatureList.sTabGroupsContinuationAndroid,
                ChromeFeatureList.sTabGroupsForTablets,
                ChromeFeatureList.sTabToGTSAnimation,
                ChromeFeatureList.sToolbarUseHardwareBitmapDraw,
                ChromeFeatureList.sUseChimeAndroidSdk,
                ChromeFeatureList.sUseLibunwindstackNativeUnwinderAndroid,
                ChromeFeatureList.sWebApkTrampolineOnInitialIntent);

        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
        CachedFeatureFlags.cacheAdditionalNativeFlags();

        List<CachedFieldTrialParameter> fieldTrialsToCache = List.of(
                BrandingController.BRANDING_CADENCE_MS,
                BrandingController.MAX_BLANK_TOOLBAR_TIMEOUT_MS,
                BrandingController.USE_TEMPORARY_STORAGE,
                BrandingController.ANIMATE_TOOLBAR_ICON_TRANSITION,
                ChimeFeatures.ALWAYS_REGISTER,
                DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT,
                DeviceClassManager.GTS_LOW_END_SUPPORT,
                FeedPlaceholderLayout.ENABLE_INSTANT_START_ANIMATION,
                FilePersistedTabDataStorage.DELAY_SAVES_UNTIL_DEFERRED_STARTUP_PARAM,
                OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE,
                OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_MERGE_CLIPBOARD_ON_NTP,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLER_MARGINS,
                OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS,
                OmniboxFeatures.TAB_STRIP_REDESIGN_DISABLE_TOOLBAR_REORDERING,
                CustomTabIntentDataProvider.AUTO_TRANSLATE_ALLOW_ALL_FIRST_PARTIES,
                CustomTabIntentDataProvider.AUTO_TRANSLATE_PACKAGE_NAME_ALLOWLIST,
                CustomTabIntentDataProvider.THIRD_PARTIES_DEFAULT_POLICY,
                CustomTabIntentDataProvider.DENYLIST_ENTRIES,
                CustomTabIntentDataProvider.ALLOWLIST_ENTRIES,
                WarmupManager.SPARE_TAB_INITIALIZE_RENDERER,
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
                TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION,
                TabPersistentStore.CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY_PARAM,
                TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION,
                TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE,
                TabUiFeatureUtilities.SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST,
                TabUiFeatureUtilities.ZOOMING_MIN_MEMORY,
                TabUiFeatureUtilities.SKIP_SLOW_ZOOMING,
                TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO,
                TabUiFeatureUtilities.TAB_STRIP_REDESIGN_DISABLE_NTB_ANCHOR,
                TabManagementFieldTrial.DELAY_TEMP_STRIP_TIMEOUT_MS,
                TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO,
                TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED,
                VersionNumberGetter.MIN_SDK_VERSION,
                ChromeActivity.CONTENT_VIS_DELAY_MS,
                MinimizeAppAndCloseTabBackPressHandler.SYSTEM_BACK);
        // clang-format on
        tryToCatchMissingParameters(fieldTrialsToCache);
        CachedFeatureFlags.cacheFieldTrialParameters(fieldTrialsToCache);

        CachedFeatureFlags.onEndCheckpoint();
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
        CachedFeatureFlags.cacheMinimalBrowserFlagsTimeFromNativeTime();

        // TODO(crbug.com/995355): Move other related flags from cacheNativeFlags() to here.
        List<CachedFlag> featuresToCache = List.of(ChromeFeatureList.sExperimentsForAgsa);
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);

        CachedFeatureFlags.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }
}
