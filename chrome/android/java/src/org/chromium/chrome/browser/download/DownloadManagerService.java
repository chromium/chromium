// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.annotation.SuppressLint;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.provider.MediaStore.MediaColumns;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadManagerBridge.DownloadEnqueueRequest;
import org.chromium.chrome.browser.download.DownloadManagerBridge.DownloadEnqueueResponse;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileKeyUtil;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.download.DownloadCollectionBridge;
import org.chromium.components.download.DownloadState;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.RejectedExecutionException;

/**
 * Chrome implementation of the {@link DownloadController.Observer} interface.
 * This class is responsible for keeping track of which downloads are in progress. It generates
 * updates for progress of downloads and handles cleaning up of interrupted progress notifications.
 * TODO(qinmin): move BroadcastReceiver inheritance into DownloadManagerBridge, as it
 * handles all Android DownloadManager interactions. And DownloadManagerService should not know
 * download Id issued by Android DownloadManager.
 */
public class DownloadManagerService implements DownloadServiceDelegate, ProfileManager.Observer {
    private static final String TAG = "DownloadService";
    private static final String DOWNLOAD_RETRY_COUNT_FILE_NAME = "DownloadRetryCount";
    private static final String DOWNLOAD_MANUAL_RETRY_SUFFIX = ".Manual";
    private static final String DOWNLOAD_TOTAL_RETRY_SUFFIX = ".Total";
    private static final long UPDATE_DELAY_MILLIS = 1000;
    public static final long UNKNOWN_BYTES_RECEIVED = -1;

    private static final Set<String> sFirstSeenDownloadIds = new HashSet<String>();

    private static DownloadManagerService sDownloadManagerService;
    private static boolean sIsNetworkListenerDisabled;
    private static boolean sIsNetworkMetered;

    private final HashMap<String, DownloadProgress> mDownloadProgressMap =
            new HashMap<String, DownloadProgress>(4, 0.75f);

    private final DownloadNotifier mDownloadNotifier;
    // Delay between UI updates.
    private final long mUpdateDelayInMillis;

    private final Handler mHandler;

    // Deprecated after new download backend.
    /** Generic interface for notifying external UI components about downloads and their states. */
    public interface DownloadObserver extends DownloadSharedPreferenceHelper.Observer {
        /** Called in response to {@link DownloadManagerService#getAllDownloads(OTRProfileID)}. */
        void onAllDownloadsRetrieved(final List<DownloadItem> list, ProfileKey profileKey);

        /** Called when a download is created. */
        void onDownloadItemCreated(DownloadItem item);

        /** Called when a download is updated. */
        void onDownloadItemUpdated(DownloadItem item);

        /** Called when a download has been removed. */
        void onDownloadItemRemoved(String guid);

        /** Only for testing */
        default void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {}
    }

    @VisibleForTesting
    protected final List<String> mAutoResumableDownloadIds = new ArrayList<String>();

    private final ObserverList<DownloadObserver> mDownloadObservers = new ObserverList<>();

    private OMADownloadHandler mOMADownloadHandler;
    private DownloadSnackbarController mDownloadSnackbarController;
    private DownloadMessageUiController mMessageUiController;
    private long mNativeDownloadManagerService;
    // Flag to track if we need to post a task to update download notifications.
    private boolean mIsUIUpdateScheduled;
    private int mAutoResumptionLimit = -1;
    private DownloadManagerRequestInterceptor mDownloadManagerRequestInterceptor;

    // Whether any ChromeActivity is launched.
    private boolean mActivityLaunched;

    // Disabling call to DownloadManager.addCompletedDownload() for test.
    private boolean mDisableAddCompletedDownloadForTesting;

    /**
     * Interface to intercept download request to Android DownloadManager. This is implemented by
     * tests so that we don't need to actually enqueue a download into the Android DownloadManager.
     */
    interface DownloadManagerRequestInterceptor {
        void interceptDownloadRequest(DownloadItem item, boolean notifyComplete);
    }

    // Deprecated after new download backend.
    /** Class representing progress of a download. */
    private static class DownloadProgress {
        final long mStartTimeInMillis;
        boolean mCanDownloadWhileMetered;
        DownloadItem mDownloadItem;
        @DownloadStatus int mDownloadStatus;
        boolean mIsAutoResumable;
        boolean mIsUpdated;
        boolean mIsSupportedMimeType;

        DownloadProgress(
                long startTimeInMillis,
                boolean canDownloadWhileMetered,
                DownloadItem downloadItem,
                @DownloadStatus int downloadStatus) {
            mStartTimeInMillis = startTimeInMillis;
            mCanDownloadWhileMetered = canDownloadWhileMetered;
            mDownloadItem = downloadItem;
            mDownloadStatus = downloadStatus;
            mIsAutoResumable = false;
            mIsUpdated = true;
        }

        DownloadProgress(DownloadProgress progress) {
            mStartTimeInMillis = progress.mStartTimeInMillis;
            mCanDownloadWhileMetered = progress.mCanDownloadWhileMetered;
            mDownloadItem = progress.mDownloadItem;
            mDownloadStatus = progress.mDownloadStatus;
            mIsAutoResumable = progress.mIsAutoResumable;
            mIsUpdated = progress.mIsUpdated;
            mIsSupportedMimeType = progress.mIsSupportedMimeType;
        }
    }

    /** Creates DownloadManagerService. */
    public static DownloadManagerService getDownloadManagerService() {
        ThreadUtils.assertOnUiThread();
        if (sDownloadManagerService == null) {
            DownloadNotifier downloadNotifier = new SystemDownloadNotifier();
            sDownloadManagerService =
                    new DownloadManagerService(
                            downloadNotifier, new Handler(), UPDATE_DELAY_MILLIS);
        }
        return sDownloadManagerService;
    }

    public static boolean hasDownloadManagerService() {
        ThreadUtils.assertOnUiThread();
        return sDownloadManagerService != null;
    }

    /**
     * For tests only: sets the DownloadManagerService.
     * @param service An instance of DownloadManagerService.
     * @return Null or a currently set instance of DownloadManagerService.
     */
    @VisibleForTesting
    public static DownloadManagerService setDownloadManagerService(DownloadManagerService service) {
        ThreadUtils.assertOnUiThread();
        DownloadManagerService prev = sDownloadManagerService;
        sDownloadManagerService = service;
        return prev;
    }

    @VisibleForTesting
    void setDownloadManagerRequestInterceptor(DownloadManagerRequestInterceptor interceptor) {
        mDownloadManagerRequestInterceptor = interceptor;
    }

    @VisibleForTesting
    protected DownloadManagerService(
            DownloadNotifier downloadNotifier, Handler handler, long updateDelayInMillis) {
        Context applicationContext = ContextUtils.getApplicationContext();
        mDownloadNotifier = downloadNotifier;
        mUpdateDelayInMillis = updateDelayInMillis;
        mHandler = handler;
        mDownloadSnackbarController = new DownloadSnackbarController();
        mOMADownloadHandler = new OMADownloadHandler(applicationContext);
        DownloadCollectionBridge.setDownloadDelegate(new DownloadDelegateImpl());
        mOMADownloadHandler.clearPendingOMADownloads();
    }

    /** Initializes download related systems for background task. */
    public void initForBackgroundTask() {
        getNativeDownloadManagerService();
    }

    /** Pre-load shared prefs to avoid being blocked on the disk access async task in the future. */
    public static void warmUpSharedPrefs() {
        getAutoRetryCountSharedPreference();
    }

    public DownloadNotifier getDownloadNotifier() {
        return mDownloadNotifier;
    }

    /** @return The {@link DownloadMessageUiController} controller associated with the profile. */
    public DownloadMessageUiController getMessageUiController(OTRProfileID otrProfileID) {
        return mMessageUiController;
    }

    /** For testing only. */
    public void setInfoBarControllerForTesting(DownloadMessageUiController infoBarController) {
        var oldValue = mMessageUiController;
        mMessageUiController = infoBarController;
        ResettersForTesting.register(() -> mMessageUiController = oldValue);
    }

    // Deprecated after new download backend.
    public void onDownloadUpdated(final DownloadInfo downloadInfo) {
        DownloadItem item = new DownloadItem(false, downloadInfo);
        // If user manually paused a download, this download is no longer auto resumable.
        if (downloadInfo.isPaused()) {
            removeAutoResumableDownload(item.getId());
        }
        updateDownloadProgress(item, DownloadStatus.IN_PROGRESS);
        updateDownloadInfoBar(item);
        scheduleUpdateIfNeeded();
    }

    // Deprecated after new download backend.
    public void onDownloadCancelled(final DownloadInfo downloadInfo) {
        DownloadInfo newInfo =
                DownloadInfo.Builder.fromDownloadInfo(downloadInfo)
                        .setState(DownloadState.CANCELLED)
                        .build();
        DownloadItem item = new DownloadItem(false, newInfo);
        removeAutoResumableDownload(item.getId());
        updateDownloadProgress(new DownloadItem(false, downloadInfo), DownloadStatus.CANCELLED);
        updateDownloadInfoBar(item);
    }

    // Deprecated after new download backend.
    public void onDownloadInterrupted(final DownloadInfo downloadInfo, boolean isAutoResumable) {
        @DownloadStatus int status = DownloadStatus.INTERRUPTED;
        DownloadItem item = new DownloadItem(false, downloadInfo);
        if (!downloadInfo.isResumable()) {
            status = DownloadStatus.FAILED;
        } else if (isAutoResumable) {
            addAutoResumableDownload(item.getId());
        }

        updateDownloadProgress(item, status);
        updateDownloadInfoBar(item);
    }

    /**
     * Called when browser activity is launched. For background resumption and cancellation, this
     * will not be called.
     */
    public void onActivityLaunched(DownloadMessageUiController.Delegate delegate) {
        if (!mActivityLaunched) {
            mMessageUiController = DownloadMessageUiControllerFactory.create(delegate);

            DownloadManagerService.getDownloadManagerService()
                    .checkForExternallyRemovedDownloads(
                            ProfileKeyUtil.getLastUsedRegularProfileKey());

            mActivityLaunched = true;
        }
    }

    private void updateDownloadInfoBar(DownloadItem item) {}

    /**
     * Broadcast that a download was successful.
     * @param downloadInfo info about the download.
     */
    // For testing only.
    protected void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {
        for (DownloadObserver observer : mDownloadObservers) {
            observer.broadcastDownloadSuccessful(downloadInfo);
        }
    }

    /**
     * Gets download information from SharedPreferences.
     * @param sharedPrefs The SharedPreferencesManager to read from.
     * @param type Type of the information to retrieve.
     * @return download information saved to the SharedPrefs for the given type.
     */
    @VisibleForTesting
    protected static Set<String> getStoredDownloadInfo(
            SharedPreferencesManager sharedPrefs, String type) {
        return new HashSet<>(sharedPrefs.readStringSet(type));
    }

    /**
     * Stores download information to shared preferences. The information can be
     * either pending download IDs, or pending OMA downloads.
     *
     * @param sharedPrefs   SharedPreferencesManager to write to.
     * @param type          Type of the information.
     * @param downloadInfo  Information to be saved.
     * @param forceCommit   Whether to synchronously update shared preferences.
     */
    @SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
    static void storeDownloadInfo(
            SharedPreferencesManager sharedPrefs,
            String type,
            Set<String> downloadInfo,
            boolean forceCommit) {
        boolean success;
        if (downloadInfo.isEmpty()) {
            if (forceCommit) {
                success = sharedPrefs.removeKeySync(type);
            } else {
                sharedPrefs.removeKey(type);
                success = true;
            }
        } else {
            if (forceCommit) {
                success = sharedPrefs.writeStringSetSync(type, downloadInfo);
            } else {
                sharedPrefs.writeStringSet(type, downloadInfo);
                success = true;
            }
        }

        if (!success) {
            // Write synchronously because it might be used on restart and needs to stay
            // up-to-date.
            Log.e(TAG, "Failed to write DownloadInfo " + type);
        }
    }

    /**
     * Updates notifications for a given list of downloads.
     * @param progresses A list of notifications to update.
     */
    private void updateAllNotifications(List<DownloadProgress> progresses) {
        assert ThreadUtils.runningOnUiThread();
        for (int i = 0; i < progresses.size(); ++i) {
            updateNotification(progresses.get(i));
        }
    }

    // Deprecated after new download backend.
    /**
     * Update notification for a specific download.
     * @param progress Specific notification to update.
     */
    private void updateNotification(DownloadProgress progress) {
        DownloadItem item = progress.mDownloadItem;
        DownloadInfo info = item.getDownloadInfo();
        boolean notificationUpdateScheduled = true;
        boolean removeFromDownloadProgressMap = true;
        switch (progress.mDownloadStatus) {
            case DownloadStatus.COMPLETE:
                notificationUpdateScheduled = updateDownloadSuccessNotification(progress);
                removeFromDownloadProgressMap = notificationUpdateScheduled;
                break;
            case DownloadStatus.FAILED:
                // TODO(cmsy): Use correct FailState.
                mDownloadNotifier.notifyDownloadFailed(info);
                Log.w(TAG, "Download failed: " + info.getFilePath());
                break;
            case DownloadStatus.IN_PROGRESS:
                if (info.isPaused()) {
                    mDownloadNotifier.notifyDownloadPaused(info);
                } else {
                    mDownloadNotifier.notifyDownloadProgress(
                            info, progress.mStartTimeInMillis, progress.mCanDownloadWhileMetered);
                    removeFromDownloadProgressMap = false;
                }
                break;
            case DownloadStatus.CANCELLED:
                mDownloadNotifier.notifyDownloadCanceled(item.getContentId());
                break;
            case DownloadStatus.INTERRUPTED:
                mDownloadNotifier.notifyDownloadInterrupted(
                        info, progress.mIsAutoResumable, PendingState.PENDING_NETWORK);
                removeFromDownloadProgressMap = !progress.mIsAutoResumable;
                break;
            default:
                assert false;
                break;
        }
        if (notificationUpdateScheduled) progress.mIsUpdated = false;
        if (removeFromDownloadProgressMap) mDownloadProgressMap.remove(item.getId());
    }

    // Deprecated after new download backend.
    /**
     * Helper method to schedule a task to update the download success notification.
     * @param progress Download progress to update.
     * @return True if the task can be scheduled, or false otherwise.
     */
    private boolean updateDownloadSuccessNotification(DownloadProgress progress) {
        final boolean isSupportedMimeType = progress.mIsSupportedMimeType;
        final DownloadItem item = progress.mDownloadItem;

        AsyncTask<Pair<Boolean, Boolean>> task =
                new AsyncTask<Pair<Boolean, Boolean>>() {
                    @Override
                    public Pair<Boolean, Boolean> doInBackground() {
                        boolean success =
                                mDisableAddCompletedDownloadForTesting
                                        || ContentUriUtils.isContentUri(
                                                item.getDownloadInfo().getFilePath())
                                        || (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q);
                        boolean canResolve =
                                success
                                        && (MimeUtils.isOMADownloadDescription(
                                                        item.getDownloadInfo().getMimeType())
                                                || canResolveDownloadItem(
                                                        item, isSupportedMimeType));
                        return Pair.create(success, canResolve);
                    }

                    @Override
                    protected void onPostExecute(Pair<Boolean, Boolean> result) {
                        DownloadInfo info = item.getDownloadInfo();
                        if (result.first) {
                            mDownloadNotifier.notifyDownloadSuccessful(
                                    info,
                                    item.getSystemDownloadId(),
                                    result.second,
                                    isSupportedMimeType);
                            broadcastDownloadSuccessful(info);
                        } else {
                            info =
                                    DownloadInfo.Builder.fromDownloadInfo(info)
                                            .setFailState(FailState.CANNOT_DOWNLOAD)
                                            .build();
                            mDownloadNotifier.notifyDownloadFailed(info);
                            // TODO(qinmin): get the failure message from native.
                        }
                    }
                };
        try {
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return true;
        } catch (RejectedExecutionException e) {
            // Reaching thread limit, update will be reschduled for the next run.
            Log.e(TAG, "Thread limit reached, reschedule notification update later.");
            return false;
        }
    }

    @CalledByNative
    private void handleOMADownload(DownloadItem download, long systemDownloadId) {
        mOMADownloadHandler.handleOMADownload(download.getDownloadInfo(), systemDownloadId);
    }

    /**
     * Handle auto opennable files after download completes.
     * TODO(qinmin): move this to DownloadManagerBridge.
     *
     * @param download A download item.
     */
    private void handleAutoOpenAfterDownload(DownloadItem download) {
        if (MimeUtils.isOMADownloadDescription(download.getDownloadInfo().getMimeType())) {
            mOMADownloadHandler.handleOMADownload(
                    download.getDownloadInfo(), download.getSystemDownloadId());
            return;
        }
        openDownloadedContent(
                download.getDownloadInfo(),
                download.getSystemDownloadId(),
                DownloadOpenSource.AUTO_OPEN);
    }

    // Deprecated after new download backend.
    /** Schedule an update if there is no update scheduled. */
    @VisibleForTesting
    protected void scheduleUpdateIfNeeded() {
        if (mIsUIUpdateScheduled) return;

        mIsUIUpdateScheduled = true;
        final List<DownloadProgress> progressPendingUpdate = new ArrayList<DownloadProgress>();
        Iterator<DownloadProgress> iter = mDownloadProgressMap.values().iterator();
        while (iter.hasNext()) {
            DownloadProgress progress = iter.next();
            if (progress.mIsUpdated) {
                progressPendingUpdate.add(progress);
            }
        }
        if (progressPendingUpdate.isEmpty()) {
            mIsUIUpdateScheduled = false;
            return;
        }
        updateAllNotifications(progressPendingUpdate);

        Runnable scheduleNextUpdateTask =
                () -> {
                    mIsUIUpdateScheduled = false;
                    scheduleUpdateIfNeeded();
                };
        mHandler.postDelayed(scheduleNextUpdateTask, mUpdateDelayInMillis);
    }

    /**
     * Updates the progress of a download.
     *
     * @param downloadItem Information about the download.
     * @param downloadStatus Status of the download.
     */
    // Deprecated after new download backend.
    private void updateDownloadProgress(
            DownloadItem downloadItem, @DownloadStatus int downloadStatus) {
        boolean isSupportedMimeType =
                downloadStatus == DownloadStatus.COMPLETE
                        && isSupportedMimeType(downloadItem.getDownloadInfo().getMimeType());
        String id = downloadItem.getId();
        DownloadProgress progress = mDownloadProgressMap.get(id);
        if (progress == null) {
            if (!downloadItem.getDownloadInfo().isPaused()) {
                long startTime = System.currentTimeMillis();
                progress =
                        new DownloadProgress(
                                startTime,
                                isActiveNetworkMetered(ContextUtils.getApplicationContext()),
                                downloadItem,
                                downloadStatus);
                progress.mIsUpdated = true;
                progress.mIsSupportedMimeType = isSupportedMimeType;
                mDownloadProgressMap.put(id, progress);
                sFirstSeenDownloadIds.add(id);

                // This is mostly for testing, when the download is not tracked/progress is null but
                // downloadStatus is not DownloadStatus.IN_PROGRESS.
                if (downloadStatus != DownloadStatus.IN_PROGRESS) {
                    updateNotification(progress);
                }
            }
            return;
        }

        progress.mDownloadStatus = downloadStatus;
        progress.mDownloadItem = downloadItem;
        progress.mIsUpdated = true;
        progress.mIsAutoResumable = mAutoResumableDownloadIds.contains(id);
        progress.mIsSupportedMimeType = isSupportedMimeType;
        switch (downloadStatus) {
            case DownloadStatus.COMPLETE:
            case DownloadStatus.FAILED:
            case DownloadStatus.CANCELLED:
                clearDownloadRetryCount(id, true);
                clearDownloadRetryCount(id, false);
                updateNotification(progress);
                sFirstSeenDownloadIds.remove(id);
                break;
            case DownloadStatus.INTERRUPTED:
                updateNotification(progress);
                break;
            case DownloadStatus.IN_PROGRESS:
                if (downloadItem.getDownloadInfo().isPaused()) {
                    updateNotification(progress);
                }
                break;
            default:
                assert false;
        }
    }

    /** See {@link DownloadManagerBridge.enqueueNewDownload}. */
    public void enqueueNewDownload(final DownloadItem item, boolean notifyCompleted) {
        if (mDownloadManagerRequestInterceptor != null) {
            mDownloadManagerRequestInterceptor.interceptDownloadRequest(item, notifyCompleted);
            return;
        }

        DownloadEnqueueRequest request = new DownloadEnqueueRequest();
        request.url = item.getDownloadInfo().getUrl().getSpec();
        request.fileName = item.getDownloadInfo().getFileName();
        request.description = item.getDownloadInfo().getDescription();
        request.mimeType = item.getDownloadInfo().getMimeType();
        request.cookie = item.getDownloadInfo().getCookie();
        request.referrer = item.getDownloadInfo().getReferrer().getSpec();
        request.userAgent = item.getDownloadInfo().getUserAgent();
        request.notifyCompleted = notifyCompleted;
        DownloadManagerBridge.enqueueNewDownload(
                request,
                response -> {
                    onDownloadEnqueued(item, response);
                });
    }

    public void onDownloadEnqueued(DownloadItem downloadItem, DownloadEnqueueResponse response) {
        downloadItem.setStartTime(response.startTime);
        downloadItem.setSystemDownloadId(response.downloadId);
        if (!response.result) {
            onDownloadFailed(downloadItem, response.failureReason);
            return;
        }

        DownloadMessageUiController messageUiController =
                getMessageUiController(downloadItem.getDownloadInfo().getOTRProfileId());
        if (messageUiController != null) messageUiController.onDownloadStarted();
    }

    static @Nullable Intent getLaunchIntentForDownload(
            @Nullable String filePath,
            long downloadId,
            boolean isSupportedMimeType,
            String originalUrl,
            String referrer,
            @Nullable String mimeType) {
        assert !ThreadUtils.runningOnUiThread();
        if (downloadId == DownloadConstants.INVALID_DOWNLOAD_ID) {
            if (!ContentUriUtils.isContentUri(filePath)) return null;
            return getLaunchIntentFromDownloadUri(
                    filePath, isSupportedMimeType, originalUrl, referrer, mimeType);
        }

        DownloadManagerBridge.DownloadQueryResult queryResult =
                DownloadManagerBridge.queryDownloadResult(downloadId);
        if (mimeType == null) mimeType = queryResult.mimeType;

        Uri contentUri =
                filePath == null
                        ? queryResult.contentUri
                        : DownloadUtils.getUriForOtherApps(filePath);
        if (contentUri == null || Uri.EMPTY.equals(contentUri)) return null;

        Uri fileUri = filePath == null ? contentUri : Uri.fromFile(new File(filePath));
        return createLaunchIntent(
                fileUri, contentUri, mimeType, isSupportedMimeType, originalUrl, referrer);
    }

    /**
     * Similar to getLaunchIntentForDownload(), but only works for download that is stored as a
     * content Uri.
     *
     * @param contentUri Uri of the download.
     * @param isSupportedMimeType Whether the MIME type is supported by browser.
     * @param originalUrl The original url of the downloaded file
     * @param referrer Referrer of the downloaded file.
     * @param mimeType MIME type of the downloaded file.
     * @return the intent to launch for the given download item.
     */
    private static @Nullable Intent getLaunchIntentFromDownloadUri(
            String contentUri,
            boolean isSupportedMimeType,
            String originalUrl,
            String referrer,
            @Nullable String mimeType) {
        assert !ThreadUtils.runningOnUiThread();
        assert ContentUriUtils.isContentUri(contentUri);

        Uri uri = Uri.parse(contentUri);
        if (mimeType == null) {
            try (Cursor cursor =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .query(uri, null, null, null, null)) {
                if (cursor == null || cursor.getCount() == 0) return null;
                cursor.moveToNext();
                mimeType = cursor.getString(cursor.getColumnIndexOrThrow(MediaColumns.MIME_TYPE));
                cursor.close();
            }
        }
        return createLaunchIntent(uri, uri, mimeType, isSupportedMimeType, originalUrl, referrer);
    }

    /**
     * Creates a an intent to launch a download.
     * @param fileUri File uri of the download has an actual file path. Otherwise, this is the same
     *                as |contentUri|.
     * @param contentUri Content uri of the download.
     * @param isSupportedMimeType Whether the MIME type is supported by browser.
     * @param originalUrl The original url of the downloaded file
     * @param referrer   Referrer of the downloaded file.
     * @return the intent to launch for the given download item.
     */
    private static Intent createLaunchIntent(
            Uri fileUri,
            Uri contentUri,
            String mimeType,
            boolean isSupportedMimeType,
            String originalUrl,
            String referrer) {
        if (isSupportedMimeType) {
            // Sharing for media files is disabled on automotive.
            boolean isAutomotive = BuildInfo.getInstance().isAutomotive;

            // Redirect the user to an internal media viewer.  The file path is necessary to show
            // the real file path to the user instead of a content:// download ID.
            return MediaViewerUtils.getMediaViewerIntent(
                    fileUri,
                    contentUri,
                    mimeType,
                    /* allowExternalAppHandlers= */ !isAutomotive,
                    /* allowShareAction= */ !isAutomotive,
                    ContextUtils.getApplicationContext());
        }
        return MediaViewerUtils.createViewIntentForUri(contentUri, mimeType, originalUrl, referrer);
    }

    /**
     * Return whether a download item can be resolved to any activity.
     *
     * @param download A download item.
     * @param isSupportedMimeType Whether the MIME type is supported by browser.
     * @return true if the download item can be resolved, or false otherwise.
     */
    static boolean canResolveDownloadItem(DownloadItem download, boolean isSupportedMimeType) {
        assert !ThreadUtils.runningOnUiThread();
        Intent intent =
                getLaunchIntentForDownload(
                        download.getDownloadInfo().getFilePath(),
                        download.getSystemDownloadId(),
                        isSupportedMimeType,
                        null,
                        null,
                        download.getDownloadInfo().getMimeType());
        return (intent == null) ? false : ExternalNavigationHandler.resolveIntent(intent, true);
    }

    /** See {@link #openDownloadedContent(Context, String, boolean, boolean, String, long)}. */
    protected void openDownloadedContent(
            final DownloadInfo downloadInfo,
            final long downloadId,
            @DownloadOpenSource int source) {
        openDownloadedContent(
                ContextUtils.getApplicationContext(),
                downloadInfo.getFilePath(),
                isSupportedMimeType(downloadInfo.getMimeType()),
                downloadInfo.getOTRProfileId(),
                downloadInfo.getDownloadGuid(),
                downloadId,
                downloadInfo.getOriginalUrl().getSpec(),
                downloadInfo.getReferrer().getSpec(),
                source,
                downloadInfo.getMimeType());
    }

    /**
     * Launch the intent for a given download item, or Download Home if that's not possible.
     * TODO(qinmin): Move this to DownloadManagerBridge.
     *
     * @param context             Context to use.
     * @param filePath            Path to the downloaded item.
     * @param isSupportedMimeType Whether the MIME type is supported by Chrome.
     * @param otrProfileID        The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param downloadGuid        GUID of the download item in DownloadManager.
     * @param downloadId          ID of the download item in DownloadManager.
     * @param originalUrl         The original url of the downloaded file.
     * @param referrer            Referrer of the downloaded file.
     * @param source              The source that tries to open the download.
     * @param mimeType            MIME type of the download, could be null.
     */
    protected static void openDownloadedContent(
            final Context context,
            final String filePath,
            final boolean isSupportedMimeType,
            final OTRProfileID otrProfileID,
            final String downloadGuid,
            final long downloadId,
            final String originalUrl,
            final String referrer,
            @DownloadOpenSource int source,
            @Nullable String mimeType) {
        new AsyncTask<Intent>() {
            @Override
            public Intent doInBackground() {
                return getLaunchIntentForDownload(
                        filePath, downloadId, isSupportedMimeType, originalUrl, referrer, mimeType);
            }

            @Override
            protected void onPostExecute(Intent intent) {
                boolean didLaunchIntent =
                        intent != null
                                && ExternalNavigationHandler.resolveIntent(intent, true)
                                && DownloadUtils.fireOpenIntentForDownload(context, intent);

                if (!didLaunchIntent) {
                    openDownloadsPage(otrProfileID, source);
                    return;
                }

                if (didLaunchIntent && hasDownloadManagerService()) {
                    DownloadManagerService.getDownloadManagerService()
                            .updateLastAccessTime(downloadGuid, otrProfileID);
                    DownloadManager manager =
                            (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
                    String mimeType = manager.getMimeTypeForDownloadedFile(downloadId);
                    DownloadMetrics.recordDownloadOpen(source, mimeType);
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Called when a download fails.
     *
     * @param reason Reason of failure reported by android DownloadManager
     */
    @VisibleForTesting
    protected void onDownloadFailed(DownloadItem item, int reason) {
        String failureMessage =
                getDownloadFailureMessage(item.getDownloadInfo().getFileName(), reason);

        if (mDownloadSnackbarController.getSnackbarManager() != null) {
            mDownloadSnackbarController.onDownloadFailed(
                    failureMessage,
                    reason == DownloadManager.ERROR_FILE_ALREADY_EXISTS,
                    item.getDownloadInfo().getOTRProfileId());
        } else {
            Toast.makeText(ContextUtils.getApplicationContext(), failureMessage, Toast.LENGTH_SHORT)
                    .show();
        }
    }

    /**
     * Open the Activity which shows a list of all downloads.
     *
     * @param otrProfileID The {@link OTRProfileID} to determine whether to open download page in
     *     incognito profile. If null, download page will be opened in normal profile.
     * @param source The source where the user action coming from.
     */
    @CalledByNative
    public static void openDownloadsPage(
            OTRProfileID otrProfileID, @DownloadOpenSource int source) {
        if (DownloadUtils.showDownloadManager(null, null, otrProfileID, source)) return;

        // Open the Android Download Manager.
        Intent pageView = new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS);
        pageView.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            ContextUtils.getApplicationContext().startActivity(pageView);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find Downloads app", e);
        }
    }

    // Deprecated after new download backend.
    @Override
    public void resumeDownload(ContentId id, DownloadItem item) {
        DownloadProgress progress = mDownloadProgressMap.get(item.getId());
        if (progress != null
                && progress.mDownloadStatus == DownloadStatus.IN_PROGRESS
                && !progress.mDownloadItem.getDownloadInfo().isPaused()) {
            // Download already in progress, do nothing
            return;
        }
        if (progress == null) {
            assert !item.getDownloadInfo().isPaused();
            // If the download was not resumed before, the browser must have been killed while the
            // download is active.
            if (!sFirstSeenDownloadIds.contains(item.getId())) {
                sFirstSeenDownloadIds.add(item.getId());
            }
            updateDownloadProgress(item, DownloadStatus.IN_PROGRESS);
            progress = mDownloadProgressMap.get(item.getId());
        }

        // If user manually resumes a download, update the connection type that the download
        // can start. If the previous connection type is metered, manually resuming on an
        // unmetered network should not affect the original connection type.
        if (!progress.mCanDownloadWhileMetered) {
            progress.mCanDownloadWhileMetered =
                    isActiveNetworkMetered(ContextUtils.getApplicationContext());
        }
        incrementDownloadRetryCount(item.getId(), true);
        clearDownloadRetryCount(item.getId(), true);

        // Downloads started from incognito mode should not be resumed in reduced mode.
        if (!ProfileManager.isInitialized() && item.getDownloadInfo().isOffTheRecord()) return;

        OTRProfileID otrProfileID = item.getDownloadInfo().getOTRProfileId();
        DownloadManagerServiceJni.get()
                .resumeDownload(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        item.getId(),
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
    }

    /**
     * Called to cancel a download.
     * @param id The {@link ContentId} of the download to cancel.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     */
    // Deprecated after new download backend.
    @Override
    public void cancelDownload(ContentId id, OTRProfileID otrProfileID) {
        DownloadManagerServiceJni.get()
                .cancelDownload(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        id.id,
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
        DownloadProgress progress = mDownloadProgressMap.get(id.id);
        if (progress != null) {
            DownloadInfo info =
                    DownloadInfo.Builder.fromDownloadInfo(progress.mDownloadItem.getDownloadInfo())
                            .build();
            onDownloadCancelled(info);
            removeDownloadProgress(id.id);
        } else {
            mDownloadNotifier.notifyDownloadCanceled(id);
        }
    }

    /**
     * Called to pause a download.
     * @param id The {@link ContentId} of the download to pause.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     */
    // Deprecated after new download backend.
    @Override
    public void pauseDownload(ContentId id, OTRProfileID otrProfileID) {
        DownloadManagerServiceJni.get()
                .pauseDownload(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        id.id,
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
        DownloadProgress progress = mDownloadProgressMap.get(id.id);
        // Calling pause will stop listening to the download item. Update its progress now.
        // If download is already completed, canceled or failed, there is no need to update the
        // download notification.
        if (progress != null
                && (progress.mDownloadStatus == DownloadStatus.INTERRUPTED
                        || progress.mDownloadStatus == DownloadStatus.IN_PROGRESS)) {
            DownloadInfo info =
                    DownloadInfo.Builder.fromDownloadInfo(progress.mDownloadItem.getDownloadInfo())
                            .setIsPaused(true)
                            .setBytesReceived(UNKNOWN_BYTES_RECEIVED)
                            .build();
            onDownloadUpdated(info);
        }
    }

    @Override
    public void destroyServiceDelegate() {
        // Lifecycle of DownloadManagerService allows for this call to be ignored.
    }

    /**
     * Removes a download from the list.
     * @param downloadGuid GUID of the download.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param externallyRemoved If the file is externally removed by other applications.
     */
    public void removeDownload(
            final String downloadGuid, OTRProfileID otrProfileID, boolean externallyRemoved) {
        mHandler.post(
                () -> {
                    DownloadManagerServiceJni.get()
                            .removeDownload(
                                    getNativeDownloadManagerService(),
                                    DownloadManagerService.this,
                                    downloadGuid,
                                    IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
                    removeDownloadProgress(downloadGuid);
                });
    }

    /**
     * Checks whether the download can be opened by the browser.
     * @param mimeType MIME type of the file.
     * @return Whether the download is openable by the browser.
     */
    public boolean isDownloadOpenableInBrowser(String mimeType) {
        // TODO(qinmin): for audio and video, check if the codec is supported by Chrome.
        return isSupportedMimeType(mimeType);
    }

    /**
     * Checks whether a file with the given MIME type can be opened by the browser.
     * @param mimeType MIME type of the file.
     * @return Whether the file would be openable by the browser.
     */
    public static boolean isSupportedMimeType(String mimeType) {
        return DownloadManagerServiceJni.get().isSupportedMimeType(mimeType);
    }

    /**
     * Helper method to create and retrieve the native DownloadManagerService when needed.
     * @return pointer to native DownloadManagerService.
     */
    private long getNativeDownloadManagerService() {
        if (mNativeDownloadManagerService == 0) {
            boolean startupCompleted = ProfileManager.isInitialized();
            mNativeDownloadManagerService =
                    DownloadManagerServiceJni.get()
                            .init(DownloadManagerService.this, startupCompleted);
            if (!startupCompleted) ProfileManager.addObserver(this);
        }
        return mNativeDownloadManagerService;
    }

    @Override
    public void onProfileAdded(Profile profile) {
        ProfileManager.removeObserver(this);
        DownloadManagerServiceJni.get()
                .onProfileAdded(
                        mNativeDownloadManagerService, DownloadManagerService.this, profile);
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    @CalledByNative
    void onResumptionFailed(@JniType("std::string") String downloadGuid) {
        mDownloadNotifier.notifyDownloadFailed(
                new DownloadInfo.Builder()
                        .setDownloadGuid(downloadGuid)
                        .setFailState(FailState.CANNOT_DOWNLOAD)
                        .build());
        removeDownloadProgress(downloadGuid);
    }

    /**
     * Called when download success notification is shown.
     * @param info Information about the download.
     * @param canResolve Whether to open the download automatically.
     * @param notificationId Notification ID of the download.
     * @param systemDownloadId System download ID assigned by the Android DownloadManager.
     */
    public void onSuccessNotificationShown(
            DownloadInfo info, boolean canResolve, int notificationId, long systemDownloadId) {
        if (getMessageUiController(info.getOTRProfileId()) != null) {
            getMessageUiController(info.getOTRProfileId())
                    .onNotificationShown(info.getContentId(), notificationId);
        }

        if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
            Profile profile = ProfileManager.getLastUsedRegularProfile();
            if (OTRProfileID.isOffTheRecord(info.getOTRProfileId())) {
                profile =
                        profile.getOffTheRecordProfile(
                                info.getOTRProfileId(), /* createIfNeeded= */ true);
            }
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.notifyEvent(EventConstants.DOWNLOAD_COMPLETED);
        }
    }

    /**
     * Helper method to record the bytes wasted metrics when a download completes.
     * @param name Histogram name
     * @param bytesWasted Bytes wasted during download.
     */
    private void recordBytesWasted(String name, long bytesWasted) {
        RecordHistogram.recordCustomCountHistogram(
                name,
                (int) ConversionUtils.bytesToKilobytes(bytesWasted),
                1,
                ConversionUtils.KILOBYTES_PER_GIGABYTE,
                50);
    }

    /**
     * Used only for android DownloadManager associated downloads.
     * @param item The associated download item.
     * @param showNotification Whether to show notification for this download.
     * @param result The query result about the download.
     */
    public void onQueryCompleted(
            DownloadItem item,
            boolean showNotification,
            DownloadManagerBridge.DownloadQueryResult result) {
        DownloadInfo.Builder builder =
                item.getDownloadInfo() == null
                        ? new DownloadInfo.Builder()
                        : DownloadInfo.Builder.fromDownloadInfo(item.getDownloadInfo());
        builder.setBytesTotalSize(result.bytesTotal);
        builder.setBytesReceived(result.bytesDownloaded);
        if (!TextUtils.isEmpty(result.fileName)) builder.setFileName(result.fileName);
        if (!TextUtils.isEmpty(result.mimeType)) builder.setMimeType(result.mimeType);
        builder.setFilePath(result.filePath);
        item.setDownloadInfo(builder.build());

        if (result.downloadStatus == DownloadStatus.IN_PROGRESS) return;
        if (showNotification) {
            switch (result.downloadStatus) {
                case DownloadStatus.COMPLETE:
                    new AsyncTask<Boolean>() {
                        @Override
                        protected Boolean doInBackground() {
                            return canResolveDownloadItem(
                                    item,
                                    isSupportedMimeType(item.getDownloadInfo().getMimeType()));
                        }

                        @Override
                        protected void onPostExecute(Boolean canResolve) {
                            if (MimeUtils.canAutoOpenMimeType(result.mimeType)
                                    && item.getDownloadInfo().hasUserGesture()
                                    && canResolve) {
                                handleAutoOpenAfterDownload(item);
                            } else {
                                DownloadMessageUiController infoBarController =
                                        getMessageUiController(
                                                item.getDownloadInfo().getOTRProfileId());
                                if (infoBarController != null) {
                                    infoBarController.onItemUpdated(
                                            DownloadItem.createOfflineItem(item), null);
                                }
                            }
                        }
                    }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    break;
                case DownloadStatus.FAILED:
                    onDownloadFailed(item, result.failureReason);
                    break;
                default:
                    break;
            }
        }
    }

    /** Called by tests to disable listening to network connection changes. */
    static void disableNetworkListenerForTest() {
        sIsNetworkListenerDisabled = true;
    }

    /**
     * Called by tests to set the network type.
     *
     * @param isNetworkMetered Whether the network should appear to be metered.
     */
    static void setIsNetworkMeteredForTest(boolean isNetworkMetered) {
        var oldValue = sIsNetworkMetered;
        sIsNetworkMetered = isNetworkMetered;
        ResettersForTesting.register(() -> sIsNetworkMetered = oldValue);
    }

    /**
     * Helper method to add an auto resumable download.
     * @param guid Id of the download item.
     */
    // Deprecated after native auto-resumption handler.
    private void addAutoResumableDownload(String guid) {}

    /**
     * Helper method to remove an auto resumable download.
     * @param guid Id of the download item.
     */
    // Deprecated after native auto-resumption.
    private void removeAutoResumableDownload(String guid) {}

    /**
     * Helper method to remove a download from |mDownloadProgressMap|.
     * @param guid Id of the download item.
     */
    // Deprecated after new download backend.
    private void removeDownloadProgress(String guid) {
        mDownloadProgressMap.remove(guid);
        removeAutoResumableDownload(guid);
        sFirstSeenDownloadIds.remove(guid);
    }

    // Deprecated after native auto resumption.
    static boolean isActiveNetworkMetered(Context context) {
        if (sIsNetworkListenerDisabled) return sIsNetworkMetered;
        ConnectivityManager cm =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        return cm.isActiveNetworkMetered();
    }

    /** Adds a new DownloadObserver to the list. */
    // Deprecated after new download backend.
    public void addDownloadObserver(DownloadObserver observer) {
        mDownloadObservers.addObserver(observer);
        DownloadSharedPreferenceHelper.getInstance().addObserver(observer);
    }

    /** Removes a DownloadObserver from the list. */
    // Deprecated after new download backend.
    public void removeDownloadObserver(DownloadObserver observer) {
        mDownloadObservers.removeObserver(observer);
        DownloadSharedPreferenceHelper.getInstance().removeObserver(observer);
    }

    /**
     * Begins sending back information about all entries in the user's DownloadHistory via
     * {@link #onAllDownloadsRetrieved}.  If the DownloadHistory is not initialized yet, the
     * callback will be delayed.
     *
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     */
    // Deprecated after new download backend.
    public void getAllDownloads(OTRProfileID otrProfileID) {
        DownloadManagerServiceJni.get()
                .getAllDownloads(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
    }

    /**
     * Fires an Intent that alerts the DownloadNotificationService that an action must be taken
     * for a particular item.
     */
    // Deprecated after new download backend.
    public void broadcastDownloadAction(DownloadItem downloadItem, String action) {
        Context appContext = ContextUtils.getApplicationContext();
        Intent intent =
                DownloadNotificationFactory.buildActionIntent(
                        appContext,
                        action,
                        LegacyHelpers.buildLegacyContentId(false, downloadItem.getId()),
                        downloadItem.getDownloadInfo().getOTRProfileId());
        appContext.startService(intent);
    }

    // Deprecated after new download backend.
    public void renameDownload(
            ContentId id,
            String name,
            Callback<Integer /*RenameResult*/> callback,
            OTRProfileID otrProfileID) {
        DownloadManagerServiceJni.get()
                .renameDownload(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        id.id,
                        name,
                        callback,
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
    }

    /**
     * Checks if the files associated with any downloads have been removed by an external action.
     * @param profileKey The {@link ProfileKey} to check the downloads for the the given profile.
     */
    public void checkForExternallyRemovedDownloads(ProfileKey profileKey) {
        DownloadManagerServiceJni.get()
                .checkForExternallyRemovedDownloads(
                        getNativeDownloadManagerService(), DownloadManagerService.this, profileKey);
    }

    // Deprecated after new download backend.
    @CalledByNative
    private List<DownloadItem> createDownloadItemList() {
        return new ArrayList<DownloadItem>();
    }

    // Deprecated after new download backend.
    @CalledByNative
    private void addDownloadItemToList(List<DownloadItem> list, DownloadItem item) {
        list.add(item);
    }

    // Deprecated after new download backend.
    @CalledByNative
    private void onAllDownloadsRetrieved(final List<DownloadItem> list, ProfileKey profileKey) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onAllDownloadsRetrieved(list, profileKey);
        }
        maybeShowMissingSdCardError(list);
    }

    /**
     * Shows a snackbar that tells the user that files may be missing because no SD card was found
     * in the case that the error was not shown before and at least one of the items was
     * externally removed and has a path that points to a missing external drive.
     *
     * @param list  List of DownloadItems to check.
     */
    // TODO(shaktisahu): Drive this from a similar observer.
    private void maybeShowMissingSdCardError(List<DownloadItem> list) {
        PrefService prefService = UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
        // Only show the missing directory snackbar once.
        if (!prefService.getBoolean(Pref.SHOW_MISSING_SD_CARD_ERROR_ANDROID)) return;

        DownloadDirectoryProvider provider = DownloadDirectoryProvider.getInstance();
        provider.getAllDirectoriesOptions(
                (ArrayList<DirectoryOption> dirs) -> {
                    if (dirs.size() > 1) return;
                    String externalStorageDir = provider.getExternalStorageDirectory();

                    for (DownloadItem item : list) {
                        boolean missingOnSDCard =
                                isFilePathOnMissingExternalDrive(
                                        item.getDownloadInfo().getFilePath(),
                                        externalStorageDir,
                                        dirs);
                        if (!isUnresumableOrCancelled(item) && missingOnSDCard) {
                            mHandler.post(
                                    () -> {
                                        // TODO(shaktisahu): Show it on infobar in the right way.
                                        mDownloadSnackbarController.onDownloadDirectoryNotFound();
                                    });
                            prefService.setBoolean(Pref.SHOW_MISSING_SD_CARD_ERROR_ANDROID, false);
                            break;
                        }
                    }
                });
    }

    /**
     * Checks to see if the item is either unresumable or cancelled.
     *
     * @param downloadItem  Item to check.
     * @return              Whether the item is unresumable or cancelled.
     */
    private boolean isUnresumableOrCancelled(DownloadItem downloadItem) {
        @DownloadState int state = downloadItem.getDownloadInfo().state();
        return (state == DownloadState.INTERRUPTED && !downloadItem.getDownloadInfo().isResumable())
                || state == DownloadState.CANCELLED;
    }

    /**
     * Returns whether a given file path is in a directory that is no longer available, most likely
     * because it is on an SD card that was removed.
     *
     * @param filePath  The file path to check, can be a content URI.
     * @param externalStorageDir  The absolute path of external storage directory for primary
     * storage.
     * @param directoryOptions  All available download directories including primary storage and
     * secondary storage.
     *
     * @return          Whether this file path is in a directory that is no longer available.
     */
    private boolean isFilePathOnMissingExternalDrive(
            String filePath,
            String externalStorageDir,
            ArrayList<DirectoryOption> directoryOptions) {
        if (TextUtils.isEmpty(filePath)
                || filePath.contains(externalStorageDir)
                || ContentUriUtils.isContentUri(filePath)) {
            return false;
        }

        for (DirectoryOption directory : directoryOptions) {
            if (TextUtils.isEmpty(directory.location)) continue;
            if (filePath.contains(directory.location)) return false;
        }

        return true;
    }

    // Deprecated after new download backend.
    @CalledByNative
    private void onDownloadItemCreated(DownloadItem item) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onDownloadItemCreated(item);
        }
    }

    // Deprecated after new download backend.
    @CalledByNative
    private void onDownloadItemUpdated(DownloadItem item) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onDownloadItemUpdated(item);
        }
    }

    // Deprecated after new download backend.
    @CalledByNative
    private void onDownloadItemRemoved(
            @JniType("std::string") String guid, OTRProfileID otrProfileID) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onDownloadItemRemoved(guid);
        }
    }

    // Deprecated after new download backend.
    @CalledByNative
    private void openDownloadItem(DownloadItem downloadItem, @DownloadOpenSource int source) {
        DownloadInfo downloadInfo = downloadItem.getDownloadInfo();
        boolean canOpen =
                DownloadUtils.openFile(
                        downloadInfo.getFilePath(),
                        downloadInfo.getMimeType(),
                        downloadInfo.getDownloadGuid(),
                        downloadInfo.getOTRProfileId(),
                        downloadInfo.getOriginalUrl().getSpec(),
                        downloadInfo.getReferrer().getSpec(),
                        source,
                        ContextUtils.getApplicationContext());
        if (!canOpen) {
            openDownloadsPage(downloadInfo.getOTRProfileId(), source);
        }
    }

    /**
     * Opens a download. If the download cannot be opened, download home will be opened instead.
     * @param id The {@link ContentId} of the download to be opened.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param source The source where the user opened this download.
     */
    // Deprecated after new download backend.
    public void openDownload(
            ContentId id, OTRProfileID otrProfileID, @DownloadOpenSource int source) {
        DownloadManagerServiceJni.get()
                .openDownload(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        id.id,
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID),
                        source);
    }

    /**
     * Checks whether the download will be immediately opened after completion.
     *
     * @param downloadItem The download item to be opened.
     */
    public void checkIfDownloadWillAutoOpen(DownloadItem downloadItem, Callback<Boolean> callback) {
        assert (downloadItem.getDownloadInfo().state() == DownloadState.COMPLETE);

        AsyncTask<Boolean> task =
                new AsyncTask<Boolean>() {
                    @Override
                    public Boolean doInBackground() {
                        DownloadInfo info = downloadItem.getDownloadInfo();
                        boolean isSupportedMimeType = isSupportedMimeType(info.getMimeType());
                        boolean canResolve =
                                MimeUtils.isOMADownloadDescription(info.getMimeType())
                                        || canResolveDownloadItem(
                                                downloadItem, isSupportedMimeType);
                        return canResolve
                                && MimeUtils.canAutoOpenMimeType(info.getMimeType())
                                && info.hasUserGesture();
                    }

                    @Override
                    protected void onPostExecute(Boolean result) {
                        callback.onResult(result);
                    }
                };

        try {
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } catch (RejectedExecutionException e) {
            // Reaching thread limit, update will be reschduled for the next run.
            Log.e(TAG, "Thread limit reached, reschedule notification update later.");
        }
    }

    /**
     * Called when a download is canceled before download target is determined.
     *
     * @param item The download item.
     * @param isExternalStorageMissing Whether the reason for failure is missing external storage.
     */
    @CalledByNative
    private static void onDownloadItemCanceled(
            DownloadItem item, boolean isExternalStorageMissing) {
        DownloadManagerService service = getDownloadManagerService();
        int reason =
                isExternalStorageMissing
                        ? DownloadManager.ERROR_DEVICE_NOT_FOUND
                        : DownloadManager.ERROR_FILE_ALREADY_EXISTS;
        service.onDownloadFailed(item, reason);

        // TODO(shaktisahu): Notify infobar controller.
    }

    /**
     * Get the message to display when a download fails.
     *
     * @param fileName Name of the download file.
     * @param reason Reason of failure reported by android DownloadManager.
     */
    private String getDownloadFailureMessage(String fileName, int reason) {
        Context appContext = ContextUtils.getApplicationContext();
        switch (reason) {
            case DownloadManager.ERROR_FILE_ALREADY_EXISTS:
                return appContext.getString(
                        R.string.download_failed_reason_file_already_exists, fileName);
            case DownloadManager.ERROR_FILE_ERROR:
                return appContext.getString(
                        R.string.download_failed_reason_file_system_error, fileName);
            case DownloadManager.ERROR_INSUFFICIENT_SPACE:
                return appContext.getString(
                        R.string.download_failed_reason_insufficient_space, fileName);
            case DownloadManager.ERROR_CANNOT_RESUME:
            case DownloadManager.ERROR_HTTP_DATA_ERROR:
                return appContext.getString(
                        R.string.download_failed_reason_network_failures, fileName);
            case DownloadManager.ERROR_TOO_MANY_REDIRECTS:
            case DownloadManager.ERROR_UNHANDLED_HTTP_CODE:
                return appContext.getString(
                        R.string.download_failed_reason_server_issues, fileName);
            case DownloadManager.ERROR_DEVICE_NOT_FOUND:
                return appContext.getString(
                        R.string.download_failed_reason_storage_not_found, fileName);
            case DownloadManager.ERROR_UNKNOWN:
            default:
                return appContext.getString(
                        R.string.download_failed_reason_unknown_error, fileName);
        }
    }

    /**
     * Returns the SharedPreferences for download retry count.
     * @return The SharedPreferences to use.
     */
    private static SharedPreferences getAutoRetryCountSharedPreference() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(DOWNLOAD_RETRY_COUNT_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Increments the interruption count for a download. If the interruption count reaches a certain
     * threshold, the download will no longer auto resume unless user click the resume button to
     * clear the count.
     *
     * @param downloadGuid Download GUID.
     * @param hasUserGesture Whether the retry is caused by user gesture.
     */
    // Deprecated after new download backend.
    private void incrementDownloadRetryCount(String downloadGuid, boolean hasUserGesture) {
        String name = getDownloadRetryCountSharedPrefName(downloadGuid, hasUserGesture, false);
        incrementDownloadRetrySharedPreferenceCount(name);
        name = getDownloadRetryCountSharedPrefName(downloadGuid, hasUserGesture, true);
        incrementDownloadRetrySharedPreferenceCount(name);
    }

    /**
     * Helper method to increment the retry count for a SharedPreference entry.
     * @param sharedPreferenceName Name of the SharedPreference entry.
     */
    // Deprecated after new download backend.
    private void incrementDownloadRetrySharedPreferenceCount(String sharedPreferenceName) {
        SharedPreferences sharedPrefs = getAutoRetryCountSharedPreference();
        int count = sharedPrefs.getInt(sharedPreferenceName, 0);
        SharedPreferences.Editor editor = sharedPrefs.edit();
        count++;
        editor.putInt(sharedPreferenceName, count);
        editor.apply();
    }

    /**
     * Helper method to retrieve the SharedPreference name for different download retry types.
     * TODO(qinmin): introduce a proto for this and consolidate all the UMA metrics (including
     * retry counts in DownloadHistory) stored in persistent storage.
     * @param downloadGuid Guid of the download.
     * @param hasUserGesture Whether the SharedPreference is for manual retry attempts.
     * @param isTotalCount Whether the SharedPreference is for total retry attempts.
     */
    // Deprecated after new download backend.
    private String getDownloadRetryCountSharedPrefName(
            String downloadGuid, boolean hasUserGesture, boolean isTotalCount) {
        if (isTotalCount) return downloadGuid + DOWNLOAD_TOTAL_RETRY_SUFFIX;
        if (hasUserGesture) return downloadGuid + DOWNLOAD_MANUAL_RETRY_SUFFIX;
        return downloadGuid;
    }

    /**
     * clears the retry count for a download.
     *
     * @param downloadGuid Download GUID.
     * @param isAutoRetryOnly Whether to clear the auto retry count only.
     */
    // Deprecated after new download backend.
    private void clearDownloadRetryCount(String downloadGuid, boolean isAutoRetryOnly) {
        SharedPreferences sharedPrefs = getAutoRetryCountSharedPreference();
        String name = getDownloadRetryCountSharedPrefName(downloadGuid, !isAutoRetryOnly, false);
        int count = Math.min(sharedPrefs.getInt(name, 0), 200);
        assert count >= 0;
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.remove(name);
        if (!isAutoRetryOnly) {
            name = getDownloadRetryCountSharedPrefName(downloadGuid, false, true);
            count = sharedPrefs.getInt(name, 0);
            assert count >= 0;
            editor.remove(name);
        }
        editor.apply();
    }

    // Deprecated after new download backend.
    int getAutoResumptionLimit() {
        if (mAutoResumptionLimit < 0) {
            mAutoResumptionLimit = DownloadManagerServiceJni.get().getAutoResumptionLimit();
        }
        return mAutoResumptionLimit;
    }

    /**
     * Creates an interrupted download in native code to be used by instrumentation tests.
     * @param url URL of the download.
     * @param guid Download GUID.
     * @param targetPath Target file path.
     */
    void createInterruptedDownloadForTest(String url, String guid, String targetPath) {
        DownloadManagerServiceJni.get()
                .createInterruptedDownloadForTest(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        url,
                        guid,
                        targetPath);
    }

    void disableAddCompletedDownloadToDownloadManager() {
        mDisableAddCompletedDownloadForTesting = true;
    }

    /**
     * Updates the last access time of a download.
     * @param downloadGuid Download GUID.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     */
    // Deprecated after new download backend.
    public void updateLastAccessTime(String downloadGuid, OTRProfileID otrProfileID) {
        if (TextUtils.isEmpty(downloadGuid)) return;

        DownloadManagerServiceJni.get()
                .updateLastAccessTime(
                        getNativeDownloadManagerService(),
                        DownloadManagerService.this,
                        downloadGuid,
                        IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID));
    }

    @NativeMethods
    interface Natives {
        boolean isSupportedMimeType(@JniType("std::string") String mimeType);

        int getAutoResumptionLimit();

        long init(DownloadManagerService caller, boolean isProfileAdded);

        void openDownload(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                ProfileKey profileKey,
                int source);

        void resumeDownload(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                ProfileKey profileKey);

        void cancelDownload(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                ProfileKey profileKey);

        void pauseDownload(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                ProfileKey profileKey);

        void removeDownload(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                ProfileKey profileKey);

        void renameDownload(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                @JniType("std::string") String targetName,
                Callback</*RenameResult*/ Integer> callback,
                ProfileKey profileKey);

        void getAllDownloads(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                ProfileKey profileKey);

        void checkForExternallyRemovedDownloads(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                ProfileKey profileKey);

        void updateLastAccessTime(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String downloadGuid,
                ProfileKey profileKey);

        void onProfileAdded(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("Profile*") Profile profile);

        void createInterruptedDownloadForTest(
                long nativeDownloadManagerService,
                DownloadManagerService caller,
                @JniType("std::string") String url,
                @JniType("std::string") String guid,
                @JniType("std::string") String targetPath);
    }
}
