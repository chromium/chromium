// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedAvailabilityStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedRecommendationFollowAcceleratorController;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.chrome.browser.xsurface.LoggingParameters;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.feed.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.chrome.browser.xsurface.feed.StreamType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feed.proto.FeedUiProto;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.io.UnsupportedEncodingException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * A implementation of a Feed {@link Stream} that is just able to render a vertical stream of cards
 * for Feed v2.
 */
public class FeedStream implements Stream {
    private static final String TAG = "FeedStream";
    private static final String SPACER_KEY = "Spacer";
    private static final String LOADING_SPINNER_KEY = "LoadingSpinner";
    private static final AtomicInteger sPageId = new AtomicInteger();

    /** Implementation of SurfaceActionsHandler methods. */
    @VisibleForTesting
    class FeedSurfaceActionsHandler
            implements SurfaceActionsHandler, FeedActionDelegate.PageLoadObserver {
        FeedActionDelegate mActionDelegate;

        FeedSurfaceActionsHandler(FeedActionDelegate actionDelegate) {
            mActionDelegate = actionDelegate;
        }

        @Override
        public void openUrl(@OpenMode int openMode, String url, OpenUrlOptions options) {
            assert ThreadUtils.runningOnUiThread();
            switch (openMode) {
                case OpenMode.UNKNOWN:
                case OpenMode.SAME_TAB:
                    mBridge.reportOpenAction(
                            new GURL(url),
                            getSliceIdFromView(options.actionSourceView()),
                            OpenActionType.DEFAULT);
                    openSuggestionUrl(
                            url, WindowOpenDisposition.CURRENT_TAB, /* inGroup= */ false, options);
                    break;
                case OpenMode.NEW_TAB:
                    mBridge.reportOpenAction(
                            new GURL(url),
                            getSliceIdFromView(options.actionSourceView()),
                            OpenActionType.NEW_TAB);
                    openSuggestionUrl(
                            url,
                            WindowOpenDisposition.NEW_BACKGROUND_TAB,
                            /* inGroup= */ false,
                            options);
                    break;
                case OpenMode.INCOGNITO_TAB:
                    mBridge.reportOtherUserAction(
                            FeedUserActionType.TAPPED_OPEN_IN_NEW_INCOGNITO_TAB);
                    openSuggestionUrl(
                            url,
                            WindowOpenDisposition.OFF_THE_RECORD,
                            /* inGroup= */ false,
                            options);
                    break;
                case OpenMode.DOWNLOAD_LINK:
                    mBridge.reportOtherUserAction(FeedUserActionType.TAPPED_DOWNLOAD);
                    mActionDelegate.downloadPage(url);
                    break;
                case OpenMode.READ_LATER:
                    mBridge.reportOtherUserAction(FeedUserActionType.TAPPED_ADD_TO_READING_LIST);
                    mActionDelegate.addToReadingList(options.getTitle(), url);
                    break;
                case OpenMode.NEW_TAB_IN_GROUP:
                    mBridge.reportOpenAction(
                            new GURL(url),
                            getSliceIdFromView(options.actionSourceView()),
                            OpenActionType.NEW_TAB_IN_GROUP);
                    openSuggestionUrl(
                            url,
                            WindowOpenDisposition.NEW_BACKGROUND_TAB,
                            /* inGroup= */ true,
                            options);
                    break;
            }

            // Attempts to load more content if needed.
            maybeLoadMore();
        }

        // Deprecated in favor of openUrl(), will be removed once internal references are removed.
        @Override
        public void navigateTab(String url, View actionSourceView) {
            openUrl(
                    OpenMode.SAME_TAB,
                    url,
                    new OpenUrlOptions() {
                        @Override
                        public View actionSourceView() {
                            return actionSourceView;
                        }
                    });
        }

        @Override
        public void showBottomSheet(View view, View actionSourceView) {
            assert ThreadUtils.runningOnUiThread();
            dismissBottomSheet();

            mBridge.reportOtherUserAction(FeedUserActionType.OPENED_CONTEXT_MENU);

            // Remember the currently focused view so that we can get back to it once the bottom
            // sheet is closed. This is to fix the problem that the last focused view is not
            // restored after opening and closing the bottom sheet.
            mLastFocusedView = mActivity.getCurrentFocus();
            // If the talkback is enabled, also remember the accessibility focused view, which may
            // be different from the focused view, so that we can get back to it once the bottom
            // sheet is closed.
            mLastAccessibilityFocusedView = findAccessibilityFocus(actionSourceView);

            // Make a sheetContent with the view.
            mBottomSheetContent = new CardMenuBottomSheetContent(view);
            mBottomSheetOriginatingSliceId = getSliceIdFromView(actionSourceView);
            mBottomSheetController.addObserver(
                    new EmptyBottomSheetObserver() {
                        @Override
                        public void onSheetClosed(@StateChangeReason int reason) {
                            if (mLastFocusedView != null) {
                                mLastFocusedView.requestFocus();
                                mLastFocusedView = null;
                            }
                            if (mLastAccessibilityFocusedView != null) {
                                mLastAccessibilityFocusedView.sendAccessibilityEvent(
                                        AccessibilityEvent.TYPE_VIEW_FOCUSED);
                                mLastAccessibilityFocusedView = null;
                            }
                        }
                    });
            mBottomSheetController.requestShowContent(mBottomSheetContent, true);
        }

        @Override
        public void dismissBottomSheet() {
            FeedStream.this.dismissBottomSheet();
        }

        /** Search the view hierarchy to find the accessibility focused view. */
        private View findAccessibilityFocus(View view) {
            if (view == null || view.isAccessibilityFocused()) return view;
            if (!(view instanceof ViewGroup)) return null;
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); ++i) {
                View childView = viewGroup.getChildAt(i);
                View focusedView = findAccessibilityFocus(childView);
                if (focusedView != null) return focusedView;
            }
            return null;
        }

        @Override
        public void updateUserProfileOnLinkClick(String url, List<Long> entityMids) {
            assert ThreadUtils.runningOnUiThread();
            long[] entityArray = new long[entityMids.size()];
            for (int i = 0; i < entityMids.size(); ++i) {
                entityArray[i] = entityMids.get(i);
            }
            mBridge.updateUserProfileOnLinkClick(new GURL(url), entityArray);
        }

        @Override
        public void updateWebFeedFollowState(WebFeedFollowUpdate update) {
            byte[] webFeedId;
            try {
                webFeedId = update.webFeedName().getBytes("UTF8");
            } catch (UnsupportedEncodingException e) {
                Log.i(TAG, "Invalid webFeedName", e);
                return;
            }
            WebFeedFollowUpdate.Callback updateCallback = update.callback();
            if (update.isFollow()) {
                Callback<WebFeedBridge.FollowResults> followCallback =
                        results -> {
                            boolean successfulFollow =
                                    results.requestStatus
                                            == WebFeedSubscriptionRequestStatus.SUCCESS;
                            if (updateCallback != null) {
                                updateCallback.requestComplete(successfulFollow);
                            }
                            if (successfulFollow && results.metadata != null) {
                                mWebFeedSnackbarController.showPostSuccessfulFollowHelp(
                                        results.metadata.title,
                                        results.metadata.availabilityStatus
                                                == WebFeedAvailabilityStatus.ACTIVE,
                                        mStreamKind,
                                        /* tab= */ null,
                                        /* url= */ null);
                            }
                        };
                WebFeedBridge.followFromId(
                        webFeedId,
                        update.isDurable(),
                        update.webFeedChangeReason(),
                        followCallback);
            } else {
                WebFeedBridge.unfollow(
                        webFeedId,
                        update.isDurable(),
                        update.webFeedChangeReason(),
                        results -> {
                            if (updateCallback != null) {
                                updateCallback.requestComplete(
                                        results.requestStatus
                                                == WebFeedSubscriptionRequestStatus.SUCCESS);
                            }
                        });
            }
        }

        @Override
        public void openWebFeed(String webFeedName, @OpenWebFeedEntryPoint int entryPoint) {
            @SingleWebFeedEntryPoint int singleWebFeedEntryPoint;

            switch (entryPoint) {
                case OpenWebFeedEntryPoint.ATTRIBUTION:
                    singleWebFeedEntryPoint = SingleWebFeedEntryPoint.ATTRIBUTION;
                    break;
                case OpenWebFeedEntryPoint.RECOMMENDATION:
                    singleWebFeedEntryPoint = SingleWebFeedEntryPoint.RECOMMENDATION;
                    break;
                case OpenWebFeedEntryPoint.GROUP_HEADER:
                    singleWebFeedEntryPoint = SingleWebFeedEntryPoint.GROUP_HEADER;
                    break;

                default:
                    singleWebFeedEntryPoint = SingleWebFeedEntryPoint.OTHER;
            }

            mActionDelegate.openWebFeed(webFeedName, singleWebFeedEntryPoint);
        }

        private void openSuggestionUrl(
                String url, int disposition, boolean inGroup, OpenUrlOptions openOptions) {
            int pageId = sPageId.incrementAndGet();
            if (disposition != WindowOpenDisposition.NEW_BACKGROUND_TAB
                    && mReliabilityLogger != null) {
                // TODO(crbug.com/338585368): Add card category.
                mReliabilityLogger.onOpenCard(pageId, 0);
                mClosedReason = ClosedReason.OPEN_CARD;
            }

            LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
            if (openOptions.shouldShowWebFeedAccelerator()) {
                WebFeedRecommendationFollowAcceleratorController
                        .updateUrlParamsForRecommendedWebFeed(
                                params, openOptions.webFeedName().getBytes(StandardCharsets.UTF_8));
            }

            // This postTask is necessary so that other click-handlers have a chance
            // to run before we begin navigating. On start surface, navigation immediately
            // triggers unbind, which can break event handling.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        mActionDelegate.openSuggestionUrl(
                                disposition,
                                params,
                                inGroup,
                                pageId,
                                /* pageLoadObserver= */ this,
                                visitResult ->
                                        mBridge.reportOpenVisitComplete(visitResult.visitTimeMs));
                    });
        }

        @Override
        public void showSyncConsentPrompt() {
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                startSigninFlow();
            } else {
                mActionDelegate.showSyncConsentActivity(SigninAccessPoint.NTP_FEED_BOTTOM_PROMO);
            }
        }

        @Override
        public void startSigninFlow() {
            mActionDelegate.startSigninFlow(SigninAccessPoint.NTP_FEED_BOTTOM_PROMO);
        }

        @Override
        public void showSignInInterstitial() {
            mActionDelegate.showSignInInterstitial(
                    SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO,
                    mBottomSheetController,
                    mWindowAndroid);
        }

        @Override
        public void onPageLoadStarted(int pageId) {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onPageLoadStarted(pageId);
            }
        }

        @Override
        public void onPageLoadFinished(int pageId, boolean inNewTab) {
            mBridge.reportPageLoaded(inNewTab);
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onPageLoadFinished(pageId);
            }
        }

        @Override
        public void onPageLoadFailed(int pageId, @NetError int errorCode) {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onPageLoadFailed(pageId, errorCode);
            }
        }

        @Override
        public void onPageFirstContentfulPaint(int pageId) {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onPageFirstContentfulPaint(pageId);
            }
        }
    }

    // Tracks in-progress work, primarily for work done by xsurface.
    class InProgressWorkTracker {
        private int mNextWorkId;
        private final HashSet<Integer> mActiveWork = new HashSet<>();
        private final ObservableSupplierImpl<Boolean> mWorkPending = new ObservableSupplierImpl<>();

        InProgressWorkTracker() {
            // ObservableSupplierImpl holds null by default.
            mWorkPending.set(false);
        }

        /**
         * Record that background work has begun, returns a runnable to be called when work is
         * complete.
         */
        Runnable addWork() {
            int id = mNextWorkId++;
            mActiveWork.add(id);
            mWorkPending.set(true);
            return () -> finishWork(id);
        }

        /** postTask to call runnable after all in-progress work is complete. */
        void postTaskAfterWorkComplete(Runnable runnable) {
            if (!mWorkPending.get()) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, runnable);
            } else {
                new DoneWatcher(runnable);
            }
        }

        /** Calls a runnable with postTask when mWorkPending is false. */
        private class DoneWatcher implements Callback<Boolean> {
            private final Runnable mDelegate;

            DoneWatcher(Runnable runnable) {
                mDelegate = runnable;
                mWorkPending.addObserver(this);
            }

            @Override
            public void onResult(Boolean workPending) {
                if (!workPending) {
                    PostTask.postTask(TaskTraits.UI_DEFAULT, mDelegate);
                    mWorkPending.removeObserver(this);
                }
            }
        }

        private void finishWork(int workId) {
            mActiveWork.remove(workId);
            if (mActiveWork.isEmpty()) {
                mWorkPending.set(false);
            }
        }
    }

    /** Implementation of FeedActionsHandler methods. */
    class FeedActionsHandlerImpl implements FeedActionsHandler {
        private static final int SNACKBAR_DURATION_MS_SHORT = 4000;
        private static final int SNACKBAR_DURATION_MS_LONG = 10000;
        // This is based on the menu animation time (218ms) from BottomSheet.java.
        // It is private to an internal target, so we can't link, to it here.
        private static final int MENU_DISMISS_TASK_DELAY = 318;

        @VisibleForTesting
        static final String FEEDBACK_REPORT_TYPE =
                "com.google.chrome.feed.USER_INITIATED_FEEDBACK_REPORT";

        @VisibleForTesting static final String XSURFACE_CARD_URL = "Card URL";

        @Override
        public void processThereAndBackAgainData(byte[] data, LoggingParameters loggingParameters) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.processThereAndBackAgain(
                    data, FeedLoggingParameters.convertToProto(loggingParameters).toByteArray());
        }

        @Override
        public void sendFeedback(Map<String, String> productSpecificDataMap) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.reportOtherUserAction(FeedUserActionType.TAPPED_SEND_FEEDBACK);

            String url = productSpecificDataMap.get(XSURFACE_CARD_URL);

            // We want to hide the bottom sheet before sending feedback so the snapshot doesn't show
            // the menu covering the article.  However the menu is animating down, we need to wait
            // for the animation to finish.  We post a task to wait for the duration of the
            // animation, then call send feedback.

            // FEEDBACK_REPORT_TYPE: Reports for Chrome mobile must have a contextTag of the form
            // com.chrome.feed.USER_INITIATED_FEEDBACK_REPORT, or they will be discarded for not
            // matching an allow list rule.
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () ->
                            HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                                    .showFeedback(
                                            mActivity,
                                            url,
                                            FEEDBACK_REPORT_TYPE,
                                            productSpecificDataMap),
                    MENU_DISMISS_TASK_DELAY);
        }

        @Override
        public int requestDismissal(byte[] data) {
            assert ThreadUtils.runningOnUiThread();
            return mBridge.executeEphemeralChange(data);
        }

        @Override
        public void commitDismissal(int changeId) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.commitEphemeralChange(changeId);

            // Attempts to load more content if needed.
            maybeLoadMore();
        }

        @Override
        public void discardDismissal(int changeId) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.discardEphemeralChange(changeId);
        }

        @Override
        public void showSnackbar(
                String text,
                String actionLabel,
                @FeedActionsHandler.SnackbarDuration int duration,
                FeedActionsHandler.SnackbarController delegateController) {
            assert ThreadUtils.runningOnUiThread();
            int durationMs = SNACKBAR_DURATION_MS_SHORT;
            if (duration == FeedActionsHandler.SnackbarDuration.LONG) {
                durationMs = SNACKBAR_DURATION_MS_LONG;
            }
            SnackbarManager.SnackbarController controller =
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            delegateController.onAction(mInProgressWorkTracker.addWork());
                        }

                        @Override
                        public void onDismissNoAction(Object actionData) {
                            delegateController.onDismissNoAction(mInProgressWorkTracker.addWork());
                        }
                    };

            mSnackbarControllers.add(controller);
            mSnackManager.showSnackbar(
                    Snackbar.make(
                                    text,
                                    controller,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_FEED_NTP_STREAM)
                            .setAction(actionLabel, /* actionData= */ null)
                            .setDuration(durationMs)
                            .setSingleLine(false));
        }

        @Override
        public void share(String url, String title) {
            assert ThreadUtils.runningOnUiThread();
            mShareHelper.share(url, title);
            mBridge.reportOtherUserAction(FeedUserActionType.SHARE);
        }

        @Override
        public void watchForViewFirstVisible(View view, float viewedThreshold, Runnable runnable) {
            assert ThreadUtils.runningOnUiThread();
            if (mSliceViewTracker != null) {
                mSliceViewTracker.watchForFirstVisible(
                        getSliceIdFromView(view), viewedThreshold, runnable);
            }
        }

        @Override
        public void reportInfoCardTrackViewStarted(int type) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.reportInfoCardTrackViewStarted(type);
        }

        @Override
        public void reportInfoCardViewed(int type, int minimumViewIntervalSeconds) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.reportInfoCardViewed(type, minimumViewIntervalSeconds);
        }

        @Override
        public void reportInfoCardClicked(int type) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.reportInfoCardClicked(type);
        }

        @Override
        public void reportInfoCardDismissedExplicitly(int type) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.reportInfoCardDismissedExplicitly(type);
        }

        @Override
        public void resetInfoCardStates(int type) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.resetInfoCardStates(type);
        }

        @Override
        public void contentViewed(long docId) {
            assert ThreadUtils.runningOnUiThread();
            mBridge.contentViewed(docId);
        }

        private @StreamKind int feedIdentifierToKind(@FeedIdentifier int fid) {
            switch (fid) {
                case FeedIdentifier.MAIN_FEED:
                    return StreamKind.FOR_YOU;
                case FeedIdentifier.FOLLOWING_FEED:
                    return StreamKind.FOLLOWING;
            }
            return StreamKind.UNKNOWN;
        }

        @Override
        public void invalidateContentCacheFor(@FeedIdentifier int feedToInvalidate) {
            @StreamKind int streamKindToInvalidate = feedIdentifierToKind(feedToInvalidate);
            if (streamKindToInvalidate != StreamKind.UNKNOWN) {
                mBridge.invalidateContentCacheFor(streamKindToInvalidate);
            }
        }

        @Override
        public void triggerManualRefresh() {
            mBridge.reportOtherUserAction(FeedUserActionType.NON_SWIPE_MANUAL_REFRESH);
            mStreamsMediator.refreshStream();
        }
    }

    private class RotationObserver implements DisplayAndroid.DisplayAndroidObserver {
        /**
         * If the device rotates, we dismiss the bottom sheet to avoid a bad interaction
         * between the XSurface client and the chrome bottom sheet.
         *
         * @param rotation One of Surface.ROTATION_* values.
         */
        @Override
        public void onRotationChanged(int rotation) {
            dismissBottomSheet();
        }
    }

    private FeedSurfaceRendererBridge mBridge;

    // How far the user has to scroll down in DP before attempting to load more content.
    private final int mLoadMoreTriggerScrollDistanceDp;

    private final Activity mActivity;
    private final Profile mProfile;
    private final ObserverList<ContentChangedListener> mContentChangedListeners =
            new ObserverList<>();
    private final int mStreamKind;
    private @ClosedReason int mClosedReason = ClosedReason.LEAVE_FEED;
    // Various helpers/controllers.
    private ShareHelperWrapper mShareHelper;
    private SnackbarManager mSnackManager;
    private WindowAndroid mWindowAndroid;
    private UnreadContentObserver mUnreadContentObserver;
    FeedContentFirstLoadWatcher mFeedContentFirstLoadWatcher;
    private Stream.StreamsMediator mStreamsMediator;
    // Snackbar (and post-Follow dialog) controller used exclusively for handling in-feed
    // post-Follow and post-Unfollow UX.
    WebFeedSnackbarController mWebFeedSnackbarController;
    InProgressWorkTracker mInProgressWorkTracker = new InProgressWorkTracker();

    // For loading more content.
    private int mAccumulatedDySinceLastLoadMore;
    private int mLoadMoreTriggerLookahead;
    private boolean mIsLoadingMoreContent;

    // Things attached on bind.
    private RestoreScrollObserver mRestoreScrollObserver = new RestoreScrollObserver();
    private RecyclerView.OnScrollListener mMainScrollListener;
    private FeedSliceViewTracker mSliceViewTracker;
    private ScrollReporter mScrollReporter;
    private final Map<String, Object> mHandlersMap;
    private RotationObserver mRotationObserver;
    private FeedReliabilityLoggingBridge mReliabilityLoggingBridge;
    private @Nullable FeedReliabilityLogger mReliabilityLogger;

    // Things valid only when bound.
    private @Nullable RecyclerView mRecyclerView;
    private @Nullable FeedListContentManager mContentManager;
    private @Nullable FeedSurfaceScope mSurfaceScope;
    private @Nullable HybridListRenderer mRenderer;
    private FeedScrollState mScrollStateToRestore;
    private int mHeaderCount;
    private long mLastFetchTimeMs;
    private ArrayList<SnackbarManager.SnackbarController> mSnackbarControllers = new ArrayList<>();

    // Placeholder view that simply takes up space.
    private FeedListContentManager.NativeViewContent mSpacerViewContent;

    // Bottomsheet.
    private final BottomSheetController mBottomSheetController;
    private BottomSheetContent mBottomSheetContent;
    private String mBottomSheetOriginatingSliceId;
    private View mLastFocusedView;
    private View mLastAccessibilityFocusedView;

    /**
     * Creates a new Feed Stream.
     *
     * @param activity {@link Activity} that this is bound to.
     * @param profile {@link Profile} that this is bound to.
     * @param snackbarManager {@link SnackbarManager} for showing snackbars.
     * @param bottomSheetController {@link BottomSheetController} for menus.
     * @param windowAndroid The {@link WindowAndroid} this is shown on.
     * @param shareDelegateSupplier The supplier for {@link ShareDelegate} for sharing actions.
     * @param streamKind Kind of stream data this feed stream serves.
     * @param actionDelegate Implements some Feed actions.
     * @param feedContentFirstLoadWatcher a listener for events about feed loading.
     * @param streamsMediator the mediator for multiple streams.
     * @param singleWebFeedParameters the parameters needed to create a single web feed.
     */
    public FeedStream(
            Activity activity,
            Profile profile,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid,
            Supplier<ShareDelegate> shareDelegateSupplier,
            int streamKind,
            FeedActionDelegate actionDelegate,
            FeedContentFirstLoadWatcher feedContentFirstLoadWatcher,
            StreamsMediator streamsMediator,
            SingleWebFeedParameters singleWebFeedParameters,
            FeedSurfaceRendererBridge.Factory feedSurfaceRendererBridgeFactory) {
        mReliabilityLoggingBridge = new FeedReliabilityLoggingBridge();
        mBridge =
                feedSurfaceRendererBridgeFactory.create(
                        new Renderer(),
                        mReliabilityLoggingBridge,
                        streamKind,
                        singleWebFeedParameters);
        mActivity = activity;
        mProfile = profile;
        mStreamKind = streamKind;
        mBottomSheetController = bottomSheetController;
        mShareHelper = new ShareHelperWrapper(windowAndroid, shareDelegateSupplier);
        mSnackManager = snackbarManager;
        mWindowAndroid = windowAndroid;
        mRotationObserver = new RotationObserver();
        mFeedContentFirstLoadWatcher = feedContentFirstLoadWatcher;
        mStreamsMediator = streamsMediator;
        WebFeedSnackbarController.FeedLauncher snackbarAction;
        if (mStreamKind == StreamKind.FOLLOWING) {
            snackbarAction =
                    () -> {
                        mStreamsMediator.refreshStream();
                    };
        } else {
            snackbarAction =
                    () -> {
                        mStreamsMediator.switchToStreamKind(StreamKind.FOLLOWING);
                    };
        }
        mWebFeedSnackbarController =
                new WebFeedSnackbarController(
                        activity,
                        snackbarAction,
                        windowAndroid.getModalDialogManager(),
                        snackbarManager);

        mHandlersMap = new HashMap<>();
        mHandlersMap.put(SurfaceActionsHandler.KEY, new FeedSurfaceActionsHandler(actionDelegate));
        mHandlersMap.put(FeedActionsHandler.KEY, new FeedActionsHandlerImpl());

        this.mLoadMoreTriggerScrollDistanceDp =
                FeedServiceBridge.getLoadMoreTriggerScrollDistanceDp();

        mScrollReporter = new ScrollReporter();

        mLoadMoreTriggerLookahead = FeedServiceBridge.getLoadMoreTriggerLookahead();

        mMainScrollListener =
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView v, int dx, int dy) {
                        super.onScrolled(v, dx, dy);
                        checkScrollingForLoadMore(dy);
                        mBridge.reportStreamScrollStart();
                        mScrollReporter.trackScroll(dx, dy);
                    }
                };

        // Only watch for unread content on the web feed, not for-you feed.
        // Sort options only available for web feed right now.
        if (streamKind == StreamKind.FOLLOWING) {
            mUnreadContentObserver = new UnreadContentObserver(/* isWebFeed= */ true);
        }
    }

    @Override
    public boolean supportsOptions() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED_SORT)
                && mStreamKind == StreamKind.FOLLOWING;
    }

    @Override
    public void destroy() {
        if (mUnreadContentObserver != null) {
            mUnreadContentObserver.destroy();
        }
        mBridge.destroy();
    }

    @Override
    public @StreamKind int getStreamKind() {
        return mStreamKind;
    }

    @Override
    public String getContentState() {
        return String.valueOf(mLastFetchTimeMs);
    }

    @Override
    public void bind(
            RecyclerView rootView,
            FeedListContentManager manager,
            FeedScrollState savedInstanceState,
            FeedSurfaceScope surfaceScope,
            HybridListRenderer renderer,
            @Nullable FeedReliabilityLogger reliabilityLogger,
            int headerCount) {
        mReliabilityLogger = reliabilityLogger;
        if (mReliabilityLogger != null) {
            mReliabilityLogger.onBindStream(getStreamType(), mBridge.surfaceId());
        }
        mReliabilityLoggingBridge.setLogger(mReliabilityLogger);

        mScrollStateToRestore = savedInstanceState;
        manager.setHandlers(mHandlersMap);
        mSliceViewTracker =
                new FeedSliceViewTracker(
                        rootView,
                        mActivity,
                        manager,
                        renderer.getListLayoutHelper(),
                        /* watchForUserInteractionReliabilityReport= */ (mReliabilityLogger != null
                                && mReliabilityLogger.getUserInteractionLogger() != null),
                        new FeedStream.ViewTrackerObserver());
        mSliceViewTracker.bind();

        rootView.addOnScrollListener(mMainScrollListener);
        rootView.getAdapter().registerAdapterDataObserver(mRestoreScrollObserver);
        mRecyclerView = rootView;
        mContentManager = manager;
        mSurfaceScope = surfaceScope;
        mRenderer = renderer;
        mHeaderCount = headerCount;
        if (mWindowAndroid.getDisplay() != null) {
            mWindowAndroid.getDisplay().addObserver(mRotationObserver);
        }
        mClosedReason = ClosedReason.LEAVE_FEED;

        mBridge.surfaceOpened();
    }

    @Override
    public void restoreSavedInstanceState(FeedScrollState scrollState) {
        if (!restoreScrollState(scrollState)) {
            mScrollStateToRestore = scrollState;
        }
    }

    // Dismiss any snackbars. Note that dismissal of snackbars sometimes triggers work in
    // xsurface.
    private void dismissSnackbars() {
        for (SnackbarManager.SnackbarController controller : mSnackbarControllers) {
            mSnackManager.dismissSnackbars(controller);
        }
    }

    @Override
    public void unbind(boolean shouldPlaceSpacer, boolean switchingStream) {
        // Find out the specific reason for unbinding the stream.
        if (switchingStream) {
            mClosedReason = ClosedReason.SWITCH_STREAM;
        } else if (ApplicationStatus.getStateForApplication()
                == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            mClosedReason = ClosedReason.SUSPEND_APP;
        }

        // This is the catch-all feed launch end event to ensure a complete flow is logged
        // even if we don't know a more specific reason for the stream unbinding.
        if (mReliabilityLogger != null) {
            mReliabilityLogger.onUnbindStream(mClosedReason);
        }

        dismissSnackbars();
        mSnackbarControllers.clear();
        mWebFeedSnackbarController.dismissSnackbars();

        mSliceViewTracker.destroy();
        mSliceViewTracker = null;
        mSurfaceScope = null;
        mAccumulatedDySinceLastLoadMore = 0;
        mScrollReporter.onUnbind();

        // Remove Feed content from the content manager. Add spacer if needed.
        ArrayList<FeedListContentManager.FeedContent> list = new ArrayList<>();
        if (shouldPlaceSpacer) {
            addSpacer(list);
        }
        updateContentsInPlace(list);

        // Dismiss bottomsheet if any is shown.
        dismissBottomSheet();

        // Clear handlers.
        mContentManager.setHandlers(new HashMap<>());
        mContentManager = null;

        mRecyclerView.removeOnScrollListener(mMainScrollListener);
        mRecyclerView.getAdapter().unregisterAdapterDataObserver(mRestoreScrollObserver);
        mRecyclerView = null;

        if (mWindowAndroid.getDisplay() != null) {
            mWindowAndroid.getDisplay().removeObserver(mRotationObserver);
        }

        mBridge.surfaceClosed();
    }

    @Override
    public void notifyNewHeaderCount(int newHeaderCount) {
        mHeaderCount = newHeaderCount;
    }

    @Override
    public void addOnContentChangedListener(ContentChangedListener listener) {
        mContentChangedListeners.addObserver(listener);
    }

    @Override
    public void removeOnContentChangedListener(ContentChangedListener listener) {
        mContentChangedListeners.removeObserver(listener);
    }

    @Override
    public void triggerRefresh(Callback<Boolean> callback) {
        dismissSnackbars();
        mInProgressWorkTracker.postTaskAfterWorkComplete(
                () -> {
                    if (mRenderer != null) {
                        mRenderer.onManualRefreshStarted();
                    }
                    mBridge.manualRefresh(callback);
                });
    }

    void dismissBottomSheet() {
        assert ThreadUtils.runningOnUiThread();
        if (mBottomSheetContent != null) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
        }
        mBottomSheetContent = null;
        mBottomSheetOriginatingSliceId = null;
    }

    @VisibleForTesting
    void checkScrollingForLoadMore(int dy) {
        if (mContentManager == null) return;

        mAccumulatedDySinceLastLoadMore += dy;
        if (mAccumulatedDySinceLastLoadMore < 0) {
            mAccumulatedDySinceLastLoadMore = 0;
        }
        if (mAccumulatedDySinceLastLoadMore
                < TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP,
                        mLoadMoreTriggerScrollDistanceDp,
                        mRecyclerView.getResources().getDisplayMetrics())) {
            return;
        }

        boolean canTrigger = maybeLoadMore();
        if (canTrigger) {
            mAccumulatedDySinceLastLoadMore = 0;
        }
    }

    @Override
    public ObservableSupplier<Boolean> hasUnreadContent() {
        return mUnreadContentObserver != null
                ? mUnreadContentObserver.mHasUnreadContent
                : Stream.super.hasUnreadContent();
    }

    @Override
    public long getLastFetchTimeMs() {
        return mBridge.getLastFetchTimeMs();
    }

    /**
     * Attempts to load more content if it can be triggered.
     *
     * <p>This method uses the default or Finch configured load more lookahead trigger.
     *
     * @return true if loading more content can be triggered.
     */
    boolean maybeLoadMore() {
        return maybeLoadMore(mLoadMoreTriggerLookahead);
    }

    /**
     * Attempts to load more content if it can be triggered.
     * @param lookaheadTrigger The threshold of off-screen cards below which the feed should attempt
     *         to load more content. I.e., if there are less than or equal to |lookaheadTrigger|
     *         cards left to show the user, then the feed should load more cards.
     * @return true if loading more content can be triggered.
     */
    private boolean maybeLoadMore(int lookaheadTrigger) {
        // Checks if we've been unbinded.
        if (mRecyclerView == null) {
            return false;
        }
        // Checks if loading more can be triggered.
        LayoutManager layoutManager = mRecyclerView.getLayoutManager();
        if (layoutManager == null) {
            return false;
        }

        // Check if the layout manager is initialized.
        int totalItemCount = layoutManager.getItemCount();
        if (totalItemCount < 0) {
            return false;
        }

        // When swapping feeds, the totalItemCount and lastVisibleItemPosition can temporarily fall
        // out of sync. Early exit on the pathological case where we think we're showing an item
        // beyond the end of the feed. This can occur if maybeLoadMore() is called during a feed
        // swap, after the feed items have been cleared, but before the view has finished updating
        // (which happens asynchronously).
        int lastVisibleItem = mRenderer.getListLayoutHelper().findLastVisibleItemPosition();
        if (totalItemCount < lastVisibleItem) {
            return false;
        }

        // No need to load more if there are more scrollable items than the trigger amount.
        int numItemsRemaining = totalItemCount - lastVisibleItem;
        if (numItemsRemaining > lookaheadTrigger) {
            return false;
        }

        // Starts to load more content if not yet.
        if (!mIsLoadingMoreContent) {
            mIsLoadingMoreContent = true;
            FeedUma.recordFeedLoadMoreTrigger(getStreamKind(), totalItemCount, numItemsRemaining);
            // The native loadMore() call may immediately result in onStreamUpdated(), which can
            // result in a crash if maybeLoadMore() is being called in response to certain events.
            // Use postTask to avoid this.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () ->
                            mBridge.loadMore(
                                    (Boolean success) -> {
                                        mIsLoadingMoreContent = false;
                                    }));
        }

        return true;
    }

    /**
     * Adds a spacer into the recycler view at the current position. If there is no spacer, we can't
     * scroll to the top, the scrolling  code won't go past the end of the content.
     */
    void addSpacer(List list) {
        if (mSpacerViewContent == null) {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
            FrameLayout spacerView = new FrameLayout(mActivity);
            mSpacerViewContent =
                    new FeedListContentManager.NativeViewContent(
                            getLateralPaddingsPx(), SPACER_KEY, spacerView);
            spacerView.setLayoutParams(
                    new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, displayMetrics.heightPixels));
        }
        list.add(mSpacerViewContent);
    }

    class Renderer implements FeedSurfaceRendererBridge.Renderer {
        @Override
        public void replaceDataStoreEntry(String key, byte[] data) {
            if (mSurfaceScope != null) mSurfaceScope.replaceDataStoreEntry(key, data);
        }

        @Override
        public void removeDataStoreEntry(String key) {
            if (mSurfaceScope != null) mSurfaceScope.removeDataStoreEntry(key);
        }

        /** Called when the stream update content is available. The content will get passed to UI */
        @Override
        public void onStreamUpdated(byte[] data) {
            // There should be no updates while the surface is closed. If the surface was recently
            // closed, just ignore these.
            if (mContentManager == null) return;
            FeedUiProto.StreamUpdate streamUpdate;
            try {
                streamUpdate = FeedUiProto.StreamUpdate.parseFrom(data);
            } catch (com.google.protobuf.InvalidProtocolBufferException e) {
                Log.wtf(TAG, "Unable to parse StreamUpdate proto data", e);
                mReliabilityLoggingBridge.onStreamUpdateError();
                return;
            }

            mLastFetchTimeMs = streamUpdate.getFetchTimeMs();

            FeedLoggingParameters loggingParameters =
                    new FeedLoggingParameters(streamUpdate.getLoggingParameters());

            // Invalidate the saved scroll state if the content in the feed has changed.
            // Don't do anything if mLastFetchTimeMs is unset.
            if (mScrollStateToRestore != null && mLastFetchTimeMs != 0) {
                if (!mScrollStateToRestore.feedContentState.equals(getContentState())) {
                    mScrollStateToRestore = null;
                }
            }

            // Update using shared states.
            for (FeedUiProto.SharedState state : streamUpdate.getNewSharedStatesList()) {
                mRenderer.update(state.getXsurfaceSharedState().toByteArray());
            }

            boolean foundNewContent = false;

            // Builds the new list containing:
            // * existing headers
            // * both new and existing contents
            ArrayList<FeedListContentManager.FeedContent> newContentList = new ArrayList<>();
            for (FeedUiProto.StreamUpdate.SliceUpdate sliceUpdate :
                    streamUpdate.getUpdatedSlicesList()) {
                if (sliceUpdate.hasSlice()) {
                    FeedListContentManager.FeedContent content =
                            createContentFromSlice(sliceUpdate.getSlice(), loggingParameters);
                    if (content != null) {
                        newContentList.add(content);
                        if (!content.isNativeView()) {
                            foundNewContent = true;
                        }
                    }
                } else {
                    String existingSliceId = sliceUpdate.getSliceId();
                    int position = mContentManager.findContentPositionByKey(existingSliceId);
                    if (position != -1) {
                        newContentList.add(mContentManager.getContent(position));
                        if (!mContentManager.getContent(position).isNativeView()) {
                            foundNewContent = true;
                        }
                    }
                    // We intentionially don't add the spacer back in. The spacer has a key
                    // SPACER_KEY, not a slice id.
                }
            }

            // Adds a special view at the end to provide the bottom margin. We can't do it with
            // the bottom margin added to the container because that would cause the bottom margin
            // be always visible since the beginning.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
                TextView bottomGapView = new TextView(mActivity);
                int bottomMargin =
                        mActivity
                                        .getResources()
                                        .getDimensionPixelSize(R.dimen.feed_containment_margin)
                                + mActivity
                                        .getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.feed_containment_bottom_card_padding);
                ViewGroup.LayoutParams bottomGapParams =
                        new ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, bottomMargin);
                bottomGapView.setLayoutParams(bottomGapParams);
                FeedListContentManager.NativeViewContent bottomGapViewContent =
                        new FeedListContentManager.NativeViewContent(
                                0,
                                "BottomGap" + bottomGapView.hashCode(),
                                bottomGapView,
                                /* isFullSpan= */ true);
                newContentList.add(bottomGapViewContent);
            }

            updateContentsInPlace(newContentList);
            mRecyclerView.post(mReliabilityLoggingBridge::onStreamUpdateFinished);

            // If we have new content, and the new content callback is set, then call it, and clear
            // the callback.
            if (mFeedContentFirstLoadWatcher != null && foundNewContent) {
                mFeedContentFirstLoadWatcher.nonNativeContentLoaded(mStreamKind);
                mFeedContentFirstLoadWatcher = null;
            }

            // If all of the cards fit on the screen, load more content. The view
            // may not be scrollable, preventing the user from otherwise triggering
            // load more.
            maybeLoadMore(/* lookaheadTrigger= */ 0);
        }
    }

    private FeedListContentManager.FeedContent createContentFromSlice(
            FeedUiProto.Slice slice, LoggingParameters loggingParameters) {
        String sliceId = slice.getSliceId();
        if (slice.hasXsurfaceSlice()) {
            return new FeedListContentManager.ExternalViewContent(
                    sliceId,
                    slice.getXsurfaceSlice().getXsurfaceFrame().toByteArray(),
                    loggingParameters);
        } else if (slice.hasLoadingSpinnerSlice()) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_LOADING_PLACEHOLDER)
                    && slice.getLoadingSpinnerSlice().getIsAtTop()) {
                return new FeedListContentManager.NativeViewContent(
                        getLateralPaddingsPx(),
                        LOADING_SPINNER_KEY,
                        R.layout.feed_placeholder_layout);
            }
            return new FeedListContentManager.NativeViewContent(
                    getLateralPaddingsPx(), LOADING_SPINNER_KEY, R.layout.feed_spinner);
        }
        assert slice.hasZeroStateSlice();
        if (mStreamKind == StreamKind.FOLLOWING) {
            return new FeedListContentManager.NativeViewContent(
                    getLateralPaddingsPx(), sliceId, R.layout.following_empty_state);
        }
        if (mStreamKind == StreamKind.SINGLE_WEB_FEED) {
            View creatorErrorCard;
            // TODO(crbug.com/40882611): Add offline error scenario.
            if (slice.getZeroStateSlice().getType()
                    == FeedUiProto.ZeroStateSlice.Type.NO_CARDS_AVAILABLE) {
                creatorErrorCard =
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.creator_content_unavailable_error,
                                        mRecyclerView,
                                        false);
            } else {
                mStreamsMediator.disableFollowButton();
                creatorErrorCard =
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.creator_general_error, mRecyclerView, false);
            }
            // TODO(crbug.com/40879463): Replace display height dependency with setting the
            // RecyclerView height to match_parent.
            DisplayMetrics displayMetrics = new DisplayMetrics();
            mActivity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
            MarginLayoutParams marginParams =
                    (MarginLayoutParams) creatorErrorCard.getLayoutParams();
            marginParams.setMargins(
                    0,
                    displayMetrics.heightPixels / 4,
                    0,
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.creator_error_margin_bottom));
            return new FeedListContentManager.NativeViewContent(
                    getLateralPaddingsPx(), sliceId, creatorErrorCard);
        }
        if (slice.getZeroStateSlice().getType() == FeedUiProto.ZeroStateSlice.Type.CANT_REFRESH) {
            return new FeedListContentManager.NativeViewContent(
                    getLateralPaddingsPx(), sliceId, R.layout.no_connection);
        }
        // TODO(crbug.com/40158714): Add new UI for NO_WEB_FEED_SUBSCRIPTIONS.
        assert slice.getZeroStateSlice().getType()
                        == FeedUiProto.ZeroStateSlice.Type.NO_CARDS_AVAILABLE
                || slice.getZeroStateSlice().getType()
                        == FeedUiProto.ZeroStateSlice.Type.NO_WEB_FEED_SUBSCRIPTIONS;
        return new FeedListContentManager.NativeViewContent(
                getLateralPaddingsPx(), sliceId, R.layout.no_content_v2);
    }

    private void updateContentsInPlace(
            ArrayList<FeedListContentManager.FeedContent> newContentList) {
        assert mHeaderCount <= mContentManager.getItemCount();
        if (mContentManager.replaceRange(
                mHeaderCount, mContentManager.getItemCount() - mHeaderCount, newContentList)) {
            notifyContentChange();
        }
    }

    private @StreamType int getStreamType() {
        switch (mStreamKind) {
            case StreamKind.FOR_YOU:
                return StreamType.FOR_YOU;
            case StreamKind.FOLLOWING:
                return StreamType.WEB_FEED;
            case StreamKind.SINGLE_WEB_FEED:
                return StreamType.SINGLE_WEB_FEED;
            case StreamKind.SUPERVISED_USER:
                return StreamType.SUPERVISED_USER_FEED;
            default:
                return StreamType.UNSPECIFIED;
        }
    }

    /**
     * Restores the scroll state serialized to |savedInstanceState|.
     * @return true if the scroll state was restored, or if the state could never be restored.
     * false if we need to wait until more items are added to the recycler view to make it
     * scrollable.
     */
    private boolean restoreScrollState(FeedScrollState state) {
        assert (mRecyclerView != null);
        if (state == null || state.lastPosition < 0 || state.position < 0) return true;

        // If too few items exist, defer scrolling until later.
        if (mContentManager.getItemCount() <= state.lastPosition) return false;
        // Don't try to resume scrolling to a refresh spinner.
        if (mContentManager.getContent(state.lastPosition).getKey().equals(LOADING_SPINNER_KEY)) {
            return false;
        }

        ListLayoutHelper layoutHelper = mRenderer.getListLayoutHelper();
        if (layoutHelper != null) {
            layoutHelper.scrollToPositionWithOffset(state.position, state.offset);
        }
        return true;
    }

    private void notifyContentChange() {
        for (ContentChangedListener listener : mContentChangedListeners) {
            listener.onContentChanged(
                    mContentManager != null ? mContentManager.getContentList() : null);
        }
    }

    @VisibleForTesting
    String getSliceIdFromView(View view) {
        View childOfRoot = findChildViewContainingDescendant(mRecyclerView, view);

        if (childOfRoot != null) {
            // View is a child of the recycler view, find slice using the index.
            int position = mRecyclerView.getChildAdapterPosition(childOfRoot);
            if (position >= 0 && position < mContentManager.getItemCount()) {
                return mContentManager.getContent(position).getKey();
            }
        } else if (mBottomSheetContent != null
                && findChildViewContainingDescendant(mBottomSheetContent.getContentView(), view)
                        != null) {
            // View is a child of the bottom sheet, return slice associated with the bottom
            // sheet.
            return mBottomSheetOriginatingSliceId;
        }
        return "";
    }

    /**
     * Returns the immediate child of parentView which contains descendantView.
     * If descendantView is not in parentView's view hierarchy, this returns null.
     * Note that the returned view may be descendantView, or descendantView.getParent(),
     * or descendantView.getParent().getParent(), etc...
     */
    private View findChildViewContainingDescendant(View parentView, View descendantView) {
        if (parentView == null || descendantView == null) return null;
        // Find the direct child of parentView which owns view.
        if (parentView == descendantView.getParent()) {
            return descendantView;
        } else {
            // One of the view's ancestors might be the child.
            ViewParent p = descendantView.getParent();
            while (true) {
                if (p == null) {
                    return null;
                }
                if (p.getParent() == parentView) {
                    if (p instanceof View) return (View) p;
                    return null;
                }
                p = p.getParent();
            }
        }
    }

    void setShareWrapperForTest(ShareHelperWrapper shareWrapper) {
        mShareHelper = shareWrapper;
    }

    /**
     * @return True if this feed has been bound.
     */
    public boolean getBoundStatusForTest() {
        return mContentManager != null;
    }

    RecyclerView.OnScrollListener getScrollListenerForTest() {
        return mMainScrollListener;
    }

    UnreadContentObserver getUnreadContentObserverForTest() {
        return mUnreadContentObserver;
    }

    InProgressWorkTracker getInProgressWorkTrackerForTesting() {
        return mInProgressWorkTracker;
    }

    // Scroll state can't be restored until enough items are added to the recycler view adapter.
    // Attempts to restore scroll state every time new items are added to the adapter.
    class RestoreScrollObserver extends RecyclerView.AdapterDataObserver {
        @Override
        public void onItemRangeInserted(int positionStart, int itemCount) {
            if (mScrollStateToRestore != null) {
                if (restoreScrollState(mScrollStateToRestore)) {
                    mScrollStateToRestore = null;
                }
            }
        }
    }

    private class ViewTrackerObserver implements FeedSliceViewTracker.Observer {
        @Override
        public void sliceVisible(String sliceId) {
            mBridge.reportSliceViewed(sliceId);
        }

        @Override
        public void reportContentSliceVisibleTime(long elapsedMs) {
            mBridge.reportContentSliceVisibleTimeForGoodVisits(elapsedMs);
        }

        @Override
        public void feedContentVisible() {
            mBridge.reportFeedViewed();
        }

        @Override
        public void reportViewFirstBarelyVisible(View view) {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onViewFirstVisible(view);
            }
        }

        @Override
        public void reportViewFirstRendered(View view) {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onViewFirstRendered(view);
            }
        }

        @Override
        public void reportLoadMoreIndicatorVisible() {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onPaginationIndicatorShown();
            }
        }

        @Override
        public void reportLoadMoreUserScrolledAwayFromIndicator() {
            if (mReliabilityLogger != null) {
                mReliabilityLogger.onPaginationUserScrolledAwayFromIndicator();
            }
        }
    }

    /**
     * Provides a wrapper around sharing methods.
     *
     * Makes it easier to test.
     */
    @VisibleForTesting
    static class ShareHelperWrapper {
        private WindowAndroid mWindowAndroid;
        private Supplier<ShareDelegate> mShareDelegateSupplier;

        public ShareHelperWrapper(
                WindowAndroid windowAndroid, Supplier<ShareDelegate> shareDelegateSupplier) {
            mWindowAndroid = windowAndroid;
            mShareDelegateSupplier = shareDelegateSupplier;
        }

        /**
         * Shares a url and title from Chrome to another app.
         * Brings up the share sheet.
         */
        public void share(String url, String title) {
            ShareParams params = new ShareParams.Builder(mWindowAndroid, title, url).build();
            mShareDelegateSupplier
                    .get()
                    .share(
                            params,
                            new ChromeShareExtras.Builder().build(),
                            ShareDelegate.ShareOrigin.FEED);
        }
    }

    // Ingests scroll events and reports scroll completion back to native.
    private class ScrollReporter extends ScrollTracker {
        @Override
        protected void onScrollEvent(int scrollAmount) {
            mBridge.reportStreamScrolled(scrollAmount);
        }
    }

    @VisibleForTesting
    static class UnreadContentObserver extends FeedServiceBridge.UnreadContentObserver {
        ObservableSupplierImpl<Boolean> mHasUnreadContent = new ObservableSupplierImpl<>();

        UnreadContentObserver(boolean isWebFeed) {
            super(isWebFeed);
            mHasUnreadContent.set(false);
        }

        @Override
        public void hasUnreadContentChanged(boolean hasUnreadContent) {
            mHasUnreadContent.set(hasUnreadContent);
        }
    }

    private int getLateralPaddingsPx() {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.ntp_header_lateral_paddings_v2);
    }
}
