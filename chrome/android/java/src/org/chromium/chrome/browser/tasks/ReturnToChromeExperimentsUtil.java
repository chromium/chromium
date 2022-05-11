// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is a utility class for managing experiments related to returning to Chrome.
 */
public final class ReturnToChromeExperimentsUtil {
    private static final String TAG = "TabSwitcherOnReturn";

    @VisibleForTesting
    public static final long INVALID_DECISION_TIMESTAMP = -1L;
    public static final long MILLISECONDS_PER_DAY = TimeUtils.SECONDS_PER_DAY * 1000;
    @VisibleForTesting
    public static final String LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA =
            "Startup.Android.LastVisitedTabIsSRPWhenOverviewShownAtLaunch";

    private static final String START_SEGMENTATION_PLATFORM_KEY = "chrome_start_android";

    /** An inner class to monitor the state of a newly create Tab. */
    private static class TabStateObserver implements UrlFocusChangeListener {
        private final Tab mNewTab;
        private final TabModel mCurrentTabModel;
        private final OmniboxStub mOmniboxStub;
        private final @Nullable Runnable mEmptyTabCloseCallback;
        private final ActivityTabProvider mActivityTabProvider;
        private boolean mIsOmniboxFocused;

        TabStateObserver(@NonNull Tab newTab, @NonNull TabModel currentTabModel,
                @NonNull OmniboxStub omniboxStub, @Nullable Runnable emptyTabCloseCallback,
                ActivityTabProvider activityTabProvider) {
            mNewTab = newTab;
            mCurrentTabModel = currentTabModel;
            mEmptyTabCloseCallback = emptyTabCloseCallback;
            mOmniboxStub = omniboxStub;
            mActivityTabProvider = activityTabProvider;
            mIsOmniboxFocused =
                    mOmniboxStub.isUrlBarFocused() && activityTabProvider.get() == newTab;
            mOmniboxStub.addUrlFocusChangeListener(this);
        }

        @Override
        public void onUrlFocusChange(boolean hasFocus) {
            // Filter out focus events that happen when the tab itself in not the current tab.
            if (mActivityTabProvider.get() != mNewTab) return;

            if (hasFocus) {
                // It is possible that unfocusing event happens before the Omnibox
                // first gets focused, use this flag to skip the cases.
                mIsOmniboxFocused = true;
                return;
            }

            if (!hasFocus && mIsOmniboxFocused) {
                if (mNewTab.getUrl().isEmpty()) {
                    if (mEmptyTabCloseCallback != null) {
                        mEmptyTabCloseCallback.run();
                    }
                    // Closes the Tab after any necessary transition is done. This
                    // is safer than closing the Tab first, especially if it is the
                    // only Tab in the TabModel.
                    if (!mNewTab.isClosing()) {
                        mCurrentTabModel.closeTab(mNewTab);
                    }
                } else {
                    // After the tab navigates, we will set the keep tab property,
                    // and the new tab won't be deleted from the TabModel when the
                    // back button is tapped.
                    StartSurfaceUserData.setKeepTab(mNewTab, true);
                }

                // No matter whether the back button is tapped or the Tab navigates,
                // {@link onUrlFocusChanged} with focus == false is always called.
                // Removes the observer here.
                mOmniboxStub.removeUrlFocusChangeListener(this);
            }
        }
    }

    @VisibleForTesting
    public static final String TAB_SWITCHER_ON_RETURN_MS_PARAM = "tab_switcher_on_return_time_ms";
    public static final IntCachedFieldTrialParameter TAB_SWITCHER_ON_RETURN_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_SWITCHER_ON_RETURN, TAB_SWITCHER_ON_RETURN_MS_PARAM, -1);

    @VisibleForTesting
    static final String UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT =
            "Startup.Android.TimeToGTSFirstMeaningfulPaint";

    private static final String UMA_THUMBNAIL_FETCHED_FOR_GTS_FIRST_MEANINGFUL_PAINT =
            "Startup.Android.ThumbnailFetchedForGTSFirstMeaningfulPaint";

    private static boolean sGTSFirstMeaningfulPaintRecorded;

    private ReturnToChromeExperimentsUtil() {}

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
     * Record the elapsed time from activity creation to first meaningful paint of Grid Tab
     * Switcher.
     * @param elapsedMs Elapsed time in ms.
     * @param numOfThumbnails Number of thumbnails fetched for the Grid Tab Switcher.
     */
    public static void recordTimeToGTSFirstMeaningfulPaint(long elapsedMs, int numOfThumbnails) {
        Log.i(TAG,
                UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + coldStartBucketName(!sGTSFirstMeaningfulPaintRecorded)
                        + numThumbnailsBucketName(numOfThumbnails) + ": " + numOfThumbnails
                        + " thumbnails " + elapsedMs + "ms");
        RecordHistogram.recordTimesHistogram(UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + coldStartBucketName(!sGTSFirstMeaningfulPaintRecorded)
                        + numThumbnailsBucketName(numOfThumbnails),
                elapsedMs);
        RecordHistogram.recordTimesHistogram(UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + coldStartBucketName(!sGTSFirstMeaningfulPaintRecorded),
                elapsedMs);
        RecordHistogram.recordTimesHistogram(UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT, elapsedMs);
        RecordHistogram.recordCount100Histogram(
                UMA_THUMBNAIL_FETCHED_FOR_GTS_FIRST_MEANINGFUL_PAINT, numOfThumbnails);
        sGTSFirstMeaningfulPaintRecorded = true;
    }

    @VisibleForTesting
    static String coldStartBucketName(boolean isColdStart) {
        if (isColdStart) return ".Cold";
        return ".Warm";
    }

    @VisibleForTesting
    static String numThumbnailsBucketName(int numOfThumbnails) {
        return "." + numThumbnailsBucket(numOfThumbnails) + "thumbnails";
    }

    /**
     * On Pixel 3 XL, at most 10 cards are fetched. Multi-thumbnail cards can have up to 4
     * thumbnails, so the maximum should be 40.
     */
    private static String numThumbnailsBucket(int numOfThumbnails) {
        if (numOfThumbnails == 0) return "0";
        if (numOfThumbnails <= 2) return "1~2";
        if (numOfThumbnails <= 5) return "3~5";
        if (numOfThumbnails <= 10) return "6~10";
        if (numOfThumbnails <= 20) return "11~20";
        return "20+";
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL.
     *
     * @param params The LoadUrlParams to load.
     * @param incognito Whether to load URL in an incognito Tab.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @return Current tab created if we have handled the navigation, null otherwise.
     */
    public static Tab handleLoadUrlFromStartSurface(
            LoadUrlParams params, @Nullable Boolean incognito, @Nullable Tab parentTab) {
        return handleLoadUrlFromStartSurface(params, false, incognito, parentTab);
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL.
     *
     * @param params The LoadUrlParams to load.
     * @param isBackground Whether to load the URL in a new tab in the background.
     * @param incognito Whether to load URL in an incognito Tab.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @return Current tab created if we have handled the navigation, null otherwise.
     */
    public static Tab handleLoadUrlFromStartSurface(LoadUrlParams params, boolean isBackground,
            @Nullable Boolean incognito, @Nullable Tab parentTab) {
        try (TraceEvent e = TraceEvent.scoped("StartSurface.LoadUrl")) {
            return handleLoadUrlWithPostDataFromStartSurface(params, null, null, isBackground,
                    incognito, parentTab, false, false, null, null);
        }
    }

    /**
     * Check if we should handle the navigation as opening a new Tab. If so, create a new tab and
     * load the URL.
     *
     * @param url The URL to load.
     * @param transition The page transition type.
     * @param incognito Whether to load URL in an incognito Tab.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @param currentTabModel The current TabModel.
     * @param emptyTabCloseCallback The callback to run when the newly created empty Tab will be
     *                              closing.
     */
    public static void handleLoadUrlFromStartSurfaceAsNewTab(String url,
            @PageTransition int transition, @Nullable Boolean incognito, @Nullable Tab parentTab,
            TabModel currentTabModel, @Nullable Runnable emptyTabCloseCallback) {
        LoadUrlParams params = new LoadUrlParams(url, transition);
        handleLoadUrlWithPostDataFromStartSurface(params, null, null, /*isBackground=*/false,
                incognito, parentTab,
                /*focusOnOmnibox*/ true, /*skipOverviewCheck*/ true, currentTabModel,
                emptyTabCloseCallback);
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL with POST
     * data.
     *
     * @param params The LoadUrlParams to load.
     * @param postDataType postData type.
     * @param postData POST data to include in the tab URL's request body, ex. bitmap when image
     *                 search.
     * @param incognito Whether to load URL in an incognito Tab. If null, the current tab model will
     *                  be used.
     * @param parentTab The parent tab used to create a new tab if needed.
     * @return true if we have handled the navigation, false otherwise.
     */
    public static boolean handleLoadUrlWithPostDataFromStartSurface(LoadUrlParams params,
            @Nullable String postDataType, @Nullable byte[] postData, @Nullable Boolean incognito,
            @Nullable Tab parentTab) {
        return handleLoadUrlWithPostDataFromStartSurface(params, postDataType, postData, false,
                       incognito, parentTab, false, false, null, null)
                != null;
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL with POST
     * data.
     *
     * @param params The LoadUrlParams to load.
     * @param postDataType   postData type.
     * @param postData       POST data to include in the tab URL's request body, ex. bitmap when
     *         image search.
     * @param isBackground Whether to load the URL in a new tab in the background.
     * @param incognito Whether to load URL in an incognito Tab. If null, the current tab model will
     *         be used.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @param focusOnOmnibox Whether to focus on the omnibox when a new Tab is created.
     * @param skipOverviewCheck Whether to skip a check of whether it is in the overview mode.
     * @param currentTabModel The current TabModel.
     * @param emptyTabCloseCallback The callback to run when the newly created empty Tab will be
     *                              closing.
     * @return Current tab created if we have handled the navigation, null otherwise.
     */
    private static Tab handleLoadUrlWithPostDataFromStartSurface(LoadUrlParams params,
            @Nullable String postDataType, @Nullable byte[] postData, boolean isBackground,
            @Nullable Boolean incognito, @Nullable Tab parentTab, boolean focusOnOmnibox,
            boolean skipOverviewCheck, @Nullable TabModel currentTabModel,
            @Nullable Runnable emptyTabCloseCallback) {
        String url = params.getUrl();
        ChromeActivity chromeActivity =
                getActivityPresentingOverviewWithOmnibox(url, skipOverviewCheck);
        if (chromeActivity == null) return null;

        // Create a new unparented tab.
        boolean incognitoParam;
        if (incognito == null) {
            incognitoParam = chromeActivity.getCurrentTabModel().isIncognito();
        } else {
            incognitoParam = incognito;
        }

        if (!TextUtils.isEmpty(postDataType) && postData != null && postData.length != 0) {
            params.setVerbatimHeaders("Content-Type: " + postDataType);
            params.setPostData(ResourceRequestBody.createFromBytes(postData));
        }

        Tab newTab = chromeActivity.getTabCreator(incognitoParam)
                             .createNewTab(params,
                                     isBackground ? TabLaunchType.FROM_LONGPRESS_BACKGROUND
                                                  : TabLaunchType.FROM_START_SURFACE,
                                     parentTab);
        if (isBackground) {
            StartSurfaceUserData.setOpenedFromStart(newTab);
        }

        if (focusOnOmnibox && newTab != null) {
            // This observer lives for as long as the user is focused in the Omnibox. It stops
            // observing once the focus is cleared, e.g, Tab navigates or user taps the back
            // button.
            new TabStateObserver(newTab, currentTabModel,
                    chromeActivity.getToolbarManager().getOmniboxStub(), emptyTabCloseCallback,
                    chromeActivity.getActivityTabProvider());
        }

        if (params.getTransitionType() == PageTransition.AUTO_BOOKMARK) {
            if (!TextUtils.equals(UrlConstants.RECENT_TABS_URL, params.getUrl())
                    && params.getReferrer() == null) {
                RecordUserAction.record("Suggestions.Tile.Tapped.StartSurface");
            }
        } else if (url == null) {
            RecordUserAction.record("MobileMenuNewTab.StartSurfaceFinale");
        } else {
            RecordUserAction.record("MobileOmniboxUse.StartSurface");

            // These are duplicated here but would have been recorded by LocationBarLayout#loadUrl.
            RecordUserAction.record("MobileOmniboxUse");
            LocaleManager.getInstance().recordLocaleBasedSearchMetrics(
                    false, url, params.getTransitionType());
        }

        return newTab;
    }

    /**
     * @param url The URL to load.
     * @param skipOverviewCheck Whether to skip a check of whether it is in the overview mode.
     * @return The ChromeActivity if it is presenting the omnibox on the tab switcher, else null.
     */
    private static ChromeActivity getActivityPresentingOverviewWithOmnibox(
            String url, boolean skipOverviewCheck) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null || !isStartSurfaceEnabled(activity)
                || !(activity instanceof ChromeActivity)) {
            return null;
        }

        ChromeActivity chromeActivity = (ChromeActivity) activity;

        assert LibraryLoader.getInstance().isInitialized();
        if (!skipOverviewCheck && !chromeActivity.isInOverviewMode()
                && !UrlUtilities.isNTPUrl(url)) {
            return null;
        }

        return chromeActivity;
    }

    /**
     * Check whether we should show Start Surface as the home page. This is used for all cases
     * except initial tab creation, which uses {@link
     * ReturnToChromeExperimentsUtil#isStartSurfaceEnabled(Context)}.
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
     * @return Whether Start Surface should be shown as NTP.
     */
    public static boolean shouldShowStartSurfaceHomeAsNTP(
            Context context, boolean incognito, boolean isTablet) {
        return !incognito && shouldShowStartSurfaceAsTheHomePageOnPhone(context, isTablet);
    }

    /**
     * @return Whether opening a NTP instead of Start surface for new Tab is enabled.
     */
    public static boolean shouldOpenNTPInsteadOfStart() {
        return StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START.getValue();
    }

    /**
     * Check whether Start Surface is enabled. It includes checks of:
     * 1) whether home page is enabled and whether it is Chrome' home page url;
     * 2) whether Start surface is enabled with current accessibility settings;
     * 3) whether it is on phone.
     * @param context The activity context.
     */
    public static boolean isStartSurfaceEnabled(Context context) {
        // When creating initial tab, i.e. cold start without restored tabs, we should only show
        // StartSurface as the HomePage if Single Pane is enabled, HomePage is not customized, not
        // on tablet, accessibility is not enabled or the tab group continuation feature is enabled.
        String homePageUrl = HomepageManager.getHomepageUri();
        return StartSurfaceConfiguration.isStartSurfaceFlagEnabled()
                && HomepageManager.isHomepageEnabled()
                && (TextUtils.isEmpty(homePageUrl)
                        || UrlUtilities.isCanonicalizedNTPUrl(homePageUrl))
                && !shouldHideStartSurfaceWithAccessibilityOn(context)
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * @return Whether start surface should be hidden when accessibility is enabled. If it's true,
     *         NTP is shown as homepage. Also, when time threshold is reached, grid tab switcher or
     *         overview list layout is shown instead of start surface.
     */
    public static boolean shouldHideStartSurfaceWithAccessibilityOn(Context context) {
        // TODO(crbug.com/1127732): Move this method back to StartSurfaceConfiguration.
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && !(StartSurfaceConfiguration.SUPPORT_ACCESSIBILITY.getValue()
                        && TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(context));
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
                && ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage(context)
                && !intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false)) {
            return true;
        }

        boolean isStartSurfaceEnabled =
                ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(context);

        // If Start surface is enabled and there's no tab existing, handle the initial tab creation.
        if (isStartSurfaceEnabled && IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeExperimentsUtil.getTotalTabCount(tabModelSelector) <= 0) {
            return true;
        }

        // Checks whether to hide Start surface when last visited tab is a search result page.
        if (isStartSurfaceEnabled && shouldHideStartSurfaceWhenLastVisitedTabIsSRP()) return false;

        // Checks whether to show the Start surface / grid Tab switcher due to feature flag
        // TAB_SWITCHER_ON_RETURN_MS.
        long lastBackgroundedTimeMillis = inactivityTracker.getLastBackgroundedTimeMs();
        boolean tabSwitcherOnReturn = IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(lastBackgroundedTimeMillis);

        // If the overview page won't be shown on startup, stops here.
        if (!tabSwitcherOnReturn) return false;

        if (isStartSurfaceEnabled) {
            if (StartSurfaceConfiguration.CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP.getValue()) {
                // We only check the sync status when flag CHECK_SYNC_BEFORE_SHOW_START_AT_STARTUP
                // and the Start surface are both enabled.
                return ReturnToChromeExperimentsUtil.isPrimaryAccountSync();
            } else if (!TextUtils.isEmpty(
                               StartSurfaceConfiguration.BEHAVIOURAL_TARGETING.getValue())) {
                return ReturnToChromeExperimentsUtil.userBehaviourSupported();
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
     * Caches the status of whether the primary account is synced.
     */
    public static void cachePrimaryAccountSyncStatus() {
        boolean isPrimaryAccountSync =
                IdentityServicesProvider.get()
                        .getSigninManager(Profile.getLastUsedRegularProfile())
                        .getIdentityManager()
                        .hasPrimaryAccount(ConsentLevel.SYNC);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.PRIMARY_ACCOUNT_SYNC, isPrimaryAccountSync);
    }

    /**
     * @param currentTab  The current {@link Tab}.
     * @return Whether the Tab is launched with launchType TabLaunchType.FROM_START_SURFACE or it
     *         has "OpenedFromStart" property.
     */
    public static boolean isTabFromStartSurface(Tab currentTab) {
        final @TabLaunchType int type = currentTab.getLaunchType();
        return type == TabLaunchType.FROM_START_SURFACE
                || StartSurfaceUserData.isOpenedFromStart(currentTab);
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

    /*
     * Computes result of the segmentation platform and store to prefs.
     */
    public static void cacheSegmentationResult() {
        SegmentationPlatformService segmentationPlatformService =
                SegmentationPlatformServiceFactory.getForProfile(
                        Profile.getLastUsedRegularProfile());
        segmentationPlatformService.getSelectedSegment(START_SEGMENTATION_PLATFORM_KEY, result -> {
            @ShowChromeStartSegmentationResult
            int resultEnum;
            if (!result.isReady) {
                resultEnum = ShowChromeStartSegmentationResult.UNINITIALIZED;
            } else if (result.selectedSegment
                    == OptimizationTarget.OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID) {
                resultEnum = ShowChromeStartSegmentationResult.SHOW;
            } else {
                resultEnum = ShowChromeStartSegmentationResult.DONT_SHOW;
            }
            SharedPreferencesManager.getInstance().writeInt(
                    ChromePreferenceKeys.SHOW_START_SEGMENTATION_RESULT, resultEnum);
        });
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
     * Called when the "History" menu is clicked. The count is only recorded when the behavioural
     * targeting is enabled on the Start surface.
     */
    public static void onHistoryOpened() {
        onUIClicked(ChromePreferenceKeys.OPEN_HISTORY_COUNT);
    }

    /**
     * Called when the "Recent tabs" menu is clicked. The count is only recorded when the
     * behavioural targeting is enabled on the Start surface.
     */
    public static void onRecentTabsOpened() {
        onUIClicked(ChromePreferenceKeys.OPEN_RECENT_TABS_COUNT);
    }

    /**
     * Called when a Feed card is opened in 1) a foreground tab; 2) a background tab and 3) an
     * incognito tab. The count is only recorded when the behavioural targeting is enabledf on the
     * Start surface.
     */
    public static void onFeedCardOpened() {
        onUIClicked(ChromePreferenceKeys.TAP_FEED_CARDS_COUNT);
    }

    /**
     * Called when a MV tile is opened. The count is only recorded when the behavioural targeting is
     * enabled on the Start surface.
     */
    public static void onMVTileOpened() {
        onUIClicked(ChromePreferenceKeys.TAP_MV_TILES_COUNT);
    }

    /**
     * Record whether the last visited tab shown in the single tab switcher or carousel tab switcher
     * is a search result page or not. This should be called when Start surface is shown at startup.
     */
    public static void recordLastVisitedTabIsSRPWhenOverviewIsShownAtLaunch() {
        RecordHistogram.recordBooleanHistogram(
                LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA,
                SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false));
    }

    @VisibleForTesting
    public static String getBehaviourTypeKeyForTesting(String key) {
        return getBehaviourType(key);
    }

    @VisibleForTesting
    public static void setSyncForTesting(boolean isSyncing) {
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        manager.writeBoolean(ChromePreferenceKeys.PRIMARY_ACCOUNT_SYNC, isSyncing);
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
