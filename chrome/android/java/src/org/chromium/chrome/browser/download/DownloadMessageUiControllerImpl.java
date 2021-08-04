// Copyright 2021 The Chromium Authors. All rights reserved.
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
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadLaterMetrics.DownloadLaterUiEvent;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogHelper;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogHelper.Source;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.download.DownloadState;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

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

/**
 * Message UI specific implementation of {@link DownloadMessageUiController}.
 */
public class DownloadMessageUiControllerImpl implements DownloadMessageUiController {
    private static final String SPEEDING_UP_MESSAGE_ENABLED = "speeding_up_message_enabled";
    private static final long DURATION_ACCELERATED_INFOBAR_IN_MS = 3000;
    private static final long DURATION_SHOW_RESULT_IN_MS = 6000;
    private static final long DURATION_SHOW_RESULT_DOWNLOAD_SCHEDULED_IN_MS = 12000;

    // Values for the histogram Android.Download.InfoBar.Shown. Keep this in sync with the
    // DownloadInfoBar.ShownState enum in enums.xml.
    @IntDef({UmaInfobarShown.ANY_STATE, UmaInfobarShown.ACCELERATED, UmaInfobarShown.DOWNLOADING,
            UmaInfobarShown.COMPLETE, UmaInfobarShown.FAILED, UmaInfobarShown.PENDING,
            UmaInfobarShown.MULTIPLE_DOWNLOADING, UmaInfobarShown.MULTIPLE_COMPLETE,
            UmaInfobarShown.MULTIPLE_FAILED, UmaInfobarShown.MULTIPLE_PENDING,
            UmaInfobarShown.SCHEDULED, UmaInfobarShown.MULTIPLE_SCHEDULED})
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
    @IntDef({DownloadInfoBarState.INITIAL, DownloadInfoBarState.DOWNLOADING,
            DownloadInfoBarState.SHOW_RESULT, DownloadInfoBarState.CANCELLED})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface DownloadInfoBarState {
        // Default initial state. It is also the final state after all the downloads are paused or
        // removed. No UI is shown in this state.
        int INITIAL = 0;
        // UI is showing a message indicating the downloads in progress. In case of a single
        // accelerated download, the message would show the speeding-up download message for {@code
        // DURATION_ACCELERATED_INFOBAR_IN_MS} before transitioning to downloading file(s) message.
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
        int SCHEDULED = 3;
    }

    /**
     * Represents the data required to show UI elements of the message.
     */
    public static class DownloadProgressMessageUiData {
        @Nullable
        public ContentId id;

        public String message;
        public String description;
        public String link;
        public int icon;

        // Whether the icon corresponds to a vector drawable.
        public boolean hasVectorDrawable;

        // Whether the the message must be shown, even though it was dismissed earlier. This
        // usually means there is a significant download update, e.g. download completed.
        public boolean forceShow;

        // Keeps track of the current number of downloads in various states.
        public DownloadCount downloadCount = new DownloadCount();

        // Used for differentiating various states (e.g. completed, failed, pending etc) in the
        // SHOW_RESULT state. Keeps track of the state of the currently displayed item(s) and should
        // be reset to null when moving out DOWNLOADING/SHOW_RESULT state.
        @ResultState
        public int resultState;

        // Contains the information to change the download schedule for download later feature.
        public OfflineItemSchedule schedule;

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
            return idEquality && TextUtils.equals(message, other.message)
                    && TextUtils.equals(description, other.description)
                    && TextUtils.equals(link, other.link) && icon == other.icon;
        }

        /** Called to update the value of this object from a given object. */
        public void update(DownloadProgressMessageUiData other) {
            id = other.id;
            message = other.message;
            link = other.link;
            icon = other.icon;
            hasVectorDrawable = other.hasVectorDrawable;
            forceShow = other.forceShow;
            downloadCount = other.downloadCount;
            resultState = other.resultState;
            schedule = other.schedule;
        }
    }

    /**
     * An utility class to count the number of downloads at different states at any given time.
     */
    private static class DownloadCount {
        public int inProgress;
        public int pending;
        public int failed;
        public int completed;
        public int scheduled;

        /** @return The total number of downloads being tracked. */
        public int totalCount() {
            return inProgress + pending + failed + completed + scheduled;
        }

        public int getCountForResultState(@ResultState int state) {
            switch (state) {
                case ResultState.COMPLETE:
                    return completed;
                case ResultState.FAILED:
                    return failed;
                case ResultState.PENDING:
                    return pending;
                case ResultState.SCHEDULED:
                    return scheduled;
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
            result = 31 * result + scheduled;
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof DownloadCount)) return false;

            DownloadCount other = (DownloadCount) obj;
            return inProgress == other.inProgress && pending == other.pending
                    && failed == other.failed && completed == other.completed
                    && scheduled == other.scheduled;
        }
    }

    private final boolean mUseNewDownloadPath;
    private final OTRProfileID mOtrProfileID;
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

    // The notification IDs associated with the currently tracked completed items. The notification
    // should be removed when the message action button is clicked to open the item.
    private final Map<ContentId, Integer> mNotificationIds = new HashMap<>();

    // The current state of the message UI.
    private @DownloadInfoBarState int mState = DownloadInfoBarState.INITIAL;

    // This is used when the message UI is currently in a state awaiting timer completion, e.g.
    // showing the speeding-up message or showing the result of a download. This is used to schedule
    // a task to determine the next state. If the message UI moves out of the current state, the
    // scheduled task should be cancelled.
    private Runnable mEndTimerRunnable;

    // Represents the currently displayed UI data.
    private DownloadProgressMessageUiData mCurrentInfo;

    // Used to show the download later dialog to change download schedule.
    private DownloadLaterDialogHelper mDownloadLaterDialogHelper;

    // The associated activity context.
    private final Context mContext;

    // The message dispatcher for showing the message UI.
    private final MessageDispatcher mMessageDispatcher;

    // Dialog manager used for creating download later dialogs.
    private final ModalDialogManager mModalDialogManager;

    // The model used to update the UI properties.
    private PropertyModel mPropertyModel;

    private Runnable mDismissRunnable;

    /** Constructor. */
    public DownloadMessageUiControllerImpl(OTRProfileID otrProfileID, Context context,
            MessageDispatcher messageDispatcher, ModalDialogManager modalDialogManager) {
        mUseNewDownloadPath =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER);
        mOtrProfileID = otrProfileID;
        mMessageDispatcher = messageDispatcher;
        mContext = context;
        mModalDialogManager = modalDialogManager;

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

    /** Updates the message UI when new information about a download comes in. */
    @Override
    public void onDownloadItemUpdated(DownloadItem downloadItem) {
        if (mUseNewDownloadPath) return;

        OfflineItem offlineItem = DownloadItem.createOfflineItem(downloadItem);
        if (!isVisibleToUser(offlineItem)) return;

        if (downloadItem.getDownloadInfo().state() == DownloadState.COMPLETE) {
            handleDownloadCompletion(downloadItem);
            return;
        }

        if (offlineItem.state == OfflineItemState.CANCELLED) {
            onItemRemoved(offlineItem.id);
            return;
        }

        computeNextStepForUpdate(offlineItem);
    }

    /** Updates the message UI after a download has been removed. */
    @Override
    public void onDownloadItemRemoved(ContentId contentId) {
        if (mUseNewDownloadPath) return;
        onItemRemoved(contentId);
    }

    /** Associates a notification ID with the tracked download for future usage. */
    // TODO(shaktisahu): Find an alternative way after moving to offline content provider.
    @Override
    public void onNotificationShown(ContentId id, int notificationId) {
        mNotificationIds.put(id, notificationId);
    }

    private void handleDownloadCompletion(DownloadItem downloadItem) {
        // Multiple OnDownloadUpdated() notifications may be issued while the
        // download is in the COMPLETE state. Don't handle it if it was previously not in-progress.
        if (!mTrackedItems.containsKey(downloadItem.getContentId())) return;

        // If the download should be auto-opened, we shouldn't show the UI.
        DownloadManagerService.getDownloadManagerService().checkIfDownloadWillAutoOpen(
                downloadItem, (result) -> {
                    if (result) {
                        onItemRemoved(downloadItem.getContentId());
                    } else {
                        computeNextStepForUpdate(DownloadItem.createOfflineItem(downloadItem));
                    }
                });
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
        if (!isVisibleToUser(item)) return;

        if (updateDelta != null && !updateDelta.stateChanged
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

    private boolean isVisibleToUser(OfflineItem offlineItem) {
        // Need to use serialized OTRProfileID for comparison, since calling
        // |OTRProfileID#deserialize| method causes crash if the OTR profile is destroyed.
        String stringOTRProfileID = OTRProfileID.serialize(mOtrProfileID);
        if (offlineItem.isTransient
                || !OTRProfileID.areEqual(stringOTRProfileID, offlineItem.otrProfileId)
                || offlineItem.isSuggested || offlineItem.isDangerous) {
            return false;
        }
        if (LegacyHelpers.isLegacyDownload(offlineItem.id)
                && TextUtils.isEmpty(offlineItem.filePath)) {
            return false;
        }

        if (MimeUtils.canAutoOpenMimeType(offlineItem.mimeType)) {
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
    private void computeNextStepForUpdate(OfflineItem updatedItem, boolean forceShowDownloadStarted,
            boolean userCancel, boolean itemWasRemoved) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR)) return;

        if (updatedItem != null && mIgnoredItems.contains(updatedItem.id)) return;

        preProcessUpdatedItem(updatedItem);
        boolean isNewDownload = forceShowDownloadStarted
                || (updatedItem != null && updatedItem.state == OfflineItemState.IN_PROGRESS
                        && updatedItem.schedule == null && !mSeenItems.contains(updatedItem.id));
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

        boolean shouldShowResult = (downloadCount.completed + downloadCount.failed
                                           + downloadCount.pending + downloadCount.scheduled)
                > 0;

        boolean shouldShowAccelerating =
                mEndTimerRunnable != null && mState == DownloadInfoBarState.DOWNLOADING;

        @DownloadInfoBarState
        int nextState = mState;
        switch (mState) {
            case DownloadInfoBarState.INITIAL: // Intentional fallthrough.
            case DownloadInfoBarState.CANCELLED:
                if (isNewDownload) {
                    nextState = DownloadInfoBarState.DOWNLOADING;
                    shouldShowAccelerating =
                            isAccelerated(updatedItem) && downloadCount.inProgress == 1;
                } else if (shouldShowResult) {
                    nextState = DownloadInfoBarState.SHOW_RESULT;
                }
                break;
            case DownloadInfoBarState.DOWNLOADING:
                if (isNewDownload) shouldShowAccelerating = false;

                if (shouldShowResult) {
                    nextState = DownloadInfoBarState.SHOW_RESULT;
                } else if (itemWasPaused || itemWasRemoved) {
                    nextState = downloadCount.inProgress == 0 ? DownloadInfoBarState.INITIAL
                                                              : DownloadInfoBarState.DOWNLOADING;
                }
                break;
            case DownloadInfoBarState.SHOW_RESULT:
                if (isNewDownload) {
                    nextState = DownloadInfoBarState.DOWNLOADING;
                    shouldShowAccelerating =
                            isAccelerated(updatedItem) && downloadCount.inProgress == 1;
                } else if (!shouldShowResult) {
                    if (mEndTimerRunnable == null && downloadCount.inProgress > 0) {
                        nextState = DownloadInfoBarState.DOWNLOADING;
                    }

                    boolean currentlyShowingPending =
                            mCurrentInfo != null && mCurrentInfo.resultState == ResultState.PENDING;
                    if (currentlyShowingPending && itemResumedFromPending) {
                        nextState = DownloadInfoBarState.DOWNLOADING;
                    }
                    if ((itemWasPaused || itemWasRemoved) && mTrackedItems.size() == 0) {
                        nextState = DownloadInfoBarState.INITIAL;
                    }
                }
                break;
        }

        if (userCancel) nextState = DownloadInfoBarState.CANCELLED;

        moveToState(nextState, shouldShowAccelerating);
    }

    private void moveToState(@DownloadInfoBarState int nextState, boolean showAccelerating) {
        boolean closePreviousMessage = nextState == DownloadInfoBarState.INITIAL
                || nextState == DownloadInfoBarState.CANCELLED;
        if (closePreviousMessage) {
            mCurrentInfo = null;
            closePreviousMessage();
            if (nextState == DownloadInfoBarState.INITIAL) {
                mTrackedItems.clear();
            } else {
                clearFinishedItems(ResultState.COMPLETE, ResultState.FAILED, ResultState.PENDING,
                        ResultState.SCHEDULED);
            }
            clearEndTimerRunnable();
        }

        if (nextState == DownloadInfoBarState.DOWNLOADING
                || nextState == DownloadInfoBarState.SHOW_RESULT) {
            int resultState = findOfflineItemStateForInfoBarState(nextState);
            if (resultState == ResultState.INVALID) {
                // This is expected in the terminal SHOW_RESULT state when we have cleared the
                // tracked items but still want to show the infobar indefinitely.
                return;
            }
            createMessageForState(nextState, resultState, showAccelerating);
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
    private @ResultState int findOfflineItemStateForInfoBarState(
            @DownloadInfoBarState int infoBarState) {
        if (infoBarState == DownloadInfoBarState.DOWNLOADING) return OfflineItemState.IN_PROGRESS;

        assert infoBarState == DownloadInfoBarState.SHOW_RESULT;

        DownloadCount downloadCount = getDownloadCount();

        // If there are completed downloads, show immediately.
        if (downloadCount.completed > 0) return ResultState.COMPLETE;
        if (downloadCount.scheduled > 0) return ResultState.SCHEDULED;

        // If the infobar is already showing this state, just add this item to the same state.
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
    private void createMessageForState(@DownloadInfoBarState int uiState,
            @ResultState int resultState, boolean showAccelerating) {
        DownloadProgressMessageUiData info = new DownloadProgressMessageUiData();

        @PluralsRes
        int stringRes = -1;
        if (uiState == DownloadInfoBarState.DOWNLOADING) {
            stringRes = R.plurals.download_message_multiple_download_in_progress;
            info.icon = R.drawable.infobar_downloading;
        } else if (resultState == ResultState.COMPLETE) {
            stringRes = R.plurals.download_message_multiple_download_complete;
            info.icon = R.drawable.infobar_download_complete;
            info.hasVectorDrawable = true;
        } else if (resultState == ResultState.FAILED) {
            stringRes = R.plurals.download_message_multiple_download_failed;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else if (resultState == ResultState.PENDING) {
            stringRes = R.plurals.download_message_multiple_download_pending;
            info.icon = R.drawable.ic_error_outline_googblue_24dp;
        } else if (resultState == ResultState.SCHEDULED) {
            stringRes = R.plurals.download_message_multiple_download_scheduled;
            info.icon = R.drawable.ic_file_download_scheduled_24dp;
        } else {
            assert false : "Unexpected resultState " + resultState + " and infoBarState " + uiState;
        }

        OfflineItem itemToShow = null;
        for (OfflineItem item : mTrackedItems.values()) {
            if (fromOfflineItemState(item) != resultState) continue;
            itemToShow = item;
        }

        DownloadCount downloadCount = getDownloadCount();
        if (uiState == DownloadInfoBarState.DOWNLOADING) {
            int inProgressDownloadCount =
                    downloadCount.inProgress == 0 ? 1 : downloadCount.inProgress;
            info.message = getContext().getResources().getQuantityString(
                    stringRes, inProgressDownloadCount, inProgressDownloadCount);
            info.description = getContext().getString(
                    R.string.download_message_download_in_progress_description);

            info.link = showAccelerating ? null : getContext().getString(R.string.details_link);
        } else if (uiState == DownloadInfoBarState.SHOW_RESULT) {
            int itemCount = getDownloadCount().getCountForResultState(resultState);
            boolean singleDownloadCompleted = itemCount == 1 && resultState == ResultState.COMPLETE;
            boolean singleDownloadScheduled =
                    itemCount == 1 && resultState == ResultState.SCHEDULED;
            info.message =
                    getContext().getResources().getQuantityString(stringRes, itemCount, itemCount);
            if (singleDownloadCompleted) {
                String bytesString =
                        org.chromium.components.browser_ui.util.DownloadUtils.getStringForBytes(
                                getContext(), itemToShow.totalSizeBytes);
                String displayUrl = UrlFormatter.formatUrlForSecurityDisplay(
                        itemToShow.url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
                info.description = getContext().getString(
                        R.string.download_message_download_complete_description, bytesString,
                        displayUrl);
                info.id = itemToShow.id;
                info.link = getContext().getString(R.string.open_downloaded_label);
                info.icon = R.drawable.infobar_download_complete_animation;
            } else if (singleDownloadScheduled) {
                // TODO(shaktisahu, xingliu): Find out what the message should be.
                info.description = getContext().getString(
                        R.string.download_message_download_scheduled_description);
                info.link = getContext().getString(R.string.change_link);
                info.id = itemToShow.id;
                info.schedule = itemToShow.schedule.clone();
            } else {
                // TODO(shaktisahu): Incorporate various types of failure messages.
                // TODO(shaktisahu, xingliu): Consult UX to handle multiple schedule variations.
                info.link = getContext().getString(R.string.details_link);
            }
        }

        info.resultState = resultState;

        if (info.equals(mCurrentInfo)) return;

        boolean startTimer = showAccelerating || uiState == DownloadInfoBarState.SHOW_RESULT;

        clearEndTimerRunnable();

        if (startTimer) {
            long delay = getDelayToNextStep(showAccelerating, resultState);
            mEndTimerRunnable = () -> {
                mEndTimerRunnable = null;
                if (mCurrentInfo != null) mCurrentInfo.resultState = ResultState.INVALID;
                if (uiState == DownloadInfoBarState.SHOW_RESULT) {
                    clearFinishedItems(resultState);
                }
                computeNextStepForUpdate(null, false, false, false);
            };
            mHandler.postDelayed(mEndTimerRunnable, delay);
        }

        setForceShow(info);
        showMessage(uiState, info);
    }

    private void setForceShow(DownloadProgressMessageUiData info) {
        info.downloadCount = getDownloadCount();
        info.forceShow = !info.downloadCount.equals(
                mCurrentInfo == null ? null : mCurrentInfo.downloadCount);

        // TODO(xingliu, shaktisahu): downloadCount may not be updated at the correct time, see
        // https://crbug.com/1127522. For now, scheduled download will always show in new tabs.
        if (info.downloadCount.scheduled > 0) {
            info.forceShow = true;
        }
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
    protected long getDelayToNextStep(boolean showAccelerating, @ResultState int resultState) {
        if (showAccelerating) return DURATION_ACCELERATED_INFOBAR_IN_MS;

        // Scheduled download uses a longer delay to reset tracking downloads states.
        return resultState == ResultState.SCHEDULED ? DURATION_SHOW_RESULT_DOWNLOAD_SCHEDULED_IN_MS
                                                    : DURATION_SHOW_RESULT_IN_MS;
    }

    @VisibleForTesting
    protected boolean isSpeedingUpMessageEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.DOWNLOAD_PROGRESS_INFOBAR, SPEEDING_UP_MESSAGE_ENABLED, false);
    }

    private boolean isAccelerated(OfflineItem offlineItem) {
        return isSpeedingUpMessageEnabled() && offlineItem != null && offlineItem.isAccelerated;
    }

    /**
     * Central function called to show the message UI. If the previous message has been dismissed,
     * it will be recreated only if |info.forceShow| is true. If the message hasn't been dismissed,
     * it will be simply updated.
     * @param state The state of the message to be shown.
     * @param info Contains the information to be displayed in the UI.
     */
    @VisibleForTesting
    protected void showMessage(
            @DownloadInfoBarState int state, DownloadProgressMessageUiData info) {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_PROGRESS_MESSAGE);
        recordInfoBarState(state, info);

        boolean shouldShowMessage = info.forceShow || (mPropertyModel != null);
        if (!shouldShowMessage) return;

        Drawable drawable = info.hasVectorDrawable
                ? VectorDrawableCompat.create(
                        getContext().getResources(), info.icon, getContext().getTheme())
                : ApiCompatibilityUtils.getDrawable(getContext().getResources(), info.icon);

        boolean updateOnly = mPropertyModel != null;
        if (mPropertyModel == null) {
            mPropertyModel = new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                     .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                             MessageIdentifier.DOWNLOAD_PROGRESS)
                                     .build();
        }

        mPropertyModel.set(MessageBannerProperties.ICON, drawable);
        mPropertyModel.set(MessageBannerProperties.TITLE, info.message);
        mPropertyModel.set(MessageBannerProperties.DESCRIPTION, info.description);
        mPropertyModel.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, info.link);
        mPropertyModel.set(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed);
        mPropertyModel.set(MessageBannerProperties.ON_PRIMARY_ACTION,
                () -> onPrimaryAction(info.id, info.schedule));
        mDismissRunnable = () -> {
            mMessageDispatcher.dismissMessage(mPropertyModel, DismissReason.SCOPE_DESTROYED);
        };

        if (updateOnly) return;
        mMessageDispatcher.enqueueWindowScopedMessage(mPropertyModel, /*highPriority=*/false);
        recordInfoBarCreated();
    }

    @VisibleForTesting
    protected void closePreviousMessage() {
        if (mDismissRunnable != null) mDismissRunnable.run();
        mPropertyModel = null;
    }

    private Context getContext() {
        return mContext;
    }

    private DownloadCount getDownloadCount() {
        DownloadCount downloadCount = new DownloadCount();
        for (OfflineItem item : mTrackedItems.values()) {
            if (item.schedule != null) {
                downloadCount.scheduled++;
                continue;
            }

            switch (item.state) {
                case OfflineItemState.IN_PROGRESS:
                    downloadCount.inProgress++;
                    break;
                case OfflineItemState.COMPLETE:
                    downloadCount.completed++;
                    break;
                case OfflineItemState.FAILED:
                    downloadCount.failed++;
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
        if (offlineItem.schedule != null) return ResultState.SCHEDULED;

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
        DownloadManagerService.getDownloadManagerService()
                .getDownloadNotifier()
                .removeDownloadNotification(mNotificationIds.get(contentId), downloadInfo);
        mNotificationIds.remove(contentId);
    }

    private void onPrimaryAction(ContentId itemId, final OfflineItemSchedule schedule) {
        mTrackedItems.remove(itemId);
        removeNotification(itemId);
        if (itemId != null && schedule != null) {
            onChangeScheduleClicked(itemId, schedule);
        } else if (itemId != null) {
            DownloadUtils.openItem(itemId, mOtrProfileID,
                    DownloadOpenSource.DOWNLOAD_PROGRESS_MESSAGE, getContext());
            recordLinkClicked(true /*openItem*/);
        } else {
            DownloadManagerService.openDownloadsPage(
                    getContext(), mOtrProfileID, DownloadOpenSource.DOWNLOAD_PROGRESS_MESSAGE);
            recordLinkClicked(false /*openItem*/);
        }
    }

    private void onMessageDismissed(Integer dismissReason) {
        mPropertyModel = null;
        if (dismissReason == DismissReason.GESTURE) {
            recordCloseButtonClicked();
            computeNextStepForUpdate(null, false, true, false);
        }
    }

    private void onChangeScheduleClicked(
            final ContentId id, final OfflineItemSchedule currentSchedule) {
        if (mDownloadLaterDialogHelper != null) mDownloadLaterDialogHelper.destroy();

        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        // Show the download later dialog to let the user change download schedule.
        mDownloadLaterDialogHelper =
                DownloadLaterDialogHelper.create(getContext(), mModalDialogManager, prefService);
        DownloadLaterMetrics.recordDownloadLaterUiEvent(
                DownloadLaterUiEvent.DOWNLOAD_INFOBAR_CHANGE_SCHEDULE_CLICKED);
        mDownloadLaterDialogHelper.showChangeScheduleDialog(
                currentSchedule, Source.DOWNLOAD_INFOBAR, (newSchedule) -> {
                    if (newSchedule == null) return;
                    if (mUseNewDownloadPath) {
                        OfflineContentAggregatorFactory.get().changeSchedule(id, newSchedule);
                    } else {
                        DownloadManagerService.getDownloadManagerService().changeSchedule(
                                id, newSchedule, mOtrProfileID);
                    }
                });
    }

    private void recordInfoBarState(
            @DownloadInfoBarState int state, DownloadProgressMessageUiData info) {
        int shownState = -1;
        int multipleDownloadState = -1;
        if (state == DownloadInfoBarState.DOWNLOADING) {
            shownState = mEndTimerRunnable != null
                    ? UmaInfobarShown.ACCELERATED
                    : (info.downloadCount.inProgress == 1 ? UmaInfobarShown.DOWNLOADING
                                                          : UmaInfobarShown.MULTIPLE_DOWNLOADING);
        } else if (state == DownloadInfoBarState.SHOW_RESULT) {
            switch (info.resultState) {
                case ResultState.COMPLETE:
                    shownState = info.downloadCount.completed == 1
                            ? UmaInfobarShown.COMPLETE
                            : UmaInfobarShown.MULTIPLE_COMPLETE;
                    break;
                case ResultState.FAILED:
                    shownState = info.downloadCount.failed == 1 ? UmaInfobarShown.FAILED
                                                                : UmaInfobarShown.MULTIPLE_FAILED;
                    break;
                case ResultState.PENDING:
                    shownState = info.downloadCount.pending == 1 ? UmaInfobarShown.PENDING
                                                                 : UmaInfobarShown.MULTIPLE_PENDING;
                    break;
                case ResultState.SCHEDULED:
                    shownState = info.downloadCount.scheduled == 1
                            ? UmaInfobarShown.SCHEDULED
                            : UmaInfobarShown.MULTIPLE_SCHEDULED;
                    break;
                default:
                    assert false : "Unexpected state " + info.resultState;
                    break;
            }
        }

        assert shownState != -1 : "Invalid state " + state;

        RecordHistogram.recordEnumeratedHistogram(
                "Android.Download.InfoBar.Shown", shownState, UmaInfobarShown.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram("Android.Download.InfoBar.Shown",
                UmaInfobarShown.ANY_STATE, UmaInfobarShown.NUM_ENTRIES);
        if (multipleDownloadState != -1) {
            RecordHistogram.recordEnumeratedHistogram("Android.Download.InfoBar.Shown",
                    multipleDownloadState, UmaInfobarShown.NUM_ENTRIES);
        }
    }

    private void recordInfoBarCreated() {
        RecordUserAction.record("Android.Download.InfoBar.Shown");
    }

    private void recordCloseButtonClicked() {
        RecordUserAction.record("Android.Download.InfoBar.CloseButtonClicked");
        RecordHistogram.recordEnumeratedHistogram("Android.Download.InfoBar.CloseButtonClicked",
                mState, DownloadInfoBarState.NUM_ENTRIES);
    }

    private void recordLinkClicked(boolean openItem) {
        if (openItem) {
            RecordUserAction.record("Android.Download.InfoBar.LinkClicked.OpenDownload");
        } else {
            RecordUserAction.record("Android.Download.InfoBar.LinkClicked.OpenDownloadHome");
        }
    }
}