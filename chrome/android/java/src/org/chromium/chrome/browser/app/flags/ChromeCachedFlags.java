// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.CachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensFeature;
import org.chromium.chrome.browser.merchant_viewer.MerchantViewerConfig;
import org.chromium.chrome.browser.notifications.chime.ChimeFeatures;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsServiceConfig;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceConfig;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
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
    private static final List<CachedFieldTrialParameter> MINIMAL_BROWSER_FIELD_TRIALS =
            new ArrayList<CachedFieldTrialParameter>() {
                {
                    // This is used by CustomTabsConnection implementation, which does not
                    // necessarily start chrome.
                    add(CustomTabActivity.EXPERIMENTS_FOR_AGSA_PARAMS);
                }
            };

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

        // Workaround for crbug.com/1223545: Do not use Arrays.asList().
        List<String> featuresToCache = new ArrayList<String>() {
            {
                add(ChromeFeatureList.ANDROID_PARTNER_CUSTOMIZATION_PHENOTYPE);
                add(ChromeFeatureList.APP_MENU_MOBILE_SITE_OPTION);
                add(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION);
                add(ChromeFeatureList.BOOKMARK_BOTTOM_SHEET);
                add(ChromeFeatureList.CCT_INCOGNITO);
                add(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY);
                add(ChromeFeatureList.CCT_REMOVE_REMOTE_VIEW_IDS);
                add(ChromeFeatureList.CLIPBOARD_SUGGESTION_CONTENT_HIDDEN);
                add(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS);
                add(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA);
                add(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
                add(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID);
                add(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE);
                add(ChromeFeatureList.DYNAMIC_COLOR_ANDROID);
                add(ChromeFeatureList.EARLY_LIBRARY_LOAD);
                add(ChromeFeatureList.ELASTIC_OVERSCROLL);
                add(ChromeFeatureList.IMMERSIVE_UI_MODE);
                add(ChromeFeatureList.INSTANT_START);
                add(ChromeFeatureList.INTEREST_FEED_V2);
                add(ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH);
                add(ChromeFeatureList.NEW_WINDOW_APP_MENU);
                add(ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK);
                add(ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS);
                add(ChromeFeatureList.PAINT_PREVIEW_DEMO);
                add(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP);
                add(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS);
                add(ChromeFeatureList.READ_LATER);
                add(ChromeFeatureList.START_SURFACE_ANDROID);
                add(ChromeFeatureList.STORE_HOURS);
                add(ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT);
                add(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID);
                add(ChromeFeatureList.TAB_GROUPS_ANDROID);
                add(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
                add(ChromeFeatureList.TAB_TO_GTS_ANIMATION);
                add(ChromeFeatureList.THEME_REFACTOR_ANDROID);
                add(ChromeFeatureList.TOOLBAR_USE_HARDWARE_BITMAP_DRAW);
                add(ChromeFeatureList.USE_CHIME_ANDROID_SDK);
            }
        };
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
        CachedFeatureFlags.cacheAdditionalNativeFlags();

        List<CachedFieldTrialParameter> fieldTrialsToCache =
                new ArrayList<CachedFieldTrialParameter>() {
                    {
                        add(ChimeFeatures.ALWAYS_REGISTER);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS);
                        add(LensFeature.DISABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_INCOGNITO);
                        add(LensFeature.ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_LOW_END_DEVICE);
                        add(LensFeature.ENABLE_LENS_CAMERA_ASSISTED_SEARCH_ON_TABLET);
                        add(LensFeature.MIN_AGSA_VERSION_LENS_CAMERA_ASSISTED_SEARCH);
                        add(LensFeature.SEARCH_BOX_START_VARIANT_LENS_CAMERA_ASSISTED_SEARCH);
                        add(LensFeature.SKIP_AGSA_VERSION_CHECK);
                        add(LensFeature.SKIP_LENS_ELIGIBILITY_CHECKS);
                        add(MerchantViewerConfig.DEFAULT_TRUST_SIGNALS_MESSAGE_DELAY);
                        add(MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR);
                        add(MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_SECONDS);
                        add(MerchantViewerConfig.TRUST_SIGNALS_SHEET_USE_PAGE_TITLE);
                        add(PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL);
                        add(ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS);
                        add(StartSurfaceConfiguration.HOME_BUTTON_ON_GRID_TAB_SWITCHER);
                        add(StartSurfaceConfiguration.NEW_SURFACE_FROM_HOME_BUTTON);
                        add(StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB);
                        add(StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER);
                        add(StartSurfaceConfiguration.START_SURFACE_EXCLUDE_MV_TILES);
                        add(StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB);
                        add(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY);
                        add(StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START);
                        add(StartSurfaceConfiguration.SHOW_NTP_TILES_ON_OMNIBOX);
                        add(StartSurfaceConfiguration.START_SURFACE_VARIATION);
                        add(StartSurfaceConfiguration.SUPPORT_ACCESSIBILITY);
                        add(StartSurfaceConfiguration.TAB_COUNT_BUTTON_ON_START_SURFACE);
                        add(StartupPaintPreviewHelper.ACCESSIBILITY_SUPPORT_PARAM);
                        add(CommerceSubscriptionsServiceConfig.STALE_TAB_LOWER_BOUND_SECONDS);
                        add(CommerceSubscriptionsServiceConfig.SUBSCRIPTIONS_SERVICE_BASE_URL);
                        add(PriceTrackingUtilities.ENABLE_PRICE_NOTIFICATION);
                        add(PriceTrackingUtilities.ENABLE_PRICE_TRACKING);
                        add(TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION);
                        add(TabUiFeatureUtilities.ENABLE_LAUNCH_BUG_FIX);
                        add(TabUiFeatureUtilities.ENABLE_LAUNCH_POLISH);
                        add(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP);
                        add(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP_ADAPTIVE);
                        add(TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION);
                        add(TabUiFeatureUtilities.ZOOMING_MIN_MEMORY);
                        add(TabUiFeatureUtilities.ZOOMING_MIN_SDK);
                        add(TabUiFeatureUtilities.SKIP_SLOW_ZOOMING);
                        add(TabUiFeatureUtilities.TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE);
                        add(TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO);
                    }
                };
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
        List<String> featuresToCache = new ArrayList<String>() {
            {
                add(ChromeFeatureList.EXPERIMENTS_FOR_AGSA);
                add(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD);
                add(ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH);
            }
        };
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);

        CachedFeatureFlags.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }
}
