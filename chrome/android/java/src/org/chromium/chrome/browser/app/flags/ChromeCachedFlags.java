// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.PartialCustomTabHeightStrategy;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingController;
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
import org.chromium.chrome.browser.page_annotations.PageAnnotationsServiceConfig;
import org.chromium.chrome.browser.tab.state.FilePersistedTabDataStorage;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
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
        List<CachedFlag> featuresToCache = new ArrayList<CachedFlag>() {
            {
                add(ChromeFeatureList.sAndroidAuxiliarySearch);
                add(ChromeFeatureList.sAnonymousUpdateChecks);
                add(ChromeFeatureList.sAppMenuMobileSiteOption);
                add(ChromeFeatureList.sBackGestureRefactorAndroid);
                add(ChromeFeatureList.sCctAutoTranslate);
                add(ChromeFeatureList.sCctBrandTransparency);
                add(ChromeFeatureList.sCctFeatureUsage);
                add(ChromeFeatureList.sCctIncognito);
                add(ChromeFeatureList.sCctIncognitoAvailableToThirdParty);
                add(ChromeFeatureList.sCctIntentFeatureOverrides);
                add(ChromeFeatureList.sCctRemoveRemoteViewIds);
                add(ChromeFeatureList.sCctResizable90MaximumHeight);
                add(ChromeFeatureList.sCctResizableAllowResizeByUserGesture);
                add(ChromeFeatureList.sCctResizableAlwaysShowNavBarButtons);
                add(ChromeFeatureList.sCctResizableForFirstParties);
                add(ChromeFeatureList.sCctResizableForThirdParties);
                add(ChromeFeatureList.sCctResizableSideSheet);
                add(ChromeFeatureList.sCctRetainableStateInMemory);
                add(ChromeFeatureList.sCctToolbarCustomizations);
                add(ChromeFeatureList.sCloseTabSuggestions);
                add(ChromeFeatureList.sCommandLineOnNonRooted);
                add(ChromeFeatureList.sConditionalTabStripAndroid);
                add(ChromeFeatureList.sCommerceCoupons);
                add(ChromeFeatureList.sCriticalPersistedTabData);
                add(ChromeFeatureList.sDiscoverMultiColumn);
                add(ChromeFeatureList.sTabStripRedesign);
                add(ChromeFeatureList.sDiscardOccludedBitmaps);
                add(ChromeFeatureList.sDownloadsAutoResumptionNative);
                add(ChromeFeatureList.sEarlyLibraryLoad);
                add(ChromeFeatureList.sFeedLoadingPlaceholder);
                add(ChromeFeatureList.sFoldableJankFix);
                add(ChromeFeatureList.sGridTabSwitcherForTablets);
                add(ChromeFeatureList.sImmersiveUiMode);
                add(ChromeFeatureList.sIncognitoReauthenticationForAndroid);
                add(ChromeFeatureList.sInstanceSwitcher);
                add(ChromeFeatureList.sInstantStart);
                add(ChromeFeatureList.sInterestFeedV2);
                add(ChromeFeatureList.sNewWindowAppMenu);
                add(ChromeFeatureList.sOmniboxMatchToolbarAndStatusBarColor);
                add(ChromeFeatureList.sOmniboxModernizeVisualUpdate);
                add(ChromeFeatureList.sOmniboxMostVisitedTilesAddRecycledViewPool);
                add(ChromeFeatureList.sOmniboxRemoveExcessiveRecycledViewClearCalls);
                add(ChromeFeatureList.sOptimizationGuidePushNotifications);
                add(ChromeFeatureList.sOSKResizesVisualViewportByDefault);
                add(ChromeFeatureList.sPaintPreviewDemo);
                add(ChromeFeatureList.sQueryTiles);
                add(ChromeFeatureList.sQueryTilesOnStart);
                add(ChromeFeatureList.sReadLater);
                add(ChromeFeatureList.sStartSurfaceAndroid);
                add(ChromeFeatureList.sStartSurfaceDisabledFeedImprovement);
                add(ChromeFeatureList.sStartSurfaceReturnTime);
                add(ChromeFeatureList.sStartSurfaceRefactor);
                add(ChromeFeatureList.sStoreHoursAndroid);
                add(ChromeFeatureList.sSwapPixelFormatToFixConvertFromTranslucent);
                add(ChromeFeatureList.sTabGridLayoutAndroid);
                add(ChromeFeatureList.sTabGroupsAndroid);
                add(ChromeFeatureList.sTabGroupsContinuationAndroid);
                add(ChromeFeatureList.sTabGroupsForTablets);
                add(ChromeFeatureList.sTabSelectionEditorV2);
                add(ChromeFeatureList.sTabStripImprovements);
                add(ChromeFeatureList.sTabToGTSAnimation);
                add(ChromeFeatureList.sToolbarUseHardwareBitmapDraw);
                add(ChromeFeatureList.sUseChimeAndroidSdk);
                add(ChromeFeatureList.sUseLibunwindstackNativeUnwinderAndroid);
                add(ChromeFeatureList.sWebApkTrampolineOnInitialIntent);
            }
        };
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
        CachedFeatureFlags.cacheAdditionalNativeFlags();

        List<CachedFieldTrialParameter> fieldTrialsToCache =
                new ArrayList<CachedFieldTrialParameter>() {
                    {
                        add(BrandingController.BRANDING_CADENCE_MS);
                        add(BrandingController.MAX_BLANK_TOOLBAR_TIMEOUT_MS);
                        add(BrandingController.USE_TEMPORARY_STORAGE);
                        add(BrandingController.ANIMATE_TOOLBAR_ICON_TRANSITION);
                        add(ChimeFeatures.ALWAYS_REGISTER);
                        add(StartSurfaceConfiguration.BEHAVIOURAL_TARGETING);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD);
                        add(ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS);
                        add(FeedPlaceholderLayout.ENABLE_INSTANT_START_ANIMATION);
                        add(FilePersistedTabDataStorage.DELAY_SAVES_UNTIL_DEFERRED_STARTUP_PARAM);
                        add(OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE);
                        add(OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET);
                        add(OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX);
                        add(OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN);
                        add(PageAnnotationsServiceConfig.PAGE_ANNOTATIONS_BASE_URL);
                        add(PartialCustomTabHeightStrategy.LOG_IMMERSIVE_MODE_CONFIRMATIONS);
                        add(ReturnToChromeUtil.TAB_SWITCHER_ON_RETURN_MS);
                        add(CustomTabIntentDataProvider.AUTO_TRANSLATE_ALLOW_ALL_FIRST_PARTIES);
                        add(CustomTabIntentDataProvider.AUTO_TRANSLATE_PACKAGE_NAME_ALLOWLIST);
                        add(CustomTabIntentDataProvider.THIRD_PARTIES_DEFAULT_POLICY);
                        add(CustomTabIntentDataProvider.DENYLIST_ENTRIES);
                        add(CustomTabIntentDataProvider.ALLOWLIST_ENTRIES);
                        add(StartSurfaceConfiguration.IS_DOODLE_SUPPORTED);
                        add(StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS);
                        add(StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL);
                        add(StartSurfaceConfiguration.NUM_DAYS_KEEP_SHOW_START_AT_STARTUP);
                        add(StartSurfaceConfiguration.NUM_DAYS_USER_CLICK_BELOW_THRESHOLD);
                        add(StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER);
                        add(StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT);
                        add(StartSurfaceConfiguration
                                        .SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS);
                        add(StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS);
                        add(StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB);
                        add(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY);
                        add(StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START);
                        add(StartSurfaceConfiguration.START_SURFACE_OPEN_START_AS_HOMEPAGE);
                        add(StartSurfaceConfiguration.START_SURFACE_VARIATION);
                        add(StartSurfaceConfiguration.SUPPORT_ACCESSIBILITY);
                        add(StartSurfaceConfiguration.TAB_COUNT_BUTTON_ON_START_SURFACE);
                        add(StartSurfaceConfiguration.USER_CLICK_THRESHOLD);
                        add(TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION);
                        add(TabPersistentStore.CRITICAL_PERSISTED_TAB_DATA_SAVE_ONLY_PARAM);
                        add(TabUiFeatureUtilities.ENABLE_LAUNCH_BUG_FIX);
                        add(TabUiFeatureUtilities.ENABLE_LAUNCH_POLISH);
                        add(TabUiFeatureUtilities.DELAY_GTS_CREATION);
                        add(TabUiFeatureUtilities.ENABLE_TAB_GROUP_AUTO_CREATION);
                        add(TabUiFeatureUtilities.SHOW_OPEN_IN_TAB_GROUP_MENU_ITEM_FIRST);
                        add(TabUiFeatureUtilities.ENABLE_TAB_GROUP_SHARING);
                        add(TabUiFeatureUtilities.ZOOMING_MIN_MEMORY);
                        add(TabUiFeatureUtilities.ZOOMING_MIN_SDK);
                        add(TabUiFeatureUtilities.SKIP_SLOW_ZOOMING);
                        add(TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO);
                        add(TabUiFeatureUtilities.GRID_TAB_SWITCHER_FOR_TABLETS_POLISH);
                        add(TabUiFeatureUtilities.TAB_STRIP_TAB_WIDTH);
                        add(TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_SHARE);
                        add(TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_BOOKMARKS);
                        add(TabUiFeatureUtilities.TAB_STRIP_REDESIGN_ENABLE_FOLIO);
                        add(TabUiFeatureUtilities.TAB_STRIP_REDESIGN_ENABLE_DETACHED);
                        add(VersionNumberGetter.MIN_SDK_VERSION);
                        add(ChromeActivity.CONTENT_VIS_DELAY_MS);
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
        List<CachedFlag> featuresToCache = new ArrayList<CachedFlag>() {
            { add(ChromeFeatureList.sExperimentsForAgsa); }
        };
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);

        CachedFeatureFlags.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }
}
