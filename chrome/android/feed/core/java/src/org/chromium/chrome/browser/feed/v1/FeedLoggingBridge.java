// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.FeedUma;
import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.ElementLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.ScrollType;
import org.chromium.chrome.browser.feed.library.api.host.logging.SessionEvent;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.logging.ZeroStateShowReason;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link BasicLoggingApi} that log actions performed on the Feed,
 * and provides access to native implementation of feed logging.
 */
@JNINamespace("feed")
public class FeedLoggingBridge implements BasicLoggingApi {
    private static final String TAG = "FeedLoggingBridge";
    private static final int SHOWN_INDEX_THRESHOLD = 2;

    private long mNativeFeedLoggingBridge;
    private static final int MIN_SCROLL_THRESHOLD_DP = 160; // one inch.
    private static final long VISIT_TIME_THRESHOLD = 1000 * 60 * 5; // 5 min in ms.
    private boolean mEngagedReported;
    private boolean mEngagedSimpleReported;
    private boolean mScrolledReported;
    private long mVisitStartTime;
    private Clock mClock;
    private boolean mHasReachedShownIndexesThreshold;

    // This enum is used for UMA, don't move or reassign these numbers.
    public @interface FeedEngagementType {
        int FEED_ENGAGED = 0;
        int FEED_ENGAGED_SIMPLE = 1;
        int FEED_INTERACTED = 2;
        int DEPRECATED_FEED_SCROLLED = 3;
        int FEED_SCROLLED = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Creates a {@link FeedLoggingBridge} for accessing native feed logging
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedLoggingBridge(Profile profile, Clock clock) {
        mNativeFeedLoggingBridge = FeedLoggingBridgeJni.get().init(FeedLoggingBridge.this, profile);
        mEngagedReported = false;
        mEngagedSimpleReported = false;
        mScrolledReported = false;
        mClock = clock;
        mVisitStartTime = 0;
        // Set the initial value to true when the conditional logging feature is disabled because
        // the threshold isn't needed in that case.
        mHasReachedShownIndexesThreshold = !ChromeFeatureList.isEnabled(
                ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD);
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().destroy(mNativeFeedLoggingBridge, FeedLoggingBridge.this);
        mNativeFeedLoggingBridge = 0;
    }

    @Override
    public void onContentViewed(ContentLoggingData data) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        onShownSlice(data.getPositionInStream());

        FeedLoggingBridgeJni.get().onContentViewed(mNativeFeedLoggingBridge, FeedLoggingBridge.this,
                data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()), data.getScore(),
                data.isAvailableOffline());
    }

    private void onShownSlice(int index) {
        maybeUpdateNoticeCardViewsCount(index);

        if (mHasReachedShownIndexesThreshold) {
            return;
        }
        if (!lastRefreshWasSignedIn()) {
            return;
        }
        if (index + 1 >= SHOWN_INDEX_THRESHOLD) {
            mHasReachedShownIndexesThreshold = true;
            UserPrefs.get(getProfile())
                    .setBoolean(Pref.HAS_REACHED_CLICK_AND_VIEW_ACTIONS_UPLOAD_CONDITIONS, true);
        }
    }

    private boolean lastRefreshWasSignedIn() {
        return UserPrefs.get(getProfile()).getBoolean(Pref.LAST_REFRESH_WAS_SIGNED_IN);
    }

    private void maybeUpdateNoticeCardViewsCount(int index) {
        if (!shouldUpdateNoticeCardClicksAndViewsCounters()) {
            return;
        }

        if (!hasNoticeCard()) {
            return;
        }

        if (index != getNoticeCardIndex()) {
            return;
        }

        // Increment the stored notice card views count by 1.
        int count = UserPrefs.get(getProfile()).getInteger(Pref.NOTICE_CARD_VIEWS_COUNT);
        UserPrefs.get(getProfile()).setInteger(Pref.NOTICE_CARD_VIEWS_COUNT, count + 1);
    }

    private void maybeUpdateNoticeCardClicksCount(int index) {
        if (!shouldUpdateNoticeCardClicksAndViewsCounters()) {
            return;
        }

        if (!hasNoticeCard()) {
            return;
        }

        if (index != getNoticeCardIndex()) {
            return;
        }

        // Increment the stored notice card clicks count by 1.
        int count = UserPrefs.get(getProfile()).getInteger(Pref.NOTICE_CARD_CLICKS_COUNT);
        UserPrefs.get(getProfile()).setInteger(Pref.NOTICE_CARD_CLICKS_COUNT, count + 1);
    }

    private boolean hasNoticeCard() {
        return UserPrefs.get(getProfile()).getBoolean(Pref.LAST_FETCH_HAD_NOTICE_CARD);
    }

    private static int getNoticeCardIndex() {
        // Infer that the notice card is at the 2nd position when the feature related to putting the
        // notice card at the second position is enabled.
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD)) {
            return 1;
        }
        return 0;
    }

    private static boolean shouldUpdateNoticeCardClicksAndViewsCounters() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS);
    }

    private static Profile getProfile() {
        return Profile.getLastUsedRegularProfile();
    }

    @Override
    public void onContentDismissed(ContentLoggingData data, boolean wasCommitted) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onContentDismissed(mNativeFeedLoggingBridge,
                FeedLoggingBridge.this, data.getPositionInStream(), data.getRepresentationUri(),
                wasCommitted);
    }

    @Override
    public void onContentSwiped(ContentLoggingData data) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onContentSwiped(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this);
    }

    @Override
    public void onContentClicked(ContentLoggingData data) {
        // Records content's clicks in onClientAction. When a user clicks on content, Feed libraries
        // will call both onClientAction and onContentClicked, and onClientAction will receive
        // ActionType.OPEN_URL in this case. so to avoid double counts, we records content's clicks
        // in onClientAction.
    }

    @Override
    public void onClientAction(ContentLoggingData data, @ActionType int actionType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        recordUserAction(actionType);
        maybeUpdateNoticeCardClicksCount(data.getPositionInStream());
        FeedLoggingBridgeJni.get().onClientAction(mNativeFeedLoggingBridge, FeedLoggingBridge.this,
                feedActionToWindowOpenDisposition(actionType), data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()), data.getScore(),
                data.isAvailableOffline());
    }

    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onContentContextMenuOpened(mNativeFeedLoggingBridge,
                FeedLoggingBridge.this, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getPublishedTimeSeconds()), data.getScore());
    }

    @Override
    public void onMoreButtonViewed(int position) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onMoreButtonViewed(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, position);
    }

    @Override
    public void onMoreButtonClicked(int position) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onMoreButtonClicked(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, position);
    }

    @Override
    public void onNotInterestedIn(int interestType, ContentLoggingData data, boolean wasCommitted) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        // TODO(crbug.com/935602): Fail to compile when new values are added to NotInterestedInData.
        if (interestType == FeedActionProto.NotInterestedInData.RecordedInterestType.TOPIC_VALUE) {
            FeedLoggingBridgeJni.get().onNotInterestedInTopic(mNativeFeedLoggingBridge,
                    FeedLoggingBridge.this, data.getPositionInStream(), wasCommitted);
        } else if (interestType
                == FeedActionProto.NotInterestedInData.RecordedInterestType.SOURCE_VALUE) {
            FeedLoggingBridgeJni.get().onNotInterestedInSource(mNativeFeedLoggingBridge,
                    FeedLoggingBridge.this, data.getPositionInStream(), wasCommitted);
        }
    }

    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onOpenedWithContent(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, timeToPopulateMs, contentCount);
    }

    @Override
    public void onOpenedWithNoImmediateContent() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onOpenedWithNoImmediateContent(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this);
    }

    @Override
    public void onOpenedWithNoContent() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onOpenedWithNoContent(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this);
    }

    @Override
    public void onSpinnerStarted(@SpinnerType int spinnerType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onSpinnerStarted(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, spinnerType);
    }

    @Override
    public void onSpinnerFinished(int timeShownMs, @SpinnerType int spinnerType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onSpinnerFinished(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, timeShownMs, spinnerType);
    }

    @Override
    public void onSpinnerDestroyedWithoutCompleting(int timeShownMs, @SpinnerType int spinnerType) {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().onSpinnerDestroyedWithoutCompleting(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, timeShownMs, spinnerType);
    }

    @Override
    public void onPietFrameRenderingEvent(List<Integer> pietErrorCodes) {
        if (mNativeFeedLoggingBridge == 0) return;
        int[] pietErrorCodesArray = new int[pietErrorCodes.size()];
        for (int i = 0; i < pietErrorCodes.size(); ++i) {
            pietErrorCodesArray[i] = pietErrorCodes.get(i);
        }
        FeedLoggingBridgeJni.get().onPietFrameRenderingEvent(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, pietErrorCodesArray);
    }

    @Override
    public void onVisualElementClicked(ElementLoggingData data, int elementType) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onVisualElementClicked(mNativeFeedLoggingBridge,
                FeedLoggingBridge.this, elementType, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()));
    }

    @Override
    public void onVisualElementViewed(ElementLoggingData data, int elementType) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onVisualElementViewed(mNativeFeedLoggingBridge,
                FeedLoggingBridge.this, elementType, data.getPositionInStream(),
                TimeUnit.SECONDS.toMillis(data.getTimeContentBecameAvailable()));
    }

    @Override
    public void onInternalError(@InternalFeedError int internalError) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onInternalError(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, internalError);
    }

    @Override
    public void onTokenCompleted(boolean wasSynthetic, int contentCount, int tokenCount) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onTokenCompleted(mNativeFeedLoggingBridge,
                FeedLoggingBridge.this, wasSynthetic, contentCount, tokenCount);
    }

    @Override
    public void onTokenFailedToComplete(boolean wasSynthetic, int failureCount) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onTokenFailedToComplete(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, wasSynthetic, failureCount);
    }

    @Override
    public void onServerRequest(@RequestReason int requestReason) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onServerRequest(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, requestReason);
    }

    @Override
    public void onZeroStateShown(@ZeroStateShowReason int zeroStateShowReason) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onZeroStateShown(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, zeroStateShowReason);
    }

    @Override
    public void onZeroStateRefreshCompleted(int newContentCount, int newTokenCount) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onZeroStateRefreshCompleted(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, newContentCount, newTokenCount);
    }

    @Override
    public void onInitialSessionEvent(
            @SessionEvent int sessionEvent, int timeFromRegisteringMs, int sessionCount) {
        // TODO(https://crbug.com/924739): Implementation.
    }

    @Override
    public void onScroll(@ScrollType int scrollType, int distanceScrolled) {
        reportScrollActivity(distanceScrolled);
    }

    @Override
    public void onTaskFinished(@Task int task, int delayTime, int taskTime) {
        if (mNativeFeedLoggingBridge == 0) return;
        FeedLoggingBridgeJni.get().onTaskFinished(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this, task, delayTime, taskTime);
    }

    /**
     * Reports how long a user spends on the page.
     *
     * @param visitTimeMs Time spent reading the page.
     * @param returnToNtp User backed to NTP after visit the page.
     */
    public void onContentTargetVisited(long visitTimeMs, boolean returnToNtp) {
        // We cannot assume that the|mNativeFeedLoggingBridge| is always available like other
        // methods. This method is called by objects not controlled by Feed lifetimes, and destroy()
        // may have already been called if Feed is disabled by policy.
        if (mNativeFeedLoggingBridge != 0) {
            FeedLoggingBridgeJni.get().onContentTargetVisited(
                    mNativeFeedLoggingBridge, FeedLoggingBridge.this, visitTimeMs, returnToNtp);
        }
    }

    private int feedActionToWindowOpenDisposition(@ActionType int actionType) {
        switch (actionType) {
            case ActionType.OPEN_URL:
                return WindowOpenDisposition.CURRENT_TAB;
            case ActionType.OPEN_URL_INCOGNITO:
                return WindowOpenDisposition.OFF_THE_RECORD;
            case ActionType.OPEN_URL_NEW_TAB:
                return WindowOpenDisposition.NEW_BACKGROUND_TAB;
            case ActionType.OPEN_URL_NEW_WINDOW:
                return WindowOpenDisposition.NEW_WINDOW;
            case ActionType.DOWNLOAD:
                return WindowOpenDisposition.SAVE_TO_DISK;
            case ActionType.LEARN_MORE:
            case ActionType.MANAGE_INTERESTS:
            case ActionType.BLOCK_CONTENT:
            case ActionType.UNKNOWN:
            default:
                return WindowOpenDisposition.UNKNOWN;
        }
    }

    private void recordUserAction(@ActionType int actionType) {
        switch (actionType) {
            case ActionType.BLOCK_CONTENT:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_BLOCK_CONTENT);
                break;
            case ActionType.OPEN_URL:
            case ActionType.OPEN_URL_INCOGNITO:
            case ActionType.OPEN_URL_NEW_TAB:
            case ActionType.OPEN_URL_NEW_WINDOW:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);
                break;
            case ActionType.LEARN_MORE:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_LEARN_MORE);
                FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_LEARN_MORE);
                break;
            case ActionType.MANAGE_INTERESTS:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_MANAGE_INTERESTS);
                FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MANAGE_INTERESTS);
                break;
            case ActionType.DOWNLOAD:
            case ActionType.UNKNOWN:
            default:
                break;
        }
    }

    private void reportScrolledAfterOpen() {
        // Bridge could have been destroyed for policy when this is called.
        // See https://crbug.com/901414.
        if (mNativeFeedLoggingBridge == 0) return;

        FeedLoggingBridgeJni.get().reportScrolledAfterOpen(
                mNativeFeedLoggingBridge, FeedLoggingBridge.this);
    }

    /**
     * One-shot reporter that records the first time the user scrolls in the {@link Stream}.
     */
    public static class ScrollEventReporter implements ScrollListener {
        private final FeedLoggingBridge mLoggingBridge;
        private boolean mFired;

        public ScrollEventReporter(@NonNull FeedLoggingBridge loggingBridge) {
            super();
            mLoggingBridge = loggingBridge;
        }

        @Override
        public void onScrollStateChanged(@ScrollState int state) {
            if (mFired) return;
            if (state != ScrollState.DRAGGING) return;

            mLoggingBridge.reportScrolledAfterOpen();
            mFired = true;
        }

        @Override
        public void onScrolled(int dx, int dy) {}
    }

    @Override
    public void reportScrollActivity(int scrollAmount) {
        // Report each engagement type if we have not already reported it this session.
        recordEngagement(scrollAmount, false /* no user interaction */);

        if (!mScrolledReported) {
            recordEngagementType(FeedEngagementType.FEED_SCROLLED);
            mScrolledReported = true;
        }
    }

    @Override
    public void reportFeedInteraction() {
        recordEngagement(0, true /* user interacted */);
        recordEngagementType(FeedEngagementType.FEED_INTERACTED);
    }

    private int convertPixelsToDP(int pixels) {
        return (int) (pixels
                / ContextUtils.getApplicationContext().getResources().getDisplayMetrics().density);
    }

    /** Records FEED_ENGAGED and FEED_ENGAGED_SIMPLE if appropriate. */
    private void recordEngagement(int scrollAmount, boolean interacted) {
        // Convert scrollAmount from pixels to DP.
        scrollAmount = Math.abs(scrollAmount);
        int dpScrolled = 0;
        if (scrollAmount > 0) {
            dpScrolled = convertPixelsToDP(scrollAmount);
        }

        resetVisitIfNeeded();

        // Report the user as engaged-simple if they have scrolled any amount or interacted
        // with the card, and we have not already reported it for this chrome run.
        if (!mEngagedSimpleReported && (scrollAmount > 0 || interacted)) {
            recordEngagementType(FeedEngagementType.FEED_ENGAGED_SIMPLE);
            mEngagedSimpleReported = true;
        }

        // Report the user as engaged if they have scrolled more than the threshold or interacted
        // with the card, and we have not already reported it this chrome run.
        if (!mEngagedReported && (dpScrolled > MIN_SCROLL_THRESHOLD_DP || interacted)) {
            recordEngagementType(FeedEngagementType.FEED_ENGAGED);
            mEngagedReported = true;
        }
    }

    /**
     * If enough time has elapsed since we the user started a visit, start a new visit.
     *  If the user interacts within the window, reset the window.
     */
    private void resetVisitIfNeeded() {
        if (mClock.elapsedRealtime() - mVisitStartTime > VISIT_TIME_THRESHOLD) {
            mEngagedReported = false;
            mEngagedSimpleReported = false;
            mScrolledReported = false;
        }
        // Reset the last active time for session measurement.
        mVisitStartTime = mClock.elapsedRealtime();
    }

    private void recordEngagementType(int engagementType) {
        RecordHistogram.recordEnumeratedHistogram("ContentSuggestions.Feed.EngagementType",
                engagementType, FeedEngagementType.NUM_ENTRIES);
    }

    @NativeMethods
    interface Natives {
        long init(FeedLoggingBridge caller, Profile profile);
        void destroy(long nativeFeedLoggingBridge, FeedLoggingBridge caller);
        void onContentViewed(long nativeFeedLoggingBridge, FeedLoggingBridge caller, int position,
                long publishedTimeMs, long timeContentBecameAvailableMs, float score,
                boolean isAvailableOffline);
        void onContentDismissed(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int position, String uri, boolean wasCommitted);
        void onContentSwiped(long nativeFeedLoggingBridge, FeedLoggingBridge caller);
        void onClientAction(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int windowOpenDisposition, int position, long publishedTimeMs, float score,
                boolean isAvailableOffline);
        void onContentContextMenuOpened(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int position, long publishedTimeMs, float score);
        void onMoreButtonViewed(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int position);
        void onMoreButtonClicked(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int position);
        void onNotInterestedInSource(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int position, boolean wasCommitted);
        void onNotInterestedInTopic(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int position, boolean wasCommitted);
        void onOpenedWithContent(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int timeToPopulateMs, int contentCount);
        void onOpenedWithNoImmediateContent(long nativeFeedLoggingBridge, FeedLoggingBridge caller);
        void onOpenedWithNoContent(long nativeFeedLoggingBridge, FeedLoggingBridge caller);
        void onSpinnerStarted(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int spinnerType);
        void onSpinnerFinished(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                long spinnerShownTimeMs, int spinnerType);
        void onSpinnerDestroyedWithoutCompleting(long nativeFeedLoggingBridge,
                FeedLoggingBridge caller, long spinnerShownTimeMs, int spinnerType);
        void onPietFrameRenderingEvent(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int[] pietErrorCodes);
        void onVisualElementClicked(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int elementType, int position, long timeContentBecameAvailableMs);
        void onVisualElementViewed(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int elementType, int position, long timeContentBecameAvailableMs);
        void onInternalError(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int internalError);
        void onTokenCompleted(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                boolean wasSynthetic, int contentCount, int tokenCount);
        void onTokenFailedToComplete(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                boolean wasSynthetic, int failureCount);
        void onServerRequest(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int requestReason);
        void onZeroStateShown(
                long nativeFeedLoggingBridge, FeedLoggingBridge caller, int zeroStateShowReason);
        void onZeroStateRefreshCompleted(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                int newContentCount, int newTokenCount);
        void onTaskFinished(long nativeFeedLoggingBridge, FeedLoggingBridge caller, int task,
                int delayTimeMs, int taskTimeMs);
        void onContentTargetVisited(long nativeFeedLoggingBridge, FeedLoggingBridge caller,
                long visitTimeMs, boolean returnToNtp);
        void reportScrolledAfterOpen(long nativeFeedLoggingBridge, FeedLoggingBridge caller);
    }
}
