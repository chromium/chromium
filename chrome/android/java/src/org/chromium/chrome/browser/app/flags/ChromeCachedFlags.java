// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.CachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensFeature;
import org.chromium.chrome.browser.merchant_viewer.MerchantViewerConfig;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsServiceConfig;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceConfig;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

import java.util.ArrayList;
import java.util.Arrays;
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
    private static final List<CachedFieldTrialParameter> MINIMAL_BROWSER_FIELD_TRIALS =
            Arrays.asList(
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
        List<String> featuresToCache = Arrays.asList(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
                ChromeFeatureList.ANDROID_MANAGED_BY_MENU_ITEM,
                ChromeFeatureList.ANDROID_PARTNER_CUSTOMIZATION_PHENOTYPE,
                ChromeFeatureList.APP_MENU_MOBILE_SITE_OPTION,
                ChromeFeatureList.BOOKMARK_BOTTOM_SHEET,
                ChromeFeatureList.CCT_INCOGNITO,
                ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY,
                ChromeFeatureList.CCT_REMOVE_REMOTE_VIEW_IDS,
                ChromeFeatureList.CHROME_STARTUP_DELEGATE,
                ChromeFeatureList.CLIPBOARD_SUGGESTION_CONTENT_HIDDEN,
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA,
                ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED,
                ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID,
                ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE,
                ChromeFeatureList.EARLY_LIBRARY_LOAD,
                ChromeFeatureList.IMMERSIVE_UI_MODE,
                ChromeFeatureList.INSTANT_START,
                ChromeFeatureList.INTEREST_FEED_V2,
                ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH,
                ChromeFeatureList.PAINT_PREVIEW_DEMO,
                ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP,
                ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS,
                ChromeFeatureList.READ_LATER,
                ChromeFeatureList.START_SURFACE_ANDROID,
                ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                ChromeFeatureList.TAB_GROUPS_ANDROID,
                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                ChromeFeatureList.TAB_TO_GTS_ANIMATION,
                ChromeFeatureList.THEME_REFACTOR_ANDROID,
                ChromeFeatureList.TOOLBAR_USE_HARDWARE_BITMAP_DRAW,
                ChromeFeatureList.USE_CHIME_ANDROID_SDK,
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK);
        // clang-format on
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
        CachedFeatureFlags.cacheAdditionalNativeFlags();

        // clang-format off
        List<CachedFieldTrialParameter> fieldTrialsToCache = Arrays.asList(
                AdaptiveToolbarFeatures.MODE_PARAM,
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT,
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD,
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS,
                LensFeature.DISABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_INCOGNITO,
                LensFeature.ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_LOW_END_DEVICE,
                LensFeature.ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_TABLET,
                LensFeature.SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH,
                LensFeature.MIN_AGSA_VERSION_LENS_CAMERA_ASSISTED_SEARCH,
                MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_SECONDS,
                MerchantViewerConfig.TRUST_SIGNALS_SHEET_USE_PAGE_TITLE,
                PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL,
                ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS,
                ShoppingPersistedTabData.TIME_TO_LIVE_MS,
                ShoppingPersistedTabData.DISPLAY_TIME_MS,
                ShoppingPersistedTabData.STALE_TAB_THRESHOLD_SECONDS,
                ShoppingPersistedTabData.PRICE_TRACKING_WITH_OPTIMIZATION_GUIDE,
                StartSurfaceConfiguration.HOME_BUTTON_ON_GRID_TAB_SWITCHER,
                StartSurfaceConfiguration.NEW_SURFACE_FROM_HOME_BUTTON,
                StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB,
                StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER,
                StartSurfaceConfiguration.START_SURFACE_EXCLUDE_MV_TILES,
                StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB,
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY,
                StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START,
                StartSurfaceConfiguration.START_SURFACE_VARIATION,
                StartSurfaceConfiguration.START_SURFACE_OMNIBOX_SCROLL_MODE,
                StartSurfaceConfiguration.SUPPORT_ACCESSIBILITY,
                StartSurfaceConfiguration.TRENDY_ENABLED,
                StartSurfaceConfiguration.TRENDY_ENDPOINT,
                StartSurfaceConfiguration.TRENDY_FAILURE_MIN_PERIOD_MS,
                StartSurfaceConfiguration.TRENDY_SUCCESS_MIN_PERIOD_MS,
                StartupPaintPreviewHelper.ACCESSIBILITY_SUPPORT_PARAM,
                CommerceSubscriptionsServiceConfig.STALE_TAB_LOWER_BOUND_SECONDS,
                CommerceSubscriptionsServiceConfig.SUBSCRIPTIONS_SERVICE_BASE_URL,
                TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION,
                TabUiFeatureUtilities.ENABLE_LAUNCH_BUG_FIX,
                TabUiFeatureUtilities.ENABLE_LAUNCH_POLISH,
                TabUiFeatureUtilities.ENABLE_SEARCH_CHIP,
                TabUiFeatureUtilities.ENABLE_PRICE_NOTIFICATION,
                TabUiFeatureUtilities.ENABLE_PRICE_TRACKING,
                TabUiFeatureUtilities.ENABLE_SEARCH_CHIP_ADAPTIVE,
                TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION,
                TabUiFeatureUtilities.ZOOMING_MIN_MEMORY,
                TabUiFeatureUtilities.ZOOMING_MIN_SDK,
                TabUiFeatureUtilities.SKIP_SLOW_ZOOMING,
                TabUiFeatureUtilities.TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE,
                TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO);
        // clang-format on
        tryToCatchMissingParameters(fieldTrialsToCache);
        CachedFeatureFlags.cacheFieldTrialParameters(fieldTrialsToCache);

        mIsFinishedCachingNativeFlags = true;
    }

    @RemovableInRelease
    private void tryToCatchMissingParameters(List<CachedFieldTrialParameter> listed) {
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
        CachedFeatureFlags.cacheNativeFlags(Arrays.asList(ChromeFeatureList.EXPERIMENTS_FOR_AGSA,
                ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD,
                ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH));

        CachedFeatureFlags.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }
}
