// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.PluralsRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Message UI specific implementation of {@link DownloadMessageUiController}. */
public class DownloadMessageUiControllerImpl implements DownloadMessageUiController {
    private static final long DURATION_SHOW_RESULT_IN_MS = 6000;

    // The description can be an extremely long data url, whose length can cause a low memory
    // error when applied to a text view. https://crbug.com/1250423
    private static final int MAX_DESCRIPTION_LENGTH = 200;

    // Keep this in sync with the DownloadInfoBar.ShownState enum in enums.xml.
    @IntDef({
        UmaInfobarShown.ANY_STATE,
        UmaInfobarShown.ACCELERATED,
        UmaInfobarShown.DOWNLOADING,
        UmaInfobarShown.COMPLETE,
        UmaInfobarShown.FAILED,
        UmaInfobarShown.PENDING,
        UmaInfobarShown.MULTIPLE_DOWNLOADING,
        UmaInfobarShown.MULTIPLE_COMPLETE,
        UmaInfobarShown.MULTIPLE_FAILED,
        UmaInfobarShown.MULTIPLE_PENDING,
        UmaInfobarShown.SCHEDULED,
        UmaInfobarShown.MULTIPLE_SCHEDULED,
        UmaInfobarShown.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface UmaInfobarShown {
        int ANY_STATE = 0;
        int ACCELERATED = 1;
        int DOWNLOADING = 2;
        int COMPLETE = 3;
        int FAILED = 4;
        int PENDING = 5;
        int MULTIPLE_DOWNLOADING = 6;
        int MULTIPLE_COMPLETE = 7;
        int MULTIPLE_FAILED = 8;
        int MULTIPLE_PENDING = 9;
        int SCHEDULED = 10;
        int MULTIPLE_SCHEDULED = 11;
        int NUM_ENTRIES = 12;
    }

    /**
     * Represents various UI states that the Message UI cycles through.
     * Note: This enum is append-only and the values must match the DownloadInfoBarState enum in
     * enums.xml. Values should be number from 0 and can't have gaps.
     */
    @VisibleForTesting
    @IntDef({
        UiState.INITIAL,
        UiState.DOWNLOADING,
        UiState.SHOW_RESULT,
        UiState.CANCELLED,
        UiState.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    protected @interface UiState {
        // Default initial state. It is also the final state after all the downloads are paused or
        // removed. No UI is shown in this state.
        int INITIAL = 0;
        // UI is showing a message indicating the downloads in progress.
        // If download completes,fails or goes to pending state, the transition happens immediately
        // to SHOW_RESULT state.
        int DOWNLOADING = 1;
        // The message is showing download complete, failed or pending message. The message stays in
        // this state for {@code DURATION_SHOW_RESULT_IN_MS} before transitioning to the next state,
        // which can be another SHOW_RESULT or DOWNLOADING state. This can also happen to be the
        // terminal state if there are no more updates to be shown.
        // In case of a new download, completed download or cancellation signal, the transition
        // happens immediately.
        int SHOW_RESULT = 2;
        // The state of the message after it was explicitly cancelled by the user. The message UI is
        // resurfaced only when there is a new download or an existing download moves to completion,
        // failed or pending state.
        int CANCELLED = 3;
        // Number of entries
        int NUM_ENTRIES = 4;
    }

    // Represents various result states shown in the message UI.
    private @interface ResultState {
        int INVALID = -1;
        int COMPLETE = 0;
        int FAILED = 1;
        int PENDING = 2;
    }

    @IntDef({IconType.DRAWABLE, IconType.VECTOR_DRAWABLE, IconType.ANIMATED_VECTOR_DRAWABLE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface IconType {
        int DRAWABLE = 0;
        int VECTOR_DRAWABLE = 1;
        int ANIMATED_VECTOR_DRAWABLE = 2;
    }

    /** Represents the values for the histogram Download.Incognito.Message. */
    @IntDef({
        IncognitoMessageEvent.SHOWN,
        IncognitoMessageEvent.ACCEPTED,
        IncognitoMessageEvent.DISMISSED_WITH_GESTURE,
        IncognitoMessageEvent.DISMISSED_WITH_TIMER,
        IncognitoMessageEvent.NUM_ENTRIES,
        IncognitoMessageEvent.DISMISSED_WITH_DIFFERENT_REASON,
        IncognitoMessageEvent.NOT_SHOWN_NULL_MESSAGE_DISPATCHER
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface IncognitoMessageEvent {
        int SHOWN = 0;
        int ACCEPTED = 1;
        int DISMISSED_WITH_GESTURE = 2;
        int DISMISSED_WITH_TIMER = 3;
        int DISMISSED_WITH_DIFFERENT_REASON = 4;
        int NOT_SHOWN_NULL_MESSAGE_DISPATCHER = 5;

        int NUM_ENTRIES = 6;
    }

    /** Represents the data required to show UI elements of the message. */
    public static class DownloadProgressMessageUiData {
        @Nullable public ContentId id;

        public String message;
        public String description;
        public String link;
        public int icon;
        public boolean ignoreAction;

        public @IconType int iconType = IconType.DRAWABLE;

        // Whether the the message must be shown, even though it was dismissed earlier. This
        // usually means there is a significant download update, e.g. download completed.
        public boolean forceShow;

        // Keeps track of the current number of downloads in various states.
        public DownloadCount downloadCount = new DownloadCount();

        // Used for differentiating various states (e.g. completed, failed, pending etc) in the
        // SHOW_RESULT state. Keeps track of the state of the currently displayed item(s) and should
        // be reset to null when moving out DOWNLOADING/SHOW_RESULT state.
        @ResultState public int resultState;

        @Override
        public int hashCode() {
            int result = (id == null ? 0 : id.hashCode());
            result = 31 * result + (message == null ? 0 : message.hashCode());
            result = 31 * result + (link == null ? 0 : link.hashCode());
            result = 31 * result + icon;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadProgressMessageUiData)) return false;

            DownloadProgressMessageUiData other = (DownloadProgressMessageUiData) obj;
            boolean idEquality = (id == null ? other.id == null : id.equals(other.id));
            return idEquality
                    && TextUtils.equals(message, other.message)
                    && TextUtils.equals(description, other.description)
                    && TextUtils.equals(link, other.link)
                    && icon == other.icon;
        }

        /** Called to update the value of this object from a given object. */
        public void update(DownloadProgressMessageUiData other) {
            id = other.id;
            message = other.message;
            link = other.link;
            icon = other.icon;
            iconType = other.iconType;
            forceShow = other.forceShow;
            downloadCount = other.downloadCount;
            resultState = other.resultState;
            ignoreAction = other.ignoreAction;
        }
    }

    /** An utility class to count the number of downloads at different states at any given time. */
    private static class DownloadCount {
        public int inProgress;
        public int pending;
        public int failed;
        public int completed;
        // Download is blocked, each blocked downloaded is also counted in failed.
        public int blocked;

        /**
         * @return The total number of downloads being tracked.
         */
        public int totalCount() {
            return inProgress + pending + failed + completed;
        }

        public int getCountForResultState(@ResultState int state) {
            switch (state) {
                case ResultState.COMPLETE:
                    return completed;
                case ResultState.FAILED:
                    return failed;
                case ResultState.PENDING:
                    return pending;
                default:
                    assert false;
            }
            return 0;
        }

        @Override
        public int hashCode() {
            int result = 31 * inProgress;
            result = 31 * result + pending;
            result = 31 * result + failed;
            result = 31 * result + completed;
            result = 31 * result + blocked;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadCount)) return false;

            DownloadCount other = (DownloadCount) obj;
            return inProgress == other.inProgress
                    && pending == other.pending
                    && failed == other.failed
                    && completed == other.completed
                    && blocked == other.blocked;
        }
    }

    private final Handler mHandler = new Handler();

    // Keeps track of a running list of items, which gets updated regularly with every update from
    // the backend. The entries are removed only when the item has reached a certain completion
    // state (i.e. complete, failed or pending) or is cancelled/removed from the backend.
    private final LinkedHashMap<ContentId, OfflineItem> mTrackedItems = new LinkedHashMap<>();

    // Keeps track of all the items that have been seen in the current chrome session.
    private final Set<ContentId> mSeenItems = new HashSet<>();

    // Keeps track of the items which are being ignored by the controller, e.g. user initiated
    // paused items.
    private final Set<ContentId> mIgnoredItems = new HashSet<>();

    // Keeps track of the items which are being downloaded in an interstitial.
    private final Set<ContentId> mInterstitialItems = new HashSet<>();

    // Used to calculate which items are being handled by a download interstitial.
    private final Set<GURL> mDownloadInterstitialSources = new HashSet<>();

    // The notification IDs associated with the currently tracked completed items. The notification
    // should be removed when the message action button is clicked to open the item.
    private final Map<ContentId, Integer> mNotificationIds = new HashMap<>();

    // The current state of the message UI.
    private @UiState int mState = UiState.INITIAL;

    // This is used when the message UI is currently in a state awaiting timer completion, e.g.
    // showing the result of a download. This is used to schedule a task to determine the next
    // state. If the message UI moves out of the current state, the scheduled task should be
    // cancelled.
    private Runnable mEndTimerRunnable;

    // Represents the currently displayed UI data.
    private DownloadProgressMessageUiData mCurrentInfo;

    // The delegate to provide dependencies.
    private final Delegate mDelegate;

    // The model used to update the UI properties.
    private PropertyModel mPropertyModel;

    private Runnable mDismissRunnable;

    /** Constructor. */
    public DownloadMessageUiControllerImpl(Delegate delegate) {
        mDelegate = delegate;
        mHandler.post(() -> getOfflineContentProvider().addObserver(this));
    }

    /**
     * Shows the message that download has started. Unlike other methods in this class, this
     * method doesn't require an {@link OfflineItem} and is invoked by the backend to provide a
     * responsive feedback to the users even before the download has actually started.
     */
    @Override
    public void onDownloadStarted() {
        computeNextStepForUpdate(null, true, false, false);
    }

    @Override
    public void showIncognitoDownloadMessage(Callback<Boolean> callback) {
        Context context = ContextUtils.getApplicationContext();

        MessageDispatcher dispatcher = getMessageDispatcher();
        // TODO(crbug.com/40234025): Fix the issue with dispatcher
        //                                  being Null and remove the following if clause
        if (dispatcher == null) {
            // When the message dispatcher is null we don't want to block the download, hence
            // we mimic the accepted workflow.
            callback.onResult(/* accepted= */ true);
            recordIncognitoDownloadMessage(IncognitoMessageEvent.NOT_SHOWN_NULL_MESSAGE_DISPATCHER);
            return;
        }

        PropertyModel propertyModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.DOWNLOAD_INCOGNITO_WARNING)
                        .build();

        propertyModel.set(
                MessageBannerProperties.TITLE,
                context.getString(R.string.incognito_download_message_title));
        propertyModel.set(
                MessageBannerProperties.DESCRIPTION,
                context.getString(R.string.incognito_download_message_detail));
        propertyModel.set(
                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                context.getString(R.string.incognito_download_message_button));
        propertyModel.set(
                MessageBannerProperties.ICON,
                AppCompatResources.getDrawable(context, R.drawable.ic_incognito_download_message));
        propertyModel.set(
                MessageBannerProperties.ON_PRIMARY_ACTION,
                () -> {
                    callback.onResult(/* accepted= */ true);
                    recordIncognitoDownloadMessage(IncognitoMessageEvent.ACCEPTED);
                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                });
        propertyModel.set(
                MessageBannerProperties.ON_DISMISSED,
                (dismissReason) -> {
                    if (dismissReason == DismissReason.TIMER) {
                        recordIncognitoDownloadMessage(IncognitoMessageEvent.DISMISSED_WITH_TIMER);
                    } else if (dismissReason == DismissReason.GESTURE) {
                        recordIncognitoDownloadMessage(
                                IncognitoMessageEvent.DISMISSED_WITH_GESTURE);
                    } else if (dismissReason == DismissReason.PRIMARY_ACTION) {
                        // Dismissal triggered by ON_PRIMARY_ACTION handler, which is already
                        // running the download callback. Here we need to not record this action
                        // into the dismiss reasons buckets.
                        return;
                    } else {
                        recordIncognitoDownloadMessage(
                                IncognitoMessageEvent.DISMISSED_WITH_DIFFERENT_REASON);
                    }
                    callback.onResult(/* accepted= */ false);
                });

        dispatcher.enqueueWindowScopedMessage(propertyModel, /* highPriority= */ true);
        recordIncognitoDownloadMessage(IncognitoMessageEvent.SHOWN);
    }

    /** Associates a notification ID with the tracked download for future usage. */
    // TODO(shaktisahu): Find an alternative way after moving to offline content provider.
    @Override
    public void onNotificationShown(ContentId id, int notificationId) {
        mNotificationIds.put(id, notificationId);
    }

    /**
     * Registers a new URL source for which a download interstitial download will be initiated.
     * @param originalUrl The URL of the download.
     */
    @Override
    public void addDownloadInterstitialSource(GURL originalUrl) {
        mDownloadInterstitialSources.add(originalUrl);
    }

    /**
     * Returns true if the given download information matches an interstitial download.
     * @param originalUrl The URL of the download.
     * @param guid Unique GUID of the download.
     */
    @Override
    public boolean isDownloadInterstitialItem(GURL originalUrl, String guid) {
        if (mDownloadInterstitialSources != null
                && mDownloadInterstitialSources.contains(originalUrl)) {
            return true;
        }
        if (mInterstitialItems == null) {
            return false;
        }
        for (ContentId id : mInterstitialItems) {
            if (id.id.equals(guid)) {
                mInterstitialItems.remove(id);
                return true;
            }
        }
        return false;
    }

    @Override
    public void onItemsAdded(List<OfflineItem> items) {
        for (OfflineItem item : items) {
            if (!isVisibleToUser(item)) continue;
            computeNextStepForUpdate(item);
        }
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (!mSeenItems.contains(id)) return;

        mTrackedItems.remove(id);
        mNotificationIds.remove(id);
        computeNextStepForUpdate(null, false, false, true);
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        if (mDownloadInterstitialSources.contains(item.originalUrl)) {
            mDownloadInterstitialSources.remove(item.originalUrl);
            mInterstitialItems.add(item.id);
        }
        if (item.state == OfflineItemState.COMPLETE) {
            mInterstitialItems.remove(item.id);
        }
        if (!isVisibleToUser(item)) return;

        if (updateDelta != null
                && !updateDelta.stateChanged
                && item.state == OfflineItemState.COMPLETE) {
            return;
        }

        if (item.state == OfflineItemState.CANCELLED) {
            onItemRemoved(item.id);
            return;
        }

        computeNextStepForUpdate(item);
    }

    /** @return Whether the message is currently showing. */
    @Override
    public boolean isShowing() {
        return mPropertyModel != null;
    }

    private MessageDispatcher getMessageDispatcher() {
        return mDelegate.getMessageDispatcher();
    }

    private ModalDialogManager getModalDialogManager() {
        return mDelegate.getModalDialogManager();
    }

    private boolean isVisibleToUser(OfflineItem offlineItem) {
        if (offlineItem.isTransient || offlineItem.isSuggested || offlineItem.isDangerous) {
            return false;
        }

        if (LegacyHelpers.isLegacyDownload(offlineItem.id)) {
            boolean shouldNotify =
                    offlineItem.state == OfflineItemState.FAILED
                            && offlineItem.failState == FailState.FILE_BLOCKED;
            if (!shouldNotify && TextUtils.isEmpty(offlineItem.filePath)) {
                return false;
            }
        }

        if (MimeUtils.canAutoOpenMimeType(offlineItem.mimeType) && offlineItem.hasUserGesture) {
            return false;
        }

        return true;
    }

    private void computeNextStepForUpdate(OfflineItem updatedItem) {
        computeNextStepForUpdate(updatedItem, false, false, false);
    }

    /**
     * Updates the state of the UI based on the update received and current state of the
     * tracked downloads.
     * @param updatedItem The item that was updated just now.
     * @param forceShowDownloadStarted Whether the message should show download started even if
     * there is no updated item.
     * @param userCancel Whether the message was cancelled just now.
     * ended.
     */
    private void computeNextStepForUpdate(
            OfflineItem updatedItem,
            boolean forceShowDownloadStarted,
            boolean userCancel,
            boolean itemWasRemoved) {
        if (updatedItem != null
                && (mIgnoredItems.contains(updatedItem.id)
                        || mInterstitialItems.contains(updatedItem.id))) {
            return;
        }

        preProcessUpdatedItem(updatedItem);
        boolean isNewDownload =
                forceShowDownloadStarted
                        || (updatedItem != null
                                && updatedItem.state == OfflineItemState.IN_PROGRESS
                                && !mSeenItems.contains(updatedItem.id));
        boolean itemResumedFromPending = itemResumedFromPending(updatedItem);

        if (updatedItem != null) {
            mTrackedItems.put(updatedItem.id, updatedItem);
            mSeenItems.add(updatedItem.id);
        }

        boolean itemWasPaused = updatedItem != null && updatedItem.state == OfflineItemState.PAUSED;
        if (itemWasPaused) {
            mIgnoredItems.add(updatedItem.id);
            mTrackedItems.remove(updatedItem.id);
        }

        DownloadCount downloadCount = getDownloadCount();

        boolean shouldShowResult =
                (downloadCount.completed + downloadCount.failed + downloadCount.pending) > 0;
        @UiState int nextState = mState;
        switch (mState) {
            case UiState.INITIAL: // Intentional fallthrough.
            case UiState.CANCELLED:
                if (isNewDownload) {
                    nextState = UiState.DOWNLOADING;
                } else if (shouldShowResult) {
                    nextState = UiState.SHOW_RESULT;
                }
                break;
            case UiState.DOWNLOADING:
                if (shouldShowResult) {
                    nextState = UiState.SHOW_RESULT;
                } else if (itemWasPaused || itemWasRemoved) {
                    nextState =
                            downloadCount.inProgress == 0 ? UiState.INITIAL : UiState.DOWNLOADING;
                }
                break;
            case UiState.SHOW_RESULT:
                if (isNewDownload) {
                    nextState = UiState.DOWNLOADING;
                } else if (!shouldShowResult) {
                    if (mEndTimerRunnable == null && downloadCount.inProgress > 0) {
                        nextState = UiState.DOWNLOADING;
                    }

                    boolean currentlyShowingPending =
                            mCurrentInfo != null && mCurrentInfo.resultState == ResultState.PENDING;
                    if (currentlyShowingPending && itemResumedFromPending) {
                        nextState = UiState.DOWNLOADING;
                    }
                    if ((itemWasPaused || itemWasRemoved) && mTrackedItems.size() == 0) {
                        nextState = UiState.INITIAL;
                    }
                }
                break;
        }

        if (userCancel) nextState = UiState.CANCELLED;

        moveToState(nextState);
    }

    private void moveToState(@UiState int nextState) {
        boolean closePreviousMessage =
                nextState == UiState.INITIAL || nextState == UiState.CANCELLED;
        if (closePreviousMessage) {
            mCurrentInfo = null;
            closePreviousMessage();
            if (nextState == UiState.INITIAL) {
                mTrackedItems.clear();
            } else {
                clearFinishedItems(ResultState.COMPLETE, ResultState.FAILED, ResultState.PENDING);
            }
            clearEndTimerRunnable();
        }

        if (nextState == UiState.DOWNLOADING || nextState == UiState.SHOW_RESULT) {
            int resultState = findOfflineItemStateForUiState(nextState);
            if (resultState == ResultState.INVALID) {
                // This is expected in the terminal SHOW_RESULT state when we have cleared the
                // tracked items but still want to show the message indefinitely.
                // TODO(shaktisahu): Does this state still happen?
                return;
            }
            createMessageForState(nextState, resultState);
        }

        mState = nextState;
    }

    /**
     * Determines the {@link OfflineItemState} for the message to be shown on the message. For
     * DOWNLOADING state, it will return {@link OfflineItemState#IN_PROGRESS}. Otherwise it should
     * show the result state which can be complete, failed or pending. There is usually a delay of
     * DURATION_SHOW_RESULT_IN_MS between transition between these states, except for the complete
     * state which must be shown as soon as received. While the UI is in one of these states,
     * if we get another download update for the same state, we incorporate that in the existing
     * message and reset the timer to another full duration. Updates for pending and failed would be
     * shown in the order received.
     */
    private @ResultState int findOfflineItemStateForUiState(@UiState int uiState) {
        if (uiState == UiState.DOWNLOADING) return OfflineItemState.IN_PROGRESS;

        assert uiState == UiState.SHOW_RESULT;

        DownloadCount downloadCount = getDownloadCount();

        // If there are completed downloads, show immediately.
        if (downloadCount.completed > 0) return ResultState.COMPLETE;

        // If the message is already showing this state, just add this item to the same state.
        int previousResultState =
                mCurrentInfo != null ? mCurrentInfo.resultState : ResultState.INVALID;
        if (previousResultState != ResultState.INVALID
                && downloadCount.getCountForResultState(previousResultState) > 0) {
            return previousResultState;
        }

        // Show any failed or pending states in the order they were received.
        for (OfflineItem item : mTrackedItems.values()) {
            int resultState = fromOfflineItemState(item);
            if (resultState != ResultState.INVALID) return resultState;
        }

        return ResultState.INVALID;
    }

    /**
     * Prepares the message to show the next state. This includes setting the message title,
     * description, icon, and action.
     * @param uiState The UI state to be shown.
     * @param resultState The state of the corresponding offline items to be shown.
     */
    private void createMessageForState(@UiState int uiState, @ResultState int resultState) {
        if (getContext() == null) return;
        DownloadProgressMessageUiData info = new DownloadProgressMessageUiData();

        @PluralsRes int stringRes = -1;
        if (uiState == UiState.DOWNLOADING) {
            stringRes = R.plurals.download_message_multiple_download_in_progress;
            info.icon = R.drawable.downloading_fill_animation_24dp;
            info.iconType = IconType.ANIMATED_VECTOR_DRAWABLE;
        } else if (resultState == ResultState.COMPLETE) {
            stringRes = R.plurals.download_message_multiple_download_complete;
            info.icon = R.drawable.infobar_download_complete;
            info.iconType = IconType.VECTOR_DRAWABLE;
        } else if (resultState == ResultState.FAILED) {
            stringRes = R.plurals.download_message_multiple_download_failed;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else if (resultState == ResultState.PENDING) {
            stringRes = R.plurals.download_message_multiple_download_pending;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else {
            assert false : "Unexpected resultState " + resultState + " and uiState " + uiState;
        }

        OfflineItem itemToShow = null;
        for (OfflineItem item : mTrackedItems.values()) {
            if (fromOfflineItemState(item) != resultState) continue;
            itemToShow = item;
        }

        DownloadCount downloadCount = getDownloadCount();
        if (uiState == UiState.DOWNLOADING) {
            int inProgressDownloadCount =
                    downloadCount.inProgress == 0 ? 1 : downloadCount.inProgress;
            info.message =
                    getContext()
                            .getResources()
                            .getQuantityString(
                                    stringRes, inProgressDownloadCount, inProgressDownloadCount);
            info.description =
                    getContext()
                            .getString(R.string.download_message_download_in_progress_description);

            info.link = getContext().getString(R.string.details_link);
        } else if (uiState == UiState.SHOW_RESULT) {
            int itemCount = getDownloadCount().getCountForResultState(resultState);
            boolean singleDownloadCompleted = itemCount == 1 && resultState == ResultState.COMPLETE;
            info.message =
                    getContext().getResources().getQuantityString(stringRes, itemCount, itemCount);
            if (singleDownloadCompleted) {
                String bytesString =
                        org.chromium.components.browser_ui.util.DownloadUtils.getStringForBytes(
                                getContext(), itemToShow.totalSizeBytes);
                String displayUrl =
                        DownloadUtils.formatUrlForDisplayInNotification(
                                itemToShow.url, DownloadUtils.MAX_ORIGIN_LENGTH_FOR_NOTIFICATION);
                info.description =
                        getContext()
                                .getString(
                                        R.string.download_message_download_complete_description,
                                        bytesString,
                                        displayUrl);
                info.id = itemToShow.id;
                info.link = getContext().getString(R.string.open_downloaded_label);
                info.icon = R.drawable.infobar_download_complete_animation;
            } else {
                // TODO(shaktisahu): Incorporate various types of failure messages.
                // TODO(shaktisahu, xingliu): Consult UX to handle multiple schedule variations.
                boolean allFailedDownloadsAreBlocked =
                        (downloadCount.blocked == downloadCount.failed);
                if (downloadCount.blocked > 0) {
                    if (allFailedDownloadsAreBlocked) {
                        info.description =
                                getContext()
                                        .getString(
                                                R.string.download_message_single_download_blocked);
                    } else {
                        info.description =
                                getContext()
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals
                                                        .download_message_multiple_download_blocked,
                                                downloadCount.blocked,
                                                downloadCount.blocked);
                    }
                }
                if (allFailedDownloadsAreBlocked) {
                    info.link = getContext().getString(R.string.ok);
                    info.ignoreAction = true;
                } else {
                    info.link = getContext().getString(R.string.details_link);
                }
            }
        }

        info.resultState = resultState;

        if (info.equals(mCurrentInfo)) return;

        boolean startTimer = uiState == UiState.SHOW_RESULT;

        clearEndTimerRunnable();

        if (startTimer) {
            long delay = getDelayToNextStep(resultState);
            mEndTimerRunnable =
                    () -> {
                        mEndTimerRunnable = null;
                        if (mCurrentInfo != null) mCurrentInfo.resultState = ResultState.INVALID;
                        if (uiState == UiState.SHOW_RESULT) {
                            clearFinishedItems(resultState);
                        }
                        computeNextStepForUpdate(null, false, false, false);
                    };
            mHandler.postDelayed(mEndTimerRunnable, delay);
        }

        setForceShow(info);
        mCurrentInfo = info;
        showMessage(uiState, info);
    }

    private void setForceShow(DownloadProgressMessageUiData info) {
        info.downloadCount = getDownloadCount();
        info.forceShow =
                !info.downloadCount.equals(
                        mCurrentInfo == null ? null : mCurrentInfo.downloadCount);
    }

    private void clearEndTimerRunnable() {
        mHandler.removeCallbacks(mEndTimerRunnable);
        mEndTimerRunnable = null;
    }

    private void preProcessUpdatedItem(OfflineItem updatedItem) {
        if (updatedItem == null) return;

        // INTERRUPTED downloads should be treated as PENDING in the UI. From here onwards,
        // there should be no INTERRUPTED state in the core logic.
        if (updatedItem.state == OfflineItemState.INTERRUPTED) {
            updatedItem.state = OfflineItemState.PENDING;
        }
    }

    private boolean itemResumedFromPending(OfflineItem updatedItem) {
        if (updatedItem == null || !mTrackedItems.containsKey(updatedItem.id)) return false;

        return mTrackedItems.get(updatedItem.id).state == OfflineItemState.PENDING
                && updatedItem.state == OfflineItemState.IN_PROGRESS;
    }

    @VisibleForTesting
    protected long getDelayToNextStep(@ResultState int resultState) {
        return DURATION_SHOW_RESULT_IN_MS;
    }

    /**
     * Central function called to show the message UI. If the previous message has been dismissed,
     * it will be recreated only if |info.forceShow| is true. If the message hasn't been dismissed,
     * it will be simply updated.
     * @param state The state of the message to be shown.
     * @param info Contains the information to be displayed in the UI.
     */
    @VisibleForTesting
    protected void showMessage(@UiState int state, DownloadProgressMessageUiData info) {
        if (mDelegate.maybeSwitchToFocusedActivity()) {
            closePreviousMessage();
        }

        boolean shouldShowMessage =
                getMessageDispatcher() != null && (info.forceShow || mPropertyModel != null);
        if (!shouldShowMessage) return;

        recordMessageState(state, info);

        Drawable drawable = createDrawable(info);

        boolean updateOnly = mPropertyModel != null;
        if (mPropertyModel == null) {
            mPropertyModel =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(
                                    MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.DOWNLOAD_PROGRESS)
                            .build();
        }

        if (info.iconType == IconType.ANIMATED_VECTOR_DRAWABLE) {
            mPropertyModel.set(
                    MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE);
            drawable = drawable.mutate();
            final AnimatedVectorDrawableCompat animatedDrawable =
                    (AnimatedVectorDrawableCompat) drawable;
            animatedDrawable.start();
            animatedDrawable.registerAnimationCallback(
                    new Animatable2Compat.AnimationCallback() {
                        @Override
                        public void onAnimationEnd(Drawable drawable) {
                            if (mCurrentInfo == null || mCurrentInfo.icon != info.icon) return;
                            animatedDrawable.start();
                        }
                    });
        }

        mPropertyModel.set(MessageBannerProperties.ICON, drawable);
        mPropertyModel.set(MessageBannerProperties.TITLE, info.message);

        String description = info.description == null ? "" : info.description;
        mPropertyModel.set(
                MessageBannerProperties.DESCRIPTION,
                description.substring(0, Math.min(MAX_DESCRIPTION_LENGTH, description.length())));

        mPropertyModel.set(MessageBannerProperties.DESCRIPTION_MAX_LINES, 3);
        mPropertyModel.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, info.link);
        mPropertyModel.set(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed);
        mPropertyModel.set(
                MessageBannerProperties.ON_PRIMARY_ACTION,
                () -> onPrimaryAction(info.id, info.ignoreAction));
        final MessageDispatcher dispatcher = getMessageDispatcher();
        mDismissRunnable =
                () -> {
                    if (dispatcher == null) return;
                    dispatcher.dismissMessage(mPropertyModel, DismissReason.SCOPE_DESTROYED);
                };

        if (updateOnly) return;
        getMessageDispatcher()
                .enqueueWindowScopedMessage(mPropertyModel, /* highPriority= */ false);
    }

    @VisibleForTesting
    protected void closePreviousMessage() {
        if (mDismissRunnable != null) mDismissRunnable.run();
        mPropertyModel = null;
    }

    private Context getContext() {
        return mDelegate.getContext();
    }

    private Drawable createDrawable(DownloadProgressMessageUiData info) {
        switch (info.iconType) {
            case IconType.DRAWABLE:
                return AppCompatResources.getDrawable(getContext(), info.icon);
            case IconType.VECTOR_DRAWABLE:
                return TraceEventVectorDrawableCompat.create(
                        getContext().getResources(), info.icon, getContext().getTheme());
            case IconType.ANIMATED_VECTOR_DRAWABLE:
                return AnimatedVectorDrawableCompat.create(getContext(), info.icon);
            default:
                assert false : "Unexpected icon type: " + info.iconType;
                return null;
        }
    }

    private DownloadCount getDownloadCount() {
        DownloadCount downloadCount = new DownloadCount();
        for (OfflineItem item : mTrackedItems.values()) {
            switch (item.state) {
                case OfflineItemState.IN_PROGRESS:
                    downloadCount.inProgress++;
                    break;
                case OfflineItemState.COMPLETE:
                    downloadCount.completed++;
                    break;
                case OfflineItemState.FAILED:
                    downloadCount.failed++;
                    if (item.failState == FailState.FILE_BLOCKED) {
                        downloadCount.blocked++;
                    }
                    break;
                case OfflineItemState.CANCELLED:
                    break;
                case OfflineItemState.PENDING:
                    downloadCount.pending++;
                    break;
                case OfflineItemState.INTERRUPTED: // intentional fall through
                case OfflineItemState.PAUSED: // intentional fall through
                default:
                    assert false;
            }
        }

        return downloadCount;
    }

    /**
     * Clears the items in finished state, i.e. completed, failed or pending.
     * @param states States to be removed.
     */
    private void clearFinishedItems(Integer... states) {
        Set<Integer> statesToRemove = new HashSet<>(Arrays.asList(states));
        List<ContentId> idsToRemove = new ArrayList<>();
        for (ContentId id : mTrackedItems.keySet()) {
            OfflineItem item = mTrackedItems.get(id);
            if (item == null) continue;
            for (Integer stateToRemove : statesToRemove) {
                if (stateToRemove == fromOfflineItemState(item)) {
                    idsToRemove.add(id);
                    break;
                }
            }
        }

        for (ContentId id : idsToRemove) {
            mTrackedItems.remove(id);
            mNotificationIds.remove(id);
        }
    }

    private @ResultState int fromOfflineItemState(OfflineItem offlineItem) {
        switch (offlineItem.state) {
            case OfflineItemState.COMPLETE:
                return ResultState.COMPLETE;
            case OfflineItemState.FAILED:
                return ResultState.FAILED;
            case OfflineItemState.PENDING:
                return ResultState.PENDING;
            default:
                return ResultState.INVALID;
        }
    }

    private OfflineContentProvider getOfflineContentProvider() {
        return OfflineContentAggregatorFactory.get();
    }

    private void removeNotification(ContentId contentId) {
        if (!mNotificationIds.containsKey(contentId)) return;

        DownloadInfo downloadInfo = new DownloadInfo.Builder().setContentId(contentId).build();
        mDelegate.removeNotification(mNotificationIds.get(contentId), downloadInfo);
        mNotificationIds.remove(contentId);
    }

    private @PrimaryActionClickBehavior int onPrimaryAction(
            ContentId itemId, boolean ignoreAction) {
        OfflineItem offlineItem = mTrackedItems.remove(itemId);
        removeNotification(itemId);
        if (ignoreAction) {
            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
        }

        if (itemId != null) {
            mDelegate.openDownload(
                    itemId,
                    OTRProfileID.deserializeWithoutVerify(
                            offlineItem == null ? null : offlineItem.otrProfileId),
                    DownloadOpenSource.DOWNLOAD_PROGRESS_MESSAGE,
                    getContext());
            recordLinkClicked(/* openItem= */ true);
        } else {
            // TODO(shaktisahu): Make a best guess for which profile, maybe from the last updated
            // item.
            mDelegate.openDownloadsPage(
                    getOTRProfileIDForTrackedItems(), DownloadOpenSource.DOWNLOAD_PROGRESS_MESSAGE);
            recordLinkClicked(/* openItem= */ false);
        }
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private OTRProfileID getOTRProfileIDForTrackedItems() {
        String otrProfileId = null;
        for (OfflineItem offlineItem : mTrackedItems.values()) {
            if (TextUtils.isEmpty(offlineItem.otrProfileId)) continue;
            otrProfileId = offlineItem.otrProfileId;
        }
        return OTRProfileID.deserializeWithoutVerify(otrProfileId);
    }

    private void onMessageDismissed(Integer dismissReason) {
        mPropertyModel = null;
        if (dismissReason == DismissReason.GESTURE) {
            computeNextStepForUpdate(null, false, true, false);
        }
    }

    private static void recordMessageState(@UiState int state, DownloadProgressMessageUiData info) {
        int shownState = -1;
        int multipleDownloadState = -1;
        if (state == UiState.DOWNLOADING) {
            shownState =
                    info.downloadCount.inProgress == 1
                            ? UmaInfobarShown.DOWNLOADING
                            : UmaInfobarShown.MULTIPLE_DOWNLOADING;
        } else if (state == UiState.SHOW_RESULT) {
            switch (info.resultState) {
                case ResultState.COMPLETE:
                    shownState =
                            info.downloadCount.completed == 1
                                    ? UmaInfobarShown.COMPLETE
                                    : UmaInfobarShown.MULTIPLE_COMPLETE;
                    break;
                case ResultState.FAILED:
                    shownState =
                            info.downloadCount.failed == 1
                                    ? UmaInfobarShown.FAILED
                                    : UmaInfobarShown.MULTIPLE_FAILED;
                    break;
                case ResultState.PENDING:
                    shownState =
                            info.downloadCount.pending == 1
                                    ? UmaInfobarShown.PENDING
                                    : UmaInfobarShown.MULTIPLE_PENDING;
                    break;
                default:
                    assert false : "Unexpected state " + info.resultState;
                    break;
            }
        }

        assert shownState != -1 : "Invalid state " + state;

        RecordHistogram.recordEnumeratedHistogram(
                "Download.Progress.InfoBar.Shown", shownState, UmaInfobarShown.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "Download.Progress.InfoBar.Shown",
                UmaInfobarShown.ANY_STATE,
                UmaInfobarShown.NUM_ENTRIES);
        if (multipleDownloadState != -1) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Download.Progress.InfoBar.Shown",
                    multipleDownloadState,
                    UmaInfobarShown.NUM_ENTRIES);
        }
    }

    private static void recordLinkClicked(boolean openItem) {
        if (openItem) {
            RecordUserAction.record("Android.Download.InfoBar.LinkClicked.OpenDownload");
        } else {
            RecordUserAction.record("Android.Download.InfoBar.LinkClicked.OpenDownloadHome");
        }
    }

    /**
     * Collects incognito download message event metrics.
     * @param event The UI event to collect.
     */
    private static void recordIncognitoDownloadMessage(@IncognitoMessageEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.Incognito.Message", event, IncognitoMessageEvent.NUM_ENTRIES);
    }
}
