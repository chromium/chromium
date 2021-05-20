// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.util.Base64;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
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
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Controls when and how the Web Feed follow intro is shown.
 */
public class WebFeedFollowIntroController {
    static final long INTRO_WAIT_TIME_MS = TimeUnit.SECONDS.toMillis(5);
    // Visit history requirements.
    static final int DEFAULT_DAILY_VISIT_MIN = 3;
    static final int DEFAULT_NUM_VISIT_MIN = 5;
    static final String PARAM_DAILY_VISIT_MIN = "intro-daily-visit-min";
    static final String PARAM_NUM_VISIT_MIN = "intro-num-visit-min";
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

    private final long mAppearanceThresholdMs;

    private boolean mAcceleratorPressed;
    private boolean mIntroShown;
    private boolean mIsRecommended;
    private boolean mMeetsVisitRequirement;
    private long mPageLoadTime;
    private byte[] mWebFeedId;
    private GURL mUrl;
    private String mTitle;

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
     * @param webFeedBridge The {@link WebFeedBridge} to connect to the Web Feed backend.
     */
    public WebFeedFollowIntroController(Activity activity, AppMenuHandler appMenuHandler,
            ObservableSupplier<Tab> tabSupplier, View menuButtonAnchorView,
            FeedLauncher feedLauncher, ModalDialogManager dialogManager,
            SnackbarManager snackbarManager, WebFeedBridge webFeedBridge) {
        mActivity = activity;
        mFeatureEngagementTracker =
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        mWebFeedSnackbarController = new WebFeedSnackbarController(
                activity, feedLauncher, dialogManager, snackbarManager, webFeedBridge);
        mWebFeedFollowIntroView =
                new WebFeedFollowIntroView(mActivity, appMenuHandler, menuButtonAnchorView);

        mAppearanceThresholdMs = TimeUnit.MINUTES.toMillis(
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(ChromeFeatureList.WEB_FEED,
                        PARAM_APPEARANCE_THRESHOLD_MINUTES, DEFAULT_APPEARANCE_THRESHOLD_MINUTES));

        int numVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_NUM_VISIT_MIN, DEFAULT_NUM_VISIT_MIN);
        int dailyVisitMin = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.WEB_FEED, PARAM_DAILY_VISIT_MIN, DEFAULT_DAILY_VISIT_MIN);
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                clearPageInfo();
                mWebFeedFollowIntroView.dismissBubble();
            }

            @Override
            public void onContentViewScrollOffsetChanged(int verticalScrollDelta) {
                if (verticalScrollDelta > 0) {
                    maybeShowFollowIntro();
                }
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                // Note that we're using didFirstVisuallyNonEmptyPaint as a proxy for a page load
                // event because some pages never fully load even though they are perfectly
                // interactive.
                GURL url = tab.getUrl();
                if (tab.isIncognito()
                        || !(url.getScheme().equals("http") || url.getScheme().equals("https"))) {
                    return;
                }

                mPageLoadTime = mClock.currentTimeMillis();

                webFeedBridge.getVisitCountsToHost(url,
                        result
                        -> mMeetsVisitRequirement = result.visits >= numVisitMin
                                && result.dailyVisits >= dailyVisitMin);
                webFeedBridge.getWebFeedMetadataForPage(tab, url, result -> {
                    // Shouldn't be recommended if there's no metadata or if the ID doesn't exist.
                    if (result == null || result.id == null || result.id.length == 0) {
                        mIsRecommended = false;
                        return;
                    }
                    mWebFeedId = result.id;
                    mIsRecommended = result.isRecommended
                            && result.subscriptionStatus
                                    == WebFeedSubscriptionStatus.NOT_SUBSCRIBED;
                    mTitle = result.title;
                    mUrl = url;
                });
            }
        };
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, mTabObserver, this::swapTabs);
    }

    public void destroy() {
        mCurrentTabObserver.destroy();
    }

    private void swapTabs(Tab tab) {
        clearPageInfo();
    }

    private void clearPageInfo() {
        mIntroShown = false;
        mIsRecommended = false;
        mAcceleratorPressed = false;
        mMeetsVisitRequirement = false;
    }

    private void maybeShowFollowIntro() {
        if (!shouldShowFollowIntro()) {
            return;
        }
        // TODO(crbug/1152592): Add IPH variation.
        showFollowAccelerator();
    }

    private void showFollowAccelerator() {
        mIntroShown = true;
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            long currentTimeMillis = mClock.currentTimeMillis();
            mSharedPreferencesManager.writeLong(
                    ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS, currentTimeMillis);
            mSharedPreferencesManager.writeLong(
                    getWebFeedIntroWebFeedIdShownTimeMsKey(mWebFeedId), currentTimeMillis);
        }

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
        mWebFeedFollowIntroView.showAccelerator(onTouchListener, mFeatureEngagementTracker);
    }

    private void performFollowWithAccelerator() {
        if (!mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            mFeatureEngagementTracker.notifyEvent(EventConstants.WEB_FEED_FOLLOW_INTRO_CLICKED);
        }

        mWebFeedFollowIntroView.showLoadingUI();
        WebFeedBridge bridge = new WebFeedBridge();
        bridge.followFromId(mWebFeedId,
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
                        mWebFeedSnackbarController.showPostFollowHelp(
                                results, followId, mUrl, mTitle);
                    }
                }));
    }

    private boolean shouldShowFollowIntro() {
        if (!mIsRecommended) return false;

        long currentTimeMillis = mClock.currentTimeMillis();
        boolean hasBeenOnPageLongEnough = (currentTimeMillis - mPageLoadTime) > INTRO_WAIT_TIME_MS;
        if (mPrefService.getBoolean(Pref.ENABLE_WEB_FEED_FOLLOW_INTRO_DEBUG)) {
            return !mIntroShown && hasBeenOnPageLongEnough;
        }

        long lastShownTime = mSharedPreferencesManager.readLong(
                ChromePreferenceKeys.WEB_FEED_INTRO_LAST_SHOWN_TIME_MS);
        long lastShownForWebFeedIdMs = mSharedPreferencesManager.readLong(
                getWebFeedIntroWebFeedIdShownTimeMsKey(mWebFeedId));
        return !mIntroShown && mMeetsVisitRequirement && hasBeenOnPageLongEnough
                && ((currentTimeMillis - lastShownTime) > mAppearanceThresholdMs)
                && ((currentTimeMillis - lastShownForWebFeedIdMs)
                        > WEB_FEED_ID_APPEARANCE_THRESHOLD_MILLIS)
                && mFeatureEngagementTracker.shouldTriggerHelpUI(
                        FeatureConstants.IPH_WEB_FEED_FOLLOW_FEATURE);
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
