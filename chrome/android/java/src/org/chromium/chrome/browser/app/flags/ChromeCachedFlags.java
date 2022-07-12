// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.CachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.chime.ChimeFeatures;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuidePushNotificationManager;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsServiceConfig;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.ThemeUtils;
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
                add(ChromeFeatureList.ANONYMOUS_UPDATE_CHECKS);
                add(ChromeFeatureList.APP_MENU_MOBILE_SITE_OPTION);
                add(ChromeFeatureList.BACK_GESTURE_REFACTOR);
                add(ChromeFeatureList.CCT_INCOGNITO);
                add(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY);
                add(ChromeFeatureList.CCT_REMOVE_REMOTE_VIEW_IDS);
                add(ChromeFeatureList.CCT_RESIZABLE_90_MAXIMUM_HEIGHT);
                add(ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE);
                add(ChromeFeatureList.CCT_RESIZABLE_FOR_FIRST_PARTIES);
                add(ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES);
                add(ChromeFeatureList.CCT_TOOLBAR_CUSTOMIZATIONS);
                add(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS);
                add(ChromeFeatureList.CREATE_SAFEBROWSING_ON_STARTUP);
                add(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA);
                add(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
                add(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID);
                add(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE);
                add(ChromeFeatureList.DYNAMIC_COLOR_ANDROID);
                add(ChromeFeatureList.DYNAMIC_COLOR_BUTTONS_ANDROID);
                add(ChromeFeatureList.EARLY_LIBRARY_LOAD);
                add(ChromeFeatureList.ELASTIC_OVERSCROLL);
                add(ChromeFeatureList.ELIDE_PRIORITIZATION_OF_PRE_NATIVE_BOOTSTRAP_TASKS);
                add(ChromeFeatureList.FEED_LOADING_PLACEHOLDER);
                add(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS);
                add(ChromeFeatureList.IMMERSIVE_UI_MODE);
                add(ChromeFeatureList.INSTANT_START);
                add(ChromeFeatureList.INSTANCE_SWITCHER);
                add(ChromeFeatureList.INTEREST_FEED_V2);
                add(ChromeFeatureList.NEW_WINDOW_APP_MENU);
                add(ChromeFeatureList.OMNIBOX_ANDROID_AUXILIARY_SEARCH);
                add(ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS);
                add(ChromeFeatureList.PAINT_PREVIEW_DEMO);
                add(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP);
                add(ChromeFeatureList.READ_LATER);
                add(ChromeFeatureList.START_SURFACE_ANDROID);
                add(ChromeFeatureList.STORE_HOURS);
                add(ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT);
                add(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID);
                add(ChromeFeatureList.TAB_GROUPS_ANDROID);
                add(ChromeFeatureList.TAB_GROUPS_FOR_TABLETS);
                add(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
                add(ChromeFeatureList.TAB_TO_GTS_ANIMATION);
                add(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS);
                add(ChromeFeatureList.TOOLBAR_USE_HARDWARE_BITMAP_DRAW);
                add(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NOTIFICATION_PERMISSION_DELEGATION);
                add(ChromeFeatureList.USE_CHIME_ANDROID_SDK);
                add(ChromeFeatureList.WEB_APK_TRAMPOLINE_ON_INITIAL_INTENT);
            }
        };
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
        CachedFeatureFlags.cacheAdditionalNativeFlags();

        List<CachedFieldTrialParameter> fieldTrialsToCache =
                new ArrayList<CachedFieldTrialParameter>() {
                    {
                        add(ChimeFeatures.ALWAYS_REGISTER);
                        add(StartSurfaceConfiguration.BEHAVIOURAL_TARGETING);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS);
                        add(FeedPlaceholderLayout.ENABLE_INSTANT_START_ANIMATION);
                        add(OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE);
                        add(PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL);
                        add(ReturnToChromeUtil.TAB_SWITCHER_ON_RETURN_MS);
                        add(CustomTabIntentDataProvider.THIRD_PARTIES_DEFAULT_POLICY);
                        add(CustomTabIntentDataProvider.DENYLIST_ENTRIES);
                        add(CustomTabIntentDataProvider.ALLOWLIST_ENTRIES);
                        add(StartSurfaceConfiguration.CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP);
                        add(StartSurfaceConfiguration.FINALE_ANIMATION_ENABLED);
                        add(StartSurfaceConfiguration.HIDE_START_WHEN_LAST_VISITED_TAB_IS_SRP);
                        add(StartSurfaceConfiguration.HOME_BUTTON_ON_GRID_TAB_SWITCHER);
                        add(StartSurfaceConfiguration.IS_DOODLE_SUPPORTED);
                        add(StartSurfaceConfiguration.NEW_SURFACE_FROM_HOME_BUTTON);
                        add(StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP);
                        add(StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD);
                        add(StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB);
                        add(StartSurfaceConfiguration.SHOW_NTP_TILES_ON_OMNIBOX);
                        add(StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER);
                        add(StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT);
                        add(StartSurfaceConfiguration
                                        .SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS);
                        add(StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS);
                        add(StartSurfaceConfiguration.SPARE_RENDERER_DELAY_MS);
                        add(StartSurfaceConfiguration.START_SURFACE_EXCLUDE_MV_TILES);
                        add(StartSurfaceConfiguration.START_SURFACE_EXCLUDE_QUERY_TILES);
                        add(StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB);
                        add(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY);
                        add(StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START);
                        add(StartSurfaceConfiguration.START_SURFACE_VARIATION);
                        add(StartSurfaceConfiguration.SUPPORT_ACCESSIBILITY);
                        add(StartSurfaceConfiguration.TAB_COUNT_BUTTON_ON_START_SURFACE);
                        add(StartSurfaceConfiguration.USER_CLICK_THRESHOLD);
                        add(StartSurfaceConfiguration.WARM_UP_RENDERER);
                        add(StartupPaintPreviewHelper.ACCESSIBILITY_SUPPORT_PARAM);
                        add(PaintPreviewTabService.ALLOW_SRP);
                        add(TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION);
                        add(TabUiFeatureUtilities.ENABLE_LAUNCH_BUG_FIX);
                        add(TabUiFeatureUtilities.ENABLE_LAUNCH_POLISH);
                        add(TabUiFeatureUtilities.DELAY_GTS_CREATION);
                        add(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP);
                        add(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP_ADAPTIVE);
                        add(TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION);
                        add(TabUiFeatureUtilities.SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST);
                        add(TabUiFeatureUtilities.ENABLE_TAB_GROUP_SHARING);
                        add(TabUiFeatureUtilities.ZOOMING_MIN_MEMORY);
                        add(TabUiFeatureUtilities.ZOOMING_MIN_SDK);
                        add(TabUiFeatureUtilities.SKIP_SLOW_ZOOMING);
                        add(TabUiFeatureUtilities.TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE);
                        add(TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO);
                        add(TabUiFeatureUtilities.GRID_TAB_SWITCHER_FOR_TABLETS_POLISH);
                        add(TabUiFeatureUtilities.TAB_STRIP_TAB_WIDTH);
                        add(ThemeUtils.ENABLE_FULL_DYNAMIC_COLORS);
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
            { add(ChromeFeatureList.EXPERIMENTS_FOR_AGSA); }
        };
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);

        CachedFeatureFlags.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }
}
