// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.os.Handler;
import android.util.Base64;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
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
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Controls when and how the Web Feed follow intro is shown.
 */
public class WebFeedFollowIntroController {
    private static final String TAG = "WFFollowIntroCtrl";

    // Intro style control
    private static final String PARAM_INTRO_STYLE = "intro_style";
    private static final String INTRO_STYLE_IPH = "IPH";
    private static final String INTRO_STYLE_ACCELERATOR = "accelerator";
    // In-page time delay to show the intro.
    private static final int DEFAULT_WAIT_TIME_MILLIS = 3 * 1000;
    private static final String PARAM_WAIT_TIME_MILLIS = "intro-wait-time-millis";
    // Visit history requirements.
    private static final int DEFAULT_DAILY_VISIT_MIN = 3;
    private static final int DEFAULT_NUM_VISIT_MIN = 3;
    private static final String PARAM_DAILY_VISIT_MIN = "intro-daily-visit-min";
    private static final String PARAM_NUM_VISIT_MIN = "intro-num-visit-min";
    // Time between appearances.
    private static final int DEFAULT_APPEARANCE_THRESHOLD_MINUTES = 15;
    private static final String PARAM_APPEARANCE_THRESHOLD_MINUTES =
            "intro-appearance-threshold-minutes";
    // Time between appearances for the same WebFeedId.
    private static final long WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS = TimeUnit.DAYS.toMillis(1);

    /** Clock to use so we can mock time in tests. */
    public interface Clock {
        long currentTimeMillis();
    }
    private Clock mClock = System::currentTimeMillis;

    private final Activity mActivity;
    private final CurrentTabObserver mCurrentTabObserver;
    private final EmptyTabObserver mTabObserver;
    private final PrefService mPrefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();
    private final Tracker mFeatureEngagementTracker;
    private final WebFeedSnackbarController mWebFeedSnackbarController;
    private final WebFeedFollowIntroView mWebFeedFollowIntroView;
    private final ObservableSupplier<Tab> mTabSupplier;

    private final long mWaitTimeMillis;
    private final long mAppearanceThresholdMillis;

    private boolean mAcceleratorPressed;
    private boolean mIntroShown;
    private boolean mMeetsVisitRequirement;

    private class RecommendedWebFeedInfo {
        public byte[] webFeedId;
        public GURL url;
        public String title;
    }
    private RecommendedWebFeedInfo mRecommendedInfo;

    /**
     * Constructs an instance of {@link WebFeedFollowIntroController}.
     *
     * @param activity The current {@link Activity}.
     * @param appMenuHandler The {@link AppMenuHandler} to highlight the Web Feed menu item.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     * @param feedLauncher The {@link FeedLauncher} to launch the feed.
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager The {@link SnackbarManager} to show snackbars.
     */
    public WebFeedFollowIntroController(Activity activity, AppMenuHandler appMenuHandler,
            ObservableSupplier<Tab> tabSupplier, View menuButtonAnchorView,
            FeedLauncher feedLauncher, ModalDialogManager dialogManager,
            SnackbarManager snackbarManager) {
        mActivity = activity;
        mTabSupplier = tabSupplier;
        mFeatureEngagementTracker =
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        mWebFeedSnackbarController = new WebFeedSnackbarController(
                activity, feedLauncher, dialogManager, snackbarManager);
        mWebFeedFollowIntroView = new WebFeedFollowIntroView(mActivity, appMenuHandler,
                menuButtonAnchorView, mFeatureEngagementTracker, this::introWasShown,
                this::introWasNotShown, this::introWasDismissed);

        mWaitTimeMillis = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_WAIT_TIME_MILLIS, DEFAULT_WAIT_TIME_MILLIS);
        mAppearanceThresholdMillis = TimeUnit.MINUTES.toMillis(
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(ChromeFeatureList.WEB_FEED,
                        PARAM_APPEARANCE_THRESHOLD_MINUTES, DEFAULT_APPEARANCE_THRESHOLD_MINUTES));

        int numVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_NUM_VISIT_MIN, DEFAULT_NUM_VISIT_MIN);
        int dailyVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_DAILY_VISIT_MIN, DEFAULT_DAILY_VISIT_MIN);
        Log.i(TAG, "Minimum visit requirements set: minTotalVisits=%s, minDailyVisits=%s",
                numVisitMin, dailyVisitMin);
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                clearPageInfo();
                mWebFeedFollowIntroView.dismissBubble();
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                // Note that we're using didFirstVisuallyNonEmptyPaint as a proxy for a page load
                // event because some pages never fully load even though they are perfectly
                // interactive.
                GURL url = tab.getUrl();
                // TODO(crbug/1152592): Also check for certificate errors or SafeBrowser warnings.
                if (tab.isIncognito()
                        || !(url.getScheme().equals("http") || url.getScheme().equals("https"))) {
                    Log.i(TAG, "No intro: URL scheme is not HTTP or HTTPS");
                    return;
                }

                WebFeedBridge.getVisitCountsToHost(url, result -> {
                    mMeetsVisitRequirement =
                            result.visits >= numVisitMin && result.dailyVisits >= dailyVisitMin;
                    Log.i(TAG,
                            "Host visits queried: totalVisits=%s (minToShow=%s), dailyVisits=%s "
                                    + "(minToShow=%s), meetsRequirements=%s",
                            result.visits, numVisitMin, result.dailyVisits, dailyVisitMin,
                            mMeetsVisitRequirement);
                });
                WebFeedBridge.getWebFeedMetadataForPage(tab, url, result -> {
                    // Shouldn't show intro if there's no metadata, ID doesn't exist, it it's not
                    // recommended or if it is already followed.
                    if (result != null && result.id != null && result.id.length > 0
                            && result.isRecommended
                            && result.subscriptionStatus
                                    == WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
                        mRecommendedInfo = new RecommendedWebFeedInfo();
                        mRecommendedInfo.webFeedId = result.id;
                        mRecommendedInfo.title = result.title;
                        mRecommendedInfo.url = url;
                    } else {
                        mRecommendedInfo = null;
                    }
                    Log.i(TAG, "Web Feed metadata queried: isRecommended=%s",
                            mRecommendedInfo != null);
                });

                // The requests for information above should all be done by the time this delayed
                // task is executed.
                PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                        WebFeedFollowIntroController.this::maybeShowFollowIntro, mWaitTimeMillis);
            }
        };
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, mTabObserver, this::swapTabs);
    }

    private void introWasShown() {
        mIntroShown = true;
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            long currentTimeMillis = mClock.currentTimeMillis();
            mSharedPreferencesManager.writeLong(
                    ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS, currentTimeMillis);
            mSharedPreferencesManager.writeLong(
                    getWebFeedIntroWebFeedIdShownTimeMsKey(mRecommendedInfo.webFeedId),
                    currentTimeMillis);
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
        clearPageInfo();
    }

    private void clearPageInfo() {
        mIntroShown = false;
        mRecommendedInfo = null;
        mAcceleratorPressed = false;
        mMeetsVisitRequirement = false;
    }

    private void maybeShowFollowIntro() {
        if (!basicFollowIntroChecks()) return;

        // Note: the maximum number of weekly appearances is controlled by calls to
        // FeatureEngagementTrackerbased based on the configuration used for this IPH. See the
        // kIPHWebFeedFollowFeature entry in
        // components/feature_engagement/public/feature_configurations.cc.
        if (isIntroStyle(INTRO_STYLE_IPH)) {
            maybeShowIPH();
        } else if (isIntroStyle(INTRO_STYLE_ACCELERATOR)) {
            maybeShowAccelerator();
        } else {
            Log.i(TAG, "No intro: not enabled by Finch controls");
        }
    }

    private boolean isIntroStyle(String style) {
        return ChromeFeatureList
                .getFieldTrialParamByFeature(ChromeFeatureList.WEB_FEED, PARAM_INTRO_STYLE)
                .equals(style);
    }

    private void maybeShowIPH() {
        UserEducationHelper helper = new UserEducationHelper(mActivity, new Handler());
        mWebFeedFollowIntroView.showIPH(helper);
    }

    private void maybeShowAccelerator() {
        GestureDetector gestureDetector = new GestureDetector(
                mActivity.getApplicationContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent motionEvent) {
                        if (!mAcceleratorPressed) {
                            mAcceleratorPressed = true;
                            performFollowWithAccelerator();
                        }
                        return true;
                    }
                });
        View.OnTouchListener onTouchListener = (view, motionEvent) -> {
            view.performClick();
            gestureDetector.onTouchEvent(motionEvent);
            return true;
        };

        mWebFeedFollowIntroView.showAccelerator(onTouchListener);
    }

    private void performFollowWithAccelerator() {
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            mFeatureEngagementTracker.notifyEvent(EventConstants.WEB_FEED_FOLLOW_INTRO_CLICKED);
        }

        mWebFeedFollowIntroView.showLoadingUI();
        Tab currentTab = mTabSupplier.get();
        FeedServiceBridge.reportOtherUserAction(
                FeedUserActionType.TAPPED_FOLLOW_ON_FOLLOW_ACCELERATOR);

        WebFeedBridge.followFromId(mRecommendedInfo.webFeedId,
                results -> mWebFeedFollowIntroView.hideLoadingUI(new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {}

                    @Override
                    public void onHideLoadingUIComplete() {
                        mWebFeedFollowIntroView.dismissBubble();
                        if (results.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS) {
                            mWebFeedFollowIntroView.showFollowingBubble();
                        }
                        byte[] followId = results.metadata != null ? results.metadata.id : null;
                        mWebFeedSnackbarController.showPostFollowHelp(currentTab, results, followId,
                                mRecommendedInfo.url, mRecommendedInfo.title);
                    }
                }));
    }

    /**
     * Executes the basic checks for the presentation of the intro (all must be true):
     *  1. It was not already presented for this page
     *  2. The URL is recommended.
     *  3. This site was visited enough in day-boolean count and in total.
     *  4. Enough time has passed since the last intro was presented and since the last intro was
     *     presented for this site.
     *
     * If the intro debug mode pref is enabled then only 1. and 2. are checked for.
     *
     * Note: The feature engagement tracker check happens only later, and it includes checking for
     * a weekly limit.
     *
     * @return true if the follow intro passes the basic checks to be shown. false otherwise.
     */
    private boolean basicFollowIntroChecks() {
        if (mIntroShown) {
            Log.i(TAG, "No intro: it was already shown");
            return false;
        }

        if (mRecommendedInfo == null) {
            Log.i(TAG, "No intro: URL is not in recommended list");
            return false;
        }

        if (mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            Log.i(TAG, "Allowed intro: debug mode is enabled");
            return true;
        }

        long currentTimeMillis = mClock.currentTimeMillis();
        long timeSinceLastShown = currentTimeMillis
                - mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS);
        long timeSinceLastShownForWebFeed = currentTimeMillis
                - mSharedPreferencesManager.readLong(
                        getWebFeedIntroWebFeedIdShownTimeMsKey(mRecommendedInfo.webFeedId));
        if (!mMeetsVisitRequirement || (timeSinceLastShown < mAppearanceThresholdMillis)
                || (timeSinceLastShownForWebFeed < WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS)) {
            Log.i(TAG,
                    "No intro: mMeetsVisitRequirement=%s, enoughTimeSinceLastShown=%s, "
                            + "enoughTimeSinceLastShownForWebFeed=%s",
                    mMeetsVisitRequirement, timeSinceLastShown > mAppearanceThresholdMillis,
                    timeSinceLastShownForWebFeed > WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS);
            return false;
        }

        return true;
    }

    private static String getWebFeedIntroWebFeedIdShownTimeMsKey(byte[] webFeedId) {
        return ChromePreferenceKeys.WEB_FEED_INTRO_WEB_FEED_ID_SHOWN_TIME_MS_PREFIX.createKey(
                Base64.encodeToString(webFeedId, Base64.DEFAULT));
    }

    @VisibleForTesting
    boolean getIntroShownForTesting() {
        return mIntroShown;
    }

    @VisibleForTesting
    EmptyTabObserver getEmptyTabObserverForTesting() {
        return mTabObserver;
    }

    @VisibleForTesting
    void setClockForTesting(Clock clock) {
        mClock = clock;
    }
}
