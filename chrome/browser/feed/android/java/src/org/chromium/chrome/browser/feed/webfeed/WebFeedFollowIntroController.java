// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.os.Handler;
import android.util.Base64;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Controls when and how the Web Feed follow intro is shown.
 *
 * <pre>
 * Main requirements for the presentation of the intro (all must be true):
 *  1. The URL is recommended.
 *  2. This site was visited enough times in day-boolean visits and in total visits.
 *  3. Enough time has passed since the last intro was presented and since the last intro for this
 *     site was presented.
 *  4. Feature tracker allows the presentation of the intro, including a weekly presentation limit
 *     check.
 *
 * If the intro debug mode pref is enabled then only 1. is checked for.
 *
 * Note: The feature engagement tracker check happens only later, and it includes checking for a
 * weekly limit.
 * </pre>
 */
public class WebFeedFollowIntroController {
    private static final String TAG = "WFFollowIntroCtrl";

    // In-page time delay to show the intro.
    private static final int DEFAULT_WAIT_TIME_MILLIS = 3 * 1000;
    // Visit history requirements.
    private static final int DEFAULT_DAILY_VISIT_MIN = 3;
    private static final int DEFAULT_NUM_VISIT_MIN = 3;
    // Time between appearances.
    private static final int DEFAULT_APPEARANCE_THRESHOLD_MINUTES = 15;
    // Time between appearances for the same WebFeedId.
    private static final long WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS = TimeUnit.DAYS.toMillis(1);
    // Maximum number of times a WebFeedId is promoted.
    private static final long WEB_FEED_ID_MAX_APPEARANCES = 3;

    /** Clock to use so we can mock time in tests. */
    public interface Clock {
        long currentTimeMillis();
    }

    private Clock mClock = System::currentTimeMillis;

    private final Activity mActivity;
    private final Profile mProfile;
    private final CurrentTabObserver mCurrentTabObserver;
    private final EmptyTabObserver mTabObserver;
    private final PrefService mPrefService;
    private final SharedPreferencesManager mSharedPreferencesManager =
            ChromeSharedPreferences.getInstance();
    private final Tracker mFeatureEngagementTracker;
    private final WebFeedSnackbarController mWebFeedSnackbarController;
    private final WebFeedFollowIntroView mWebFeedFollowIntroView;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final WebFeedRecommendationFollowAcceleratorController
            mRecommendationFollowAcceleratorController;
    private final RecommendationInfoFetcher mRecommendationFetcher;

    private final long mAppearanceThresholdMillis;

    private boolean mIntroShownForTesting;

    private static class RecommendedWebFeedInfo {
        public byte[] webFeedId;
        public GURL url;
        public String title;
    }

    /**
     * Constructs an instance of {@link WebFeedFollowIntroController}.
     *
     * @param activity The current {@link Activity}.
     * @param profile The {@link Profile} associated with the web feed.
     * @param appMenuHandler The {@link AppMenuHandler} to highlight the Web Feed menu item.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     * @param feedLauncher The {@link FeedLauncher} to launch the feed.
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager The {@link SnackbarManager} to show snackbars.
     */
    public WebFeedFollowIntroController(
            Activity activity,
            Profile profile,
            AppMenuHandler appMenuHandler,
            ObservableSupplier<Tab> tabSupplier,
            View menuButtonAnchorView,
            FeedLauncher feedLauncher,
            ModalDialogManager dialogManager,
            SnackbarManager snackbarManager) {
        mPrefService = UserPrefs.get(profile);
        mRecommendationFetcher = new RecommendationInfoFetcher(mPrefService);

        mRecommendationFollowAcceleratorController =
                new WebFeedRecommendationFollowAcceleratorController(
                        activity,
                        appMenuHandler,
                        tabSupplier,
                        menuButtonAnchorView,
                        feedLauncher,
                        dialogManager,
                        snackbarManager);

        mActivity = activity;
        mProfile = profile;
        mTabSupplier = tabSupplier;
        mFeatureEngagementTracker = TrackerFactory.getTrackerForProfile(profile);
        mWebFeedSnackbarController =
                new WebFeedSnackbarController(
                        activity, feedLauncher, dialogManager, snackbarManager);
        mWebFeedFollowIntroView =
                new WebFeedFollowIntroView(
                        mActivity,
                        appMenuHandler,
                        menuButtonAnchorView,
                        mFeatureEngagementTracker,
                        this::introWasDismissed);

        mAppearanceThresholdMillis =
                TimeUnit.MINUTES.toMillis(DEFAULT_APPEARANCE_THRESHOLD_MINUTES);

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        mRecommendationFetcher.abort();
                        mRecommendationFollowAcceleratorController.dismissBubble();
                        mWebFeedFollowIntroView.dismissBubble();
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        mRecommendationFollowAcceleratorController.onDidFinishNavigation(
                                tab, navigationHandle);
                    }

                    @Override
                    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                        // Note that we're using didFirstVisuallyNonEmptyPaint as a proxy for a page
                        // load event because some pages never fully load even though they are
                        // perfectly interactive.
                        GURL url = tab.getUrl();
                        // TODO(crbug.com/40158714): Also check for certificate errors or
                        // SafeBrowser
                        // warnings.
                        if (tab.isIncognito()) {
                            Log.i(TAG, "No intro: tab is incognito");
                            return;
                        } else if (!(url.getScheme().equals("http")
                                || url.getScheme().equals("https"))) {
                            Log.i(
                                    TAG,
                                    "No intro: URL scheme is not HTTP or HTTPS: "
                                            + url.getValidSpecOrEmpty());
                            return;
                        }

                        if (mRecommendationFollowAcceleratorController
                                .showIfPageIsFromRecommendation(tab)) {
                            return;
                        }

                        mRecommendationFetcher.beginFetch(
                                tab,
                                url,
                                result -> {
                                    if (result != null) {
                                        maybeShowFollowIntro(result);
                                    }
                                });
                    }
                };
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, mTabObserver, this::swapTabs);
    }

    private void introWasShown(RecommendedWebFeedInfo recommendedInfo) {
        mIntroShownForTesting = true;
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            long currentTimeMillis = mClock.currentTimeMillis();
            mSharedPreferencesManager.writeLong(
                    ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS, currentTimeMillis);
            mSharedPreferencesManager.writeLong(
                    getWebFeedIntroWebFeedIdShownTimeMsKey(recommendedInfo.webFeedId),
                    currentTimeMillis);

            String showCountKey = getWebFeedIntroWebFeedIdShownCountKey(recommendedInfo.webFeedId);
            long readCountBefore = mSharedPreferencesManager.readLong(showCountKey);
            mSharedPreferencesManager.writeLong(showCountKey, readCountBefore + 1);
        }
        Log.i(TAG, "Allowed intro: all requirements met");
    }

    private void introWasNotShown() {
        Log.i(TAG, "No intro: not allowed by feature engagement tracker");
    }

    private void introWasDismissed() {
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            mFeatureEngagementTracker.dismissed(FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE);
        }
    }

    public void destroy() {
        mCurrentTabObserver.destroy();
    }

    private void swapTabs(Tab tab) {
        mRecommendationFetcher.abort();
        mIntroShownForTesting = false;
    }

    private void maybeShowFollowIntro(RecommendedWebFeedInfo recommendedInfo) {
        if (!basicFollowIntroChecks(recommendedInfo)) return;

        // Note: the maximum number of weekly appearances is controlled by calls to
        // FeatureEngagementTrackerbased based on the configuration used for this IPH. See the
        // kIPHWebFeedFollowFeature entry in
        // components/feature_engagement/public/feature_configurations.cc.
        maybeShowIPH(recommendedInfo);
    }

    private void maybeShowIPH(RecommendedWebFeedInfo recommendedInfo) {
        UserEducationHelper helper = new UserEducationHelper(mActivity, mProfile, new Handler());
        mWebFeedFollowIntroView.showIPH(
                helper, () -> introWasShown(recommendedInfo), this::introWasNotShown);
    }

    private void performFollowWithAccelerator(RecommendedWebFeedInfo recommendedInfo) {
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            mFeatureEngagementTracker.notifyEvent(EventConstants.WEB_FEED_FOLLOW_INTRO_CLICKED);
        }

        mWebFeedFollowIntroView.showLoadingUI();
        Tab currentTab = mTabSupplier.get();
        FeedServiceBridge.reportOtherUserAction(
                StreamKind.UNKNOWN, FeedUserActionType.TAPPED_FOLLOW_ON_FOLLOW_ACCELERATOR);
        GURL url = currentTab.getUrl();
        WebFeedBridge.followFromUrl(
                currentTab,
                url,
                WebFeedBridge.CHANGE_REASON_WEB_PAGE_ACCELERATOR,
                results ->
                        mWebFeedFollowIntroView.hideLoadingUI(
                                new LoadingView.Observer() {
                                    @Override
                                    public void onShowLoadingUIComplete() {}

                                    @Override
                                    public void onHideLoadingUIComplete() {
                                        mWebFeedFollowIntroView.dismissBubble();
                                        if (results.requestStatus
                                                == WebFeedSubscriptionRequestStatus.SUCCESS) {
                                            mWebFeedFollowIntroView.showFollowingBubble();
                                        }
                                        byte[] followId =
                                                results.metadata != null
                                                        ? results.metadata.id
                                                        : null;
                                        mWebFeedSnackbarController.showPostFollowHelp(
                                                currentTab,
                                                results,
                                                followId,
                                                url,
                                                recommendedInfo.title,
                                                WebFeedBridge.CHANGE_REASON_WEB_PAGE_ACCELERATOR);
                                    }
                                }));
    }

    /**
     * Executes some basic checks for the presentation of the intro.
     * @return true if the follow intro passes the basic checks. false otherwise.
     */
    private boolean basicFollowIntroChecks(RecommendedWebFeedInfo recommendedInfo) {
        Tab tab = mTabSupplier.get();
        if (tab == null || !tab.getUrl().equals(recommendedInfo.url)) {
            return false;
        }

        if (mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            return true;
        }

        long currentTimeMillis = mClock.currentTimeMillis();
        long timeSinceLastShown =
                currentTimeMillis
                        - mSharedPreferencesManager.readLong(
                                ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS);
        long timeSinceLastShownForWebFeed =
                currentTimeMillis
                        - mSharedPreferencesManager.readLong(
                                getWebFeedIntroWebFeedIdShownTimeMsKey(recommendedInfo.webFeedId));
        long previousShowCount =
                mSharedPreferencesManager.readLong(
                        getWebFeedIntroWebFeedIdShownCountKey(recommendedInfo.webFeedId));
        if (timeSinceLastShown < mAppearanceThresholdMillis
                || timeSinceLastShownForWebFeed < WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS
                || previousShowCount >= WEB_FEED_ID_MAX_APPEARANCES) {
            Log.i(
                    TAG,
                    "No intro: enoughTimeSinceLastShown=%s, "
                            + "enoughTimeSinceLastShownForWebFeed=%s"
                            + "tooManyShows=%s",
                    timeSinceLastShown > mAppearanceThresholdMillis,
                    timeSinceLastShownForWebFeed > WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS,
                    previousShowCount >= WEB_FEED_ID_MAX_APPEARANCES);
            return false;
        }

        return true;
    }

    private static String getWebFeedIntroWebFeedIdShownTimeMsKey(byte[] webFeedId) {
        return ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX.createKey(
                Base64.encodeToString(webFeedId, Base64.DEFAULT));
    }

    private static String getWebFeedIntroWebFeedIdShownCountKey(byte[] webFeedId) {
        return ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_COUNT_PREFIX.createKey(
                Base64.encodeToString(webFeedId, Base64.DEFAULT));
    }

    boolean getIntroShownForTesting() {
        return mIntroShownForTesting;
    }

    void clearIntroShownForTesting() {
        mIntroShownForTesting = false;
    }

    EmptyTabObserver getEmptyTabObserverForTesting() {
        return mTabObserver;
    }

    void setClockForTesting(Clock clock) {
        mClock = clock;
    }

    @VisibleForTesting
    WebFeedRecommendationFollowAcceleratorController
            getRecommendationFollowAcceleratorController() {
        return mRecommendationFollowAcceleratorController;
    }

    private static class RecommendationInfoFetcher {
        private final int mNumVisitMin;
        private final int mDailyVisitMin;
        private final PrefService mPrefService;
        private Request mRequest;

        private static class Request {
            public Tab tab;
            public GURL url;
            public Callback<RecommendedWebFeedInfo> callback;
        }

        RecommendationInfoFetcher(PrefService prefService) {
            mPrefService = prefService;
            mNumVisitMin = DEFAULT_NUM_VISIT_MIN;
            mDailyVisitMin = DEFAULT_DAILY_VISIT_MIN;
        }

        /**
         * Fetch RecommendedWebFeedInfo for `url` if it is a recommended WebFeed, and meets the
         * visit requirement. Calls `callback` with the result after the appropriate wait time. If
         * beginFetch() is called again before the result is returned, the old callback will not be
         * called.
         */
        void beginFetch(Tab tab, GURL url, Callback<RecommendedWebFeedInfo> callback) {
            Request request = new Request();
            mRequest = request;
            request.tab = tab;
            request.url = url;
            request.callback = callback;

            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // Skip visit counts check if debug mode is enabled.
                        if (mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
                            Log.i(TAG, "Intro debug mode is enabled: some checks will be skipped");
                            fetchWebFeedInfoIfRecommended(request);
                        } else {
                            fetchVisitCounts(request);
                        }
                    },
                    DEFAULT_WAIT_TIME_MILLIS);
        }

        /** Abort a previous `beginFetch()` call, its callback will not be invoked. */
        void abort() {
            mRequest = null;
        }

        private void fetchVisitCounts(Request request) {
            if (!prerequisitesMet(request)) {
                return;
            }
            WebFeedBridge.getVisitCountsToHost(
                    request.url,
                    result -> {
                        boolean meetsVisitRequirement =
                                result.visits >= mNumVisitMin
                                        && result.dailyVisits >= mDailyVisitMin;
                        if (!meetsVisitRequirement) {
                            Log.i(
                                    TAG,
                                    "No intro: visit requirement not met. totalVisits=%s"
                                            + " (minToShow=%s),  dailyVisits=%s (minToShow=%s)",
                                    result.visits,
                                    mNumVisitMin,
                                    result.dailyVisits,
                                    mDailyVisitMin);
                            sendResult(request, null);
                            return;
                        }
                        fetchWebFeedInfoIfRecommended(request);
                    });
        }

        private void fetchWebFeedInfoIfRecommended(Request request) {
            if (!prerequisitesMet(request)) {
                sendResult(request, null);
                return;
            }

            Callback<WebFeedBridge.WebFeedMetadata> metadata_callback =
                    result -> {
                        // Shouldn't be recommended if there's no metadata, ID doesn't exist, or if
                        // it is already followed.
                        if (result != null
                                && result.id != null
                                && result.id.length > 0
                                && result.isRecommended
                                && result.subscriptionStatus
                                        == WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
                            RecommendedWebFeedInfo recommendedInfo = new RecommendedWebFeedInfo();
                            recommendedInfo.webFeedId = result.id;
                            recommendedInfo.title = result.title;
                            recommendedInfo.url = request.url;

                            sendResult(request, recommendedInfo);
                        } else {
                            if (result != null) {
                                Log.i(
                                        TAG,
                                        "No intro: Web Feed exists, but not suitable. "
                                                + "recommended=%s status=%s",
                                        result.isRecommended,
                                        result.subscriptionStatus);
                            } else {
                                Log.i(TAG, "No intro: No web feed metadata found");
                            }

                            sendResult(request, null);
                        }
                    };

            WebFeedBridge.getWebFeedMetadataForPage(
                    request.tab,
                    request.url,
                    WebFeedPageInformationRequestReason.FOLLOW_RECOMMENDATION,
                    metadata_callback);
        }

        private void sendResult(Request request, RecommendedWebFeedInfo result) {
            if (mRequest == request) {
                request.callback.onResult(prerequisitesMet(request) ? result : null);
            }
        }

        private boolean prerequisitesMet(Request request) {
            return mRequest == request && request.tab.getUrl().equals(request.url);
        }
    }
}
