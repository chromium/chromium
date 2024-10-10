// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.app.Activity;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

/**
 * Controls showing the follow accelerator upon navigation to pages from a Following Feed
 * recommendation (a recommendation card within the feed).
 */
public class WebFeedRecommendationFollowAcceleratorController {
    /** We use UserData to put the web feed name into the tab and the NavigationHandle. */
    @VisibleForTesting
    private static class AssociatedWebFeedData implements UserData {
        byte[] mWebFeedName;

        public AssociatedWebFeedData(byte[] webFeedName) {
            mWebFeedName = webFeedName;
        }
    }

    /** Put the web feed name into a passed in UserDataHost. */
    @VisibleForTesting
    public static void associateWebFeedWithUserData(UserDataHost host, byte[] webFeedName) {
        host.setUserData(AssociatedWebFeedData.class, new AssociatedWebFeedData(webFeedName));
    }

    private final Activity mActivity;
    private final WebFeedFollowIntroView mWebFeedFollowIntroView;
    private final Supplier<Tab> mTabSupplier;
    private final WebFeedSnackbarController mWebFeedSnackbarController;

    /**
     * Constructs an instance of {@link WebFeedRecommendationFollowAcceleratorController}.
     *
     * @param activity The current {@link Activity}.
     * @param appMenuHandler The {@link AppMenuHandler} to highlight the Web Feed menu item.
     * @param tabSupplier The supplier for the currently active {@link Tab}.
     * @param menuButtonAnchorView The menu button {@link View} to serve as an anchor.
     * @param feedLauncher The {@link FeedLauncher} to launch the feed.
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager The {@link SnackbarManager} to show snackbars.
     */
    public WebFeedRecommendationFollowAcceleratorController(
            Activity activity,
            AppMenuHandler appMenuHandler,
            Supplier<Tab> tabSupplier,
            View menuButtonAnchorView,
            FeedLauncher feedLauncher,
            ModalDialogManager dialogManager,
            SnackbarManager snackbarManager) {
        mActivity = activity;
        mTabSupplier = tabSupplier;
        mWebFeedSnackbarController =
                new WebFeedSnackbarController(
                        activity, feedLauncher, dialogManager, snackbarManager);
        // featureEngagementTracker is set to null because we don't want to use IPH conditions for
        // showing the accelerator.
        mWebFeedFollowIntroView =
                new WebFeedFollowIntroView(
                        mActivity,
                        appMenuHandler,
                        menuButtonAnchorView,
                        /* featureEngagementTracker= */ null,
                        /* introDismissedCallback= */ CallbackUtils.emptyRunnable());
    }

    /** Dismiss the Follow bubble if it is showing. */
    public void dismissBubble() {
        mWebFeedFollowIntroView.dismissBubble();
    }

    /**
     * Should be called whenever a tab finishes navigation. Updates user data on the tab to remember
     * whether the tab is showing the result of a navigation from a recommendation.
     */
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
        if (!navigationHandle.isInPrimaryMainFrame() || navigationHandle.isSameDocument()) {
            return;
        }
        mWebFeedFollowIntroView.dismissBubble();

        byte[] webFeedName = getWebFeedNameIfNavigationIsForRecommendation(navigationHandle);
        if (webFeedName != null) {
            associateWebFeedWithUserData(tab.getUserDataHost(), webFeedName);
        } else {
            if (tab.getUserDataHost() != null
                    && tab.getUserDataHost().getUserData(AssociatedWebFeedData.class) != null) {
                tab.getUserDataHost().removeUserData(AssociatedWebFeedData.class);
            }
        }
    }

    /**
     * Shows the follow accelerator if the current tab is the direct result of navigating from
     * a Following feed recommendation, and if the user is not already following this site. Returns
     * true if this tab is a result of navigating from a Following feed recommendation.
     */
    public boolean showIfPageIsFromRecommendation(Tab tab) {
        byte[] webFeedId = getWebFeedNameIfPageIsRecommended(tab);
        if (webFeedId == null) return false;
        WebFeedBridge.getWebFeedMetadata(
                webFeedId,
                (WebFeedMetadata metadata) -> {
                    if (metadata != null
                            && metadata.subscriptionStatus
                                    != WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
                        return;
                    }
                    showAccelerator(webFeedId);
                });
        return true;
    }

    private void showAccelerator(byte[] webFeedName) {
        GestureDetector gestureDetector =
                new GestureDetector(
                        mActivity.getApplicationContext(),
                        new GestureDetector.SimpleOnGestureListener() {
                            private boolean mPressed;

                            @Override
                            public boolean onSingleTapUp(MotionEvent motionEvent) {
                                if (!mPressed) {
                                    mPressed = true;
                                    performFollowWithAccelerator(webFeedName);
                                }
                                return true;
                            }
                        });
        View.OnTouchListener onTouchListener =
                (view, motionEvent) -> {
                    view.performClick();
                    gestureDetector.onTouchEvent(motionEvent);
                    return true;
                };

        mWebFeedFollowIntroView.showAccelerator(
                onTouchListener,
                /* introShownCallback= */ CallbackUtils.emptyRunnable(),
                /*introNotShownCallback*/ CallbackUtils.emptyRunnable());
    }

    private void performFollowWithAccelerator(byte[] webFeedId) {
        mWebFeedFollowIntroView.showLoadingUI();
        Tab currentTab = mTabSupplier.get();
        FeedServiceBridge.reportOtherUserAction(
                StreamKind.UNKNOWN,
                FeedUserActionType.TAPPED_FOLLOW_ON_RECOMMENDATION_FOLLOW_ACCELERATOR);
        GURL url = currentTab.getUrl();
        WebFeedBridge.followFromId(
                webFeedId,
                /* isDurable= */ true,
                WebFeedBridge.CHANGE_REASON_RECOMMENDATION_WEB_PAGE_ACCELERATOR,
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
                                        mWebFeedSnackbarController.showPostFollowHelp(
                                                currentTab,
                                                results,
                                                webFeedId,
                                                url,
                                                results.metadata.title,
                                                WebFeedBridge
                                                        .CHANGE_REASON_RECOMMENDATION_WEB_PAGE_ACCELERATOR);
                                    }
                                }));
    }

    public WebFeedFollowIntroView getIntroViewForTesting() {
        return mWebFeedFollowIntroView;
    }

    public static byte[] getWebFeedNameIfPageIsRecommended(Tab tab) {
        UserDataHost userDataHost = tab.getUserDataHost();
        if (userDataHost == null) return null;
        AssociatedWebFeedData userData = userDataHost.getUserData(AssociatedWebFeedData.class);
        return userData != null ? userData.mWebFeedName : null;
    }

    @Nullable
    public static byte[] getWebFeedNameIfNavigationIsForRecommendation(NavigationHandle handle) {
        UserDataHost userDataHost = handle.getUserDataHost();
        if (userDataHost == null) return null;
        AssociatedWebFeedData userData = userDataHost.getUserData(AssociatedWebFeedData.class);
        return userData != null ? userData.mWebFeedName : null;
    }

    /**
     * Add extra data to params so that we can identify the subsequent navigation as belonging to a
     * Follow recommendation.
     */
    public static void updateUrlParamsForRecommendedWebFeed(
            LoadUrlParams params, byte[] webFeedName) {
        associateWebFeedWithUserData(params.getNavigationHandleUserData(), webFeedName);
    }

    @VisibleForTesting
    public static byte[] getWebFeedNameIfInLoadUrlParams(LoadUrlParams params) {
        AssociatedWebFeedData userData =
                params.getNavigationHandleUserData().getUserData(AssociatedWebFeedData.class);
        return userData != null ? userData.mWebFeedName : null;
    }
}
