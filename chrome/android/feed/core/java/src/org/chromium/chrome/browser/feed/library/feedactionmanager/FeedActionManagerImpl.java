// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionmanager;

import android.graphics.Rect;
import android.net.Uri;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.Consumer;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.library.api.common.MutationContext;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ScrollListener.ScrollState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Default implementation of {@link ActionManager} */
public class FeedActionManagerImpl implements ActionManager {
    private static final String TAG = "FeedActionManager";

    /**
     * Exposure is the fraction of card view visible in the viewport. Coverage is the fraction of
     * the viewport covered by a card view. A card must pass the exposure or coverage threshold to
     * be considered visible for a VIEW action.
     */
    private static final String VIEW_EXPOSURE_THRESHOLD = "view_exposure_threshold";
    static final double VIEW_EXPOSURE_THRESHOLD_DEFAULT = 0.5;
    private static final String VIEWPORT_COVERAGE_THRESHOLD = "viewport_coverage_threshold";
    static final double VIEWPORT_COVERAGE_THRESHOLD_DEFAULT = 0.5;

    /** Time on screen for a card to be considered visible for a VIEW action. */
    private static final String VIEW_DURATION_MS_THRESHOLD = "view_duration_threshold";
    static final long VIEW_DURATION_MS_THRESHOLD_DEFAULT = 500;

    private FeedSessionManager mFeedSessionManager;
    private final Store mStore;
    private final ThreadUtils mThreadUtils;
    private final TaskQueue mTaskQueue;
    private final MainThreadRunner mMainThreadRunner;
    private final ViewHandler mViewHandler;
    private final Clock mClock;
    private final BasicLoggingApi mBasicLoggingApi;

    private View mViewport;
    // Maps content to the ViewActionData used to decide if a View action needs to be recorded.
    // ViewActionData contains the payload that needs to be sent back, and a view duration that is
    // incremented each time the content is considered "visible" in the viewport.
    private final Map<String, ViewActionData> mContentData = new HashMap<>();
    // Time when we started tracking the content meeting the visibility conditions (normally this
    // happens when the viewport becomes stable).
    private long mTrackedStartTimeMs = -1L;

    private final double mViewExposureThreshold;
    private final double mViewportCoverageThreshold;
    private final long mViewDurationMsThreshold;

    private boolean mCanUploadClicksAndViewsWhenNoticePresent;

    FeedActionManagerImpl(Store store, ThreadUtils threadUtils, TaskQueue taskQueue,
            MainThreadRunner mainThreadRunner, ViewHandler viewHandler, Clock clock,
            BasicLoggingApi basicLoggingApi) {
        this.mStore = store;
        this.mThreadUtils = threadUtils;
        this.mTaskQueue = taskQueue;
        this.mMainThreadRunner = mainThreadRunner;
        this.mViewHandler = viewHandler;
        this.mClock = clock;
        this.mBasicLoggingApi = basicLoggingApi;

        mViewExposureThreshold = ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.REPORT_FEED_USER_ACTIONS, VIEW_EXPOSURE_THRESHOLD,
                VIEW_EXPOSURE_THRESHOLD_DEFAULT);
        mViewportCoverageThreshold = ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.REPORT_FEED_USER_ACTIONS, VIEWPORT_COVERAGE_THRESHOLD,
                VIEWPORT_COVERAGE_THRESHOLD_DEFAULT);
        mViewDurationMsThreshold = (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.REPORT_FEED_USER_ACTIONS, VIEW_DURATION_MS_THRESHOLD,
                (int) VIEW_DURATION_MS_THRESHOLD_DEFAULT);
    }

    public FeedActionManagerImpl(Store store, ThreadUtils threadUtils, TaskQueue taskQueue,
            MainThreadRunner mainThreadRunner, Clock clock, BasicLoggingApi basicLoggingApi) {
        this(store, threadUtils, taskQueue, mainThreadRunner, new ViewHandler(), clock,
                basicLoggingApi);
    }

    public void initialize(FeedSessionManager feedSessionManager) {
        mFeedSessionManager = feedSessionManager;
    }

    @Override
    public void setCanUploadClicksAndViewsWhenNoticeCardIsPresent(boolean canUploadClicksAndViews) {
        mCanUploadClicksAndViewsWhenNoticePresent = canUploadClicksAndViews;
    }

    @Override
    public void dismissLocal(List<String> contentIds,
            List<StreamDataOperation> streamDataOperations, @Nullable String sessionId) {
        executeStreamDataOperations(streamDataOperations, sessionId);
        // Store the dismissLocal actions
        mTaskQueue.execute(Task.DISMISS_LOCAL, TaskType.BACKGROUND, () -> {
            LocalActionMutation localActionMutation = mStore.editLocalActions();
            for (String contentId : contentIds) {
                localActionMutation.add(ActionType.DISMISS, contentId);
            }
            localActionMutation.commit();
        });
    }

    @Override
    public void dismiss(
            List<StreamDataOperation> streamDataOperations, @Nullable String sessionId) {
        executeStreamDataOperations(streamDataOperations, sessionId);
        mBasicLoggingApi.reportFeedInteraction();
    }

    @Override
    public void createAndUploadAction(
            String contentId, ActionPayload payload, ActionManager.UploadActionType type) {
        // Don't upload click actions when logging is disabled.
        if (!canUploadClicksAndViews() && type == ActionManager.UploadActionType.CLICK) {
            return;
        }

        mTaskQueue.execute(Task.CREATE_AND_UPLOAD, TaskType.BACKGROUND, () -> {
            HashSet<StreamUploadableAction> actionSet = new HashSet<>();
            long currentTime = TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis());
            actionSet.add(StreamUploadableAction.newBuilder()
                                  .setFeatureContentId(contentId)
                                  .setPayload(payload)
                                  .setTimestampSeconds(currentTime)
                                  .build());
            mFeedSessionManager.triggerUploadActions(actionSet);
        });
    }

    @Override
    public void createAndStoreAction(
            String contentId, ActionPayload payload, ActionManager.UploadActionType type) {
        // Don't store click actions when logging is disabled.
        if (!canUploadClicksAndViews() && type == ActionManager.UploadActionType.CLICK) {
            return;
        }

        mTaskQueue.execute(Task.CREATE_AND_STORE, TaskType.BACKGROUND, () -> {
            long currentTime = TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis());
            StreamUploadableAction action = StreamUploadableAction.newBuilder()
                                                    .setFeatureContentId(contentId)
                                                    .setPayload(payload)
                                                    .setTimestampSeconds(currentTime)
                                                    .build();
            mStore.editUploadableActions().upsert(action, action.getFeatureContentId()).commit();
        });
    }

    @Override
    public void uploadAllActionsAndUpdateUrl(
            String url, String consistencyTokenQueryParamName, Consumer<String> consumer) {
        mTaskQueue.execute(Task.UPLOAD_ALL_ACTIONS_FOR_URL, TaskType.BACKGROUND, () -> {
            // TODO: figure out spinner and/or timeout conditions
            mFeedSessionManager.fetchActionsAndUpload(result -> {
                mMainThreadRunner.execute("Open url", () -> {
                    if (result.isSuccessful()) {
                        consumer.accept(updateParam(url, consistencyTokenQueryParamName,
                                result.getValue().toByteArray()));
                    } else {
                        consumer.accept(url);
                    }
                });
            });
        });
    }

    static String updateParam(String url, String consistencyTokenQueryParamName, byte[] value) {
        Uri.Builder uriBuilder = Uri.parse(url).buildUpon();
        uriBuilder.appendQueryParameter(consistencyTokenQueryParamName,
                Base64.encodeToString(value, Base64.URL_SAFE | Base64.NO_WRAP));
        return uriBuilder.build().toString();
    }

    private void executeStreamDataOperations(
            List<StreamDataOperation> streamDataOperations, @Nullable String sessionId) {
        mThreadUtils.checkMainThread();

        MutationContext.Builder mutationContextBuilder =
                new MutationContext.Builder().setUserInitiated(true);
        if (sessionId != null) {
            mutationContextBuilder.setRequestingSessionId(sessionId);
        }
        mFeedSessionManager.getUpdateConsumer(mutationContextBuilder.build())
                .accept(Result.success(Model.of(streamDataOperations)));
    }

    @Override
    public void setViewport(@Nullable View viewport) {
        mThreadUtils.checkMainThread();
        mViewport = viewport;
    }

    @Override
    public void onViewVisible(View view, String contentId, ActionPayload actionPayload) {
        mThreadUtils.checkMainThread();
        mViewHandler.setContentId(view, contentId);
        if (!mContentData.containsKey(contentId)) {
            mContentData.put(
                    contentId, ViewActionData.createUntrackedWithZeroDuration(actionPayload));
        }

        // Viewport may already be stable, so make sure new visible content is tracked if necessary.
        if (mTrackedStartTimeMs >= 0) maybeTrack(view);
    }

    @Override
    public void onViewHidden(View view, String contentId) {
        mThreadUtils.checkMainThread();
        // Viewport may already be stable, so make sure hidden content is not tracked.
        if (mTrackedStartTimeMs >= 0 && mContentData.containsKey(contentId)) {
            mContentData.get(contentId).tracked = false;
        }

        /**
         * Content is not removed from mContentData as we report view actions across multiple
         * content appearances, until we decide to send a VIEW action to the server.
         */

        mViewHandler.setContentId(view, null);
    }

    @Override
    public void storeViewActions(Runnable doneCallback) {
        mThreadUtils.checkMainThread();
        reportViewActions(doneCallback);
    }

    @Override
    public ScrollListener getScrollListener() {
        return new ScrollListener() {
            @Override
            public void onScrollStateChanged(int state) {
                switch (state) {
                    case ScrollState.DRAGGING:
                        FeedActionManagerImpl.this.onScrollStart();
                        break;
                    case ScrollState.IDLE:
                        FeedActionManagerImpl.this.onScrollEnd();
                        break;
                }
            }

            @Override
            public void onScrolled(int dx, int dy) {}
        };
    }

    @Override
    public void onAnimationFinished() {
        restartStableViewport();
    }

    @Override
    public void onLayoutChange() {
        restartStableViewport();
    }

    @Override
    public void onShow() {
        startStableViewport();
    }

    @Override
    public void onHide() {
        stopStableViewport();
        reportViewActions(() -> {});
        mContentData.clear();
    }

    /**
     * Signal an {@link
     * org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver#onScrollStateChanged}
     * that is {@link androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_DRAGGING}
     */
    void onScrollStart() {
        stopStableViewport();
    }

    /**
     * Signal an {@link
     * org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll.ScrollObserver#onScrollStateChanged}
     * that is {@link androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_IDLE}.
     * Should be called after view actions are triggered so that new tracked views may be monitored.
     */
    void onScrollEnd() {
        startStableViewport();
    }

    private void startStableViewport() {
        mThreadUtils.checkMainThread();
        // No viewport, or viewport already stable.
        if (mViewport == null || mTrackedStartTimeMs > 0) return;

        mTrackedStartTimeMs = mClock.currentTimeMillis();
        traverseViewHierarchy(mViewport);
    }

    private void stopStableViewport() {
        mThreadUtils.checkMainThread();
        // Viewport not stable.
        if (mTrackedStartTimeMs < 0) return;

        long durationMs = mClock.currentTimeMillis() - mTrackedStartTimeMs;
        for (Map.Entry<String, ViewActionData> entry : mContentData.entrySet()) {
            if (entry.getValue().tracked) {
                entry.getValue().durationMs += durationMs;
            }
            entry.getValue().tracked = false;
        }

        mTrackedStartTimeMs = -1L;
    }

    private void restartStableViewport() {
        stopStableViewport();
        startStableViewport();
    }

    private void traverseViewHierarchy(View view) {
        maybeTrack(view);
        for (int index = 0; index < mViewHandler.getChildCount(view); index++) {
            traverseViewHierarchy(mViewHandler.getChildAt(view, index));
        }
    }

    private void maybeTrack(View view) {
        String contentId = mViewHandler.getContentId(view);
        if (contentId != null && mViewport != null && isVisibilityConditionMet(view, mViewport)) {
            if (mContentData.containsKey(contentId)) {
                mContentData.get(contentId).tracked = true;
            }
        }
    }

    private void reportViewActions(Runnable doneCallback) {
        // Don't report when logging is disabled.
        if (!canUploadClicksAndViews()) {
            return;
        }

        Set<StreamUploadableAction> actions = new HashSet<>();
        if (FeedFeatures.isReportingUserActions()) {
            Iterator<Map.Entry<String, ViewActionData>> entryIterator =
                    mContentData.entrySet().iterator();

            while (entryIterator.hasNext()) {
                Map.Entry<String, ViewActionData> entry = entryIterator.next();
                String contentId = entry.getKey();
                ViewActionData viewActionData = entry.getValue();

                long currentTimeS = TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis());
                if (isDurationConditionMet(viewActionData.durationMs)) {
                    actions.add(StreamUploadableAction.newBuilder()
                                        .setFeatureContentId(contentId)
                                        .setPayload(viewActionData.actionPayload)
                                        .setDurationMs(viewActionData.durationMs)
                                        .setTimestampSeconds(currentTimeS)
                                        .build());
                    // Stop tracking this particular content as we're already uploading a view
                    // action for it.
                    entryIterator.remove();
                }
            }
        }
        if (actions.isEmpty()) {
            doneCallback.run();
        } else {
            mTaskQueue.execute(Task.STORE_VIEW_ACTIONS, TaskType.IMMEDIATE, () -> {
                UploadableActionMutation actionMutation = mStore.editUploadableActions();
                for (StreamUploadableAction action : actions) {
                    actionMutation.upsert(action, action.getFeatureContentId());
                }
                CommitResult commitResult = actionMutation.commit();
                if (commitResult != CommitResult.SUCCESS) {
                    Log.d(TAG, "Upserting view actions failed.");
                }
                mMainThreadRunner.execute(
                        "Store view actions callback", () -> { doneCallback.run(); });
            });
        }
    }

    private boolean isVisibilityConditionMet(View view, View viewport) {
        double exposure = getViewExposure(view, viewport);
        double coverage = getViewportCoverage(view, viewport);
        return exposure >= mViewExposureThreshold || coverage >= mViewportCoverageThreshold;
    }

    private double getViewExposure(View view, View viewport) {
        Rect viewRect = mViewHandler.getRectOnScreen(view);
        Rect viewportRect = mViewHandler.getRectOnScreen(viewport);

        double viewArea = viewRect.height() * viewRect.width();
        if (viewportRect.intersect(viewRect)) { // viewportRect becomes intersection.
            double visibleArea = viewportRect.height() * viewportRect.width();
            return visibleArea / viewArea;
        }
        return 0;
    }

    private double getViewportCoverage(View view, View viewport) {
        Rect viewRect = mViewHandler.getRectOnScreen(view);
        Rect viewportRect = mViewHandler.getRectOnScreen(viewport);

        double viewportArea = viewportRect.height() * viewportRect.width();
        if (viewportRect.intersect(viewRect)) { // viewPortRect becomes intersection.
            double visibleArea = viewportRect.height() * viewportRect.width();
            return visibleArea / viewportArea;
        }
        return 0;
    }

    private boolean isDurationConditionMet(long durationMs) {
        return durationMs >= mViewDurationMsThreshold;
    }

    // Handles logic specific to Views needed for VIEW action tracking.
    static class ViewHandler {
        public final void setContentId(View view, @Nullable String contentId) {
            view.setTag(R.id.tag_view_actions_content_id, contentId);
        }

        // Retrieve the content ID that may have been previously set on this View.
        @Nullable
        public final String getContentId(View view) {
            return (String) view.getTag(R.id.tag_view_actions_content_id);
        }

        public int getChildCount(View view) {
            return view instanceof ViewGroup ? ((ViewGroup) view).getChildCount() : 0;
        }

        @Nullable
        public View getChildAt(View view, int index) {
            return view instanceof ViewGroup ? ((ViewGroup) view).getChildAt(index) : null;
        }

        // Get the Rect that this View is occupying on screen.
        public Rect getRectOnScreen(View view) {
            int[] viewLocation = new int[2];
            view.getLocationOnScreen(viewLocation);
            return new Rect(viewLocation[0], viewLocation[1], viewLocation[0] + view.getWidth(),
                    viewLocation[1] + view.getHeight());
        }
    }

    private static class ViewActionData {
        public final ActionPayload actionPayload;
        public long durationMs;
        public boolean tracked;

        private ViewActionData(ActionPayload actionPayload, long durationMs, boolean tracked) {
            this.actionPayload = actionPayload;
            this.durationMs = durationMs;
            this.tracked = tracked;
        }

        public static ViewActionData createUntrackedWithZeroDuration(ActionPayload actionPayload) {
            return new ViewActionData(actionPayload, 0L, false);
        }
    }

    private boolean canUploadClicksAndViews() {
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)) {
            return true;
        }
        boolean wasNoticePresent = UserPrefs.get(Profile.getLastUsedRegularProfile())
                                           .getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD);
        return mCanUploadClicksAndViewsWhenNoticePresent || !wasNoticePresent;
    }
}
