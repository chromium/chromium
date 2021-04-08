// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * This is a utility class for managing experiments related to returning to Chrome.
 */
public final class ReturnToChromeExperimentsUtil {
    private static final String TAG = "TabSwitcherOnReturn";

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
            if (hasFocus) {
                // It is possible that unfocusing event happens before the Omnibox
                // first gets focused, use this flag to skip the cases.
                mIsOmniboxFocused = true;
                return;
            }

            if (!hasFocus && mIsOmniboxFocused) {
                if (mNewTab.getUrl().isEmpty()) {
                    if (mEmptyTabCloseCallback != null && mNewTab == mActivityTabProvider.get()) {
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
        return handleLoadUrlWithPostDataFromStartSurface(
                params, null, null, incognito, parentTab, false, false, null, null);
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
        handleLoadUrlWithPostDataFromStartSurface(params, null, null, incognito, parentTab,
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
        return handleLoadUrlWithPostDataFromStartSurface(params, postDataType, postData, incognito,
                       parentTab, false, false, null, null)
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
            @Nullable String postDataType, @Nullable byte[] postData, @Nullable Boolean incognito,
            @Nullable Tab parentTab, boolean focusOnOmnibox, boolean skipOverviewCheck,
            @Nullable TabModel currentTabModel, @Nullable Runnable emptyTabCloseCallback) {
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
                             .createNewTab(params, TabLaunchType.FROM_START_SURFACE, parentTab);
        if (focusOnOmnibox && newTab != null) {
            // This observer lives for as long as the user is focused in the Omnibox. It stops
            // observing once the focus is cleared, e.g, Tab navigates or user taps the back button.
            new TabStateObserver(newTab, currentTabModel,
                    chromeActivity.getToolbarManager().getOmniboxStub(), emptyTabCloseCallback,
                    chromeActivity.getActivityTabProvider());
        }

        if (params.getTransitionType() == PageTransition.AUTO_BOOKMARK) {
            if (params.getReferrer() == null) {
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
        if (!StartSurfaceConfiguration.isStartSurfaceEnabled()) return null;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) return null;

        ChromeActivity chromeActivity = (ChromeActivity) activity;

        assert LibraryLoader.getInstance().isInitialized();
        if (!skipOverviewCheck && !chromeActivity.isInOverviewMode()
                && !UrlUtilities.isNTPUrl(url)) {
            return null;
        }

        return chromeActivity;
    }

    public static boolean isCanonicalizedNTPUrl(String url) {
        if (TextUtils.isEmpty(url)) return false;
        // Avoid loading native library due to GURL usage since
        // #shouldShowStartSurfaceAsTheHomePage() is in the critical path in Instant Start.
        return url.equals("chrome://newtab/") || url.equals("chrome-native://newtab/")
                || url.equals("about:newtab");
    }

    /**
     * Check whether we should show Start Surface as the home page. This is used for all cases
     * except initial tab creation, which uses {@link
     * #shouldShowStartSurfaceAsTheHomePageNoTabs()}.
     *
     * @return Whether Start Surface should be shown as the home page.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePage() {
        return shouldShowStartSurfaceAsTheHomePageNoTabs()
                && !StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START.getValue();
    }

    /**
     * @return Whether we should show Start Surface as the home page on phone. Start surface
     *         hasn't been enabled on tablet yet.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePageOnPhone(boolean isTablet) {
        return !isTablet && shouldShowStartSurfaceAsTheHomePage();
    }

    /**
     * @return Whether Start Surface should be shown as NTP.
     */
    public static boolean shouldShowStartSurfaceHomeAsNTP(boolean incognito, boolean isTablet) {
        return !incognito && shouldShowStartSurfaceAsTheHomePageOnPhone(isTablet);
    }

    /**
     * @return Whether hides the home button on an incognito tab.
     */
    public static boolean shouldHideHomeButtonForStartSurface(boolean incognito, boolean isTablet) {
        return incognito && shouldShowStartSurfaceAsTheHomePageOnPhone(isTablet);
    }

    /**
     * Check whether we should show Start Surface as the home page for initial tab creation.
     *
     * @return Whether Start Surface should be shown as the home page.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePageNoTabs() {
        // When creating initial tab, i.e. cold start without restored tabs, we should only show
        // StartSurface as the HomePage if Single Pane is enabled, HomePage is not customized, not
        // on tablet, accessibility is not enabled or the tab group continuation feature is enabled.
        String homePageUrl = HomepageManager.getHomepageUri();
        return StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled()
                && (TextUtils.isEmpty(homePageUrl) || isCanonicalizedNTPUrl(homePageUrl))
                && !StartSurfaceConfiguration.shouldHideStartSurfaceWithAccessibilityOn()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }

    /**
     * @param tabModelSelector The tab model selector.
     * @return the total tab count, and works before native initialization.
     */
    public static int getTotalTabCount(TabModelSelector tabModelSelector) {
        if ((CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)
                    || CachedFeatureFlags.isEnabled(
                            ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP))
                && !tabModelSelector.isTabStateInitialized()) {
            List<PseudoTab> allTabs;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                allTabs = PseudoTab.getAllPseudoTabsFromStateFile();
            }
            return allTabs != null ? allTabs.size() : 0;
        }
        return tabModelSelector.getTotalTabCount();
    }
}
