// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.annotation.SuppressLint;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Handler;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadMetrics.DownloadOpenSource;
import org.chromium.chrome.browser.download.ui.BackendProvider;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.download.DownloadState;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.RegistrationPolicyAlwaysRegister;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * Chrome implementation of the {@link DownloadController.DownloadNotificationService} interface.
 * This class is responsible for keeping track of which downloads are in progress. It generates
 * updates for progress of downloads and handles cleaning up of interrupted progress notifications.
 * TODO(qinmin): move BroadcastReceiver inheritance into DownloadManagerDelegate, as it handles all
 * Android DownloadManager interactions. And DownloadManagerService should not know download Id
 * issued by Android DownloadManager.
 */
public class DownloadManagerService
        implements DownloadController.DownloadNotificationService,
                   NetworkChangeNotifierAutoDetect.Observer,
                   DownloadManagerDelegate.DownloadQueryCallback,
                   DownloadManagerDelegate.EnqueueDownloadRequestCallback, DownloadServiceDelegate,
                   BackendProvider.DownloadDelegate, BrowserStartupController.StartupCallback {
    // Download status.
    @IntDef({DownloadStatus.IN_PROGRESS, DownloadStatus.COMPLETE, DownloadStatus.FAILED,
            DownloadStatus.CANCELLED, DownloadStatus.INTERRUPTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DownloadStatus {
        int IN_PROGRESS = 0;
        int COMPLETE = 1;
        int FAILED = 2;
        int CANCELLED = 3;
        int INTERRUPTED = 4;
    }

    private static final String TAG = "DownloadService";
    private static final String DOWNLOAD_DIRECTORY = "Download";
    private static final String UNKNOWN_MIME_TYPE = "application/unknown";
    private static final String DOWNLOAD_UMA_ENTRY = "DownloadUmaEntry";
    private static final String DOWNLOAD_RETRY_COUNT_FILE_NAME = "DownloadRetryCount";
    private static final String DOWNLOAD_MANUAL_RETRY_SUFFIX = ".Manual";
    private static final String DOWNLOAD_TOTAL_RETRY_SUFFIX = ".Total";
    private static final long UPDATE_DELAY_MILLIS = 1000;
    // Wait 10 seconds to resume all downloads, so that we won't impact tab loading.
    private static final long RESUME_DELAY_MILLIS = 10000;
    private static final int UNKNOWN_DOWNLOAD_STATUS = -1;
    public static final long UNKNOWN_BYTES_RECEIVED = -1;
    private static final String PREF_IS_DOWNLOAD_HOME_ENABLED =
            "org.chromium.chrome.browser.download.IS_DOWNLOAD_HOME_ENABLED";

    // Values for the histogram MobileDownloadResumptionCount.
    @IntDef({UmaDownloadResumption.MANUAL_PAUSE, UmaDownloadResumption.BROWSER_KILLED,
            UmaDownloadResumption.CLICKED, UmaDownloadResumption.FAILED,
            UmaDownloadResumption.AUTO_STARTED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface UmaDownloadResumption {
        int MANUAL_PAUSE = 0;
        int BROWSER_KILLED = 1;
        int CLICKED = 2;
        int FAILED = 3;
        int AUTO_STARTED = 4;
        int NUM_ENTRIES = 5;
    }

    // Set will be more expensive to initialize, so use an ArrayList here.
    private static final List<String> MIME_TYPES_TO_OPEN = new ArrayList<String>(Arrays.asList(
            OMADownloadHandler.OMA_DOWNLOAD_DESCRIPTOR_MIME,
            "application/pdf",
            "application/x-x509-ca-cert",
            "application/x-x509-user-cert",
            "application/x-x509-server-cert",
            "application/x-pkcs12",
            "application/application/x-pem-file",
            "application/pkix-cert",
            "application/x-wifi-config"));

    private static final Set<String> sFirstSeenDownloadIds = new HashSet<String>();

    private static DownloadManagerService sDownloadManagerService;
    private static boolean sIsNetworkListenerDisabled;
    private static boolean sIsNetworkMetered;

    private final SharedPreferences mSharedPrefs;
    private final HashMap<String, DownloadProgress> mDownloadProgressMap =
            new HashMap<String, DownloadProgress>(4, 0.75f);

    private final DownloadNotifier mDownloadNotifier;
    // Delay between UI updates.
    private final long mUpdateDelayInMillis;

    private final Handler mHandler;

    /** Generic interface for notifying external UI components about downloads and their states. */
    public interface DownloadObserver extends DownloadSharedPreferenceHelper.Observer {
        /** Called in response to {@link DownloadManagerService#getAllDownloads(boolean)}. */
        void onAllDownloadsRetrieved(final List<DownloadItem> list, boolean isOffTheRecord);

        /** Called when a download is created. */
        void onDownloadItemCreated(DownloadItem item);

        /** Called when a download is updated. */
        void onDownloadItemUpdated(DownloadItem item);

        /** Called when a download has been removed. */
        void onDownloadItemRemoved(String guid, boolean isOffTheRecord);
    }

    @VisibleForTesting protected final List<String> mAutoResumableDownloadIds =
            new ArrayList<String>();
    private final List<DownloadUmaStatsEntry> mUmaEntries = new ArrayList<DownloadUmaStatsEntry>();
    private final ObserverList<DownloadObserver> mDownloadObservers = new ObserverList<>();

    private OMADownloadHandler mOMADownloadHandler;
    private DownloadSnackbarController mDownloadSnackbarController;
    private DownloadInfoBarController mInfoBarController;
    private DownloadInfoBarController mIncognitoInfoBarController;
    private long mNativeDownloadManagerService;
    private DownloadManagerDelegate mDownloadManagerDelegate;
    private NetworkChangeNotifierAutoDetect mNetworkChangeNotifier;
    // Flag to track if we need to post a task to update download notifications.
    private boolean mIsUIUpdateScheduled;
    private int mAutoResumptionLimit = -1;
    private DownloadManagerRequestInterceptor mDownloadManagerRequestInterceptor;

    // Whether any ChromeActivity is launched.
    private boolean mActivityLaunched;

    /**
     * Interface to intercept download request to Android DownloadManager. This is implemented by
     * tests so that we don't need to actually enqueue a download into the Android DownloadManager.
     */
    static interface DownloadManagerRequestInterceptor {
        void interceptDownloadRequest(DownloadItem item, boolean notifyComplete);
    }

    /**
     * Class representing progress of a download.
     */
    private static class DownloadProgress {
        final long mStartTimeInMillis;
        boolean mCanDownloadWhileMetered;
        DownloadItem mDownloadItem;
        @DownloadStatus
        int mDownloadStatus;
        boolean mIsAutoResumable;
        boolean mIsUpdated;
        boolean mIsSupportedMimeType;

        DownloadProgress(long startTimeInMillis, boolean canDownloadWhileMetered,
                DownloadItem downloadItem, @DownloadStatus int downloadStatus) {
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

    /**
     * Creates DownloadManagerService.
     */
    public static DownloadManagerService getDownloadManagerService() {
        ThreadUtils.assertOnUiThread();
        if (sDownloadManagerService == null) {
            DownloadNotifier downloadNotifier = new SystemDownloadNotifier2();
            sDownloadManagerService = new DownloadManagerService(
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
        mSharedPrefs = ContextUtils.getAppSharedPreferences();
        // Clean up unused shared prefs. TODO(qinmin): remove this after M61.
        mSharedPrefs.edit().remove(PREF_IS_DOWNLOAD_HOME_ENABLED).apply();
        mDownloadNotifier = downloadNotifier;
        mUpdateDelayInMillis = updateDelayInMillis;
        mHandler = handler;
        mDownloadSnackbarController = new DownloadSnackbarController();
        mDownloadManagerDelegate = new DownloadManagerDelegate(applicationContext);
        mOMADownloadHandler = new OMADownloadHandler(
                applicationContext, mDownloadManagerDelegate, mDownloadSnackbarController);
        // Note that this technically leaks the native object, however, DownloadManagerService
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android.
        init();
        mOMADownloadHandler.clearPendingOMADownloads();
    }

    @VisibleForTesting
    protected void init() {
        DownloadController.setDownloadNotificationService(this);
        // Post a delayed task to resume all pending downloads.
        mHandler.postDelayed(() -> mDownloadNotifier.resumePendingDownloads(), RESUME_DELAY_MILLIS);
        parseUMAStatsEntriesFromSharedPrefs();
        Iterator<DownloadUmaStatsEntry> iterator = mUmaEntries.iterator();
        boolean hasChanges = false;
        while (iterator.hasNext()) {
            DownloadUmaStatsEntry entry = iterator.next();
            if (entry.useDownloadManager) {
                mDownloadManagerDelegate.queryDownloadResult(
                        entry.buildDownloadItem(), false, this);
            } else if (!entry.isPaused) {
                entry.isPaused = true;
                entry.numInterruptions++;
                hasChanges = true;
            }
        }
        if (hasChanges) storeUmaEntries();
    }

    /**
     * Pre-load shared prefs to avoid being blocked on the disk access async task in the future.
     */
    public static void warmUpSharedPrefs() {
        getAutoRetryCountSharedPreference();
    }

    public DownloadNotifier getDownloadNotifier() {
        return mDownloadNotifier;
    }

    /** @return The {@link DownloadInfoBarController} controller associated with the profile. */
    public DownloadInfoBarController getInfoBarController(boolean isIncognito) {
        return isIncognito ? mIncognitoInfoBarController : mInfoBarController;
    }

    @Override
    public void onDownloadCompleted(final DownloadInfo downloadInfo) {
        @DownloadStatus
        int status = DownloadStatus.COMPLETE;
        String mimeType = downloadInfo.getMimeType();
        if (downloadInfo.getBytesReceived() == 0) {
            status = DownloadStatus.FAILED;
        } else {
            String origMimeType = mimeType;
            if (TextUtils.isEmpty(origMimeType)) origMimeType = UNKNOWN_MIME_TYPE;
            mimeType = ChromeDownloadDelegate.remapGenericMimeType(
                    origMimeType, downloadInfo.getOriginalUrl(), downloadInfo.getFileName());
        }
        DownloadInfo newInfo =
                DownloadInfo.Builder.fromDownloadInfo(downloadInfo).setMimeType(mimeType).build();
        DownloadItem downloadItem = new DownloadItem(false, newInfo);
        updateDownloadProgress(downloadItem, status);
    }

    @Override
    public void onDownloadUpdated(final DownloadInfo downloadInfo) {
        DownloadItem item = new DownloadItem(false, downloadInfo);
        // If user manually paused a download, this download is no longer auto resumable.
        if (downloadInfo.isPaused()) {
            removeAutoResumableDownload(item.getId());
        }
        updateDownloadProgress(item, DownloadStatus.IN_PROGRESS);
        scheduleUpdateIfNeeded();
    }

    @Override
    public void onDownloadCancelled(final DownloadInfo downloadInfo) {
        DownloadItem item = new DownloadItem(false, downloadInfo);
        removeAutoResumableDownload(item.getId());
        updateDownloadProgress(new DownloadItem(false, downloadInfo), DownloadStatus.CANCELLED);
    }

    @Override
    public void onDownloadInterrupted(final DownloadInfo downloadInfo, boolean isAutoResumable) {
        @DownloadStatus
        int status = DownloadStatus.INTERRUPTED;
        DownloadItem item = new DownloadItem(false, downloadInfo);
        if (!downloadInfo.isResumable()) {
            status = DownloadStatus.FAILED;
        } else if (isAutoResumable) {
            addAutoResumableDownload(item.getId());
        }
        updateDownloadProgress(item, status);

        DownloadProgress progress = mDownloadProgressMap.get(item.getId());
        if (progress == null) return;
        if (!isAutoResumable || sIsNetworkListenerDisabled) return;
        ConnectivityManager cm =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null || !info.isConnected()) return;
        if (progress.mCanDownloadWhileMetered
                || !isActiveNetworkMetered(ContextUtils.getApplicationContext())) {
            // Normally the download will automatically resume when network is reconnected.
            // However, if there are multiple network connections and the interruption is caused
            // by switching between active networks, onConnectionTypeChanged() will not get called.
            // As a result, we should resume immediately.
            scheduleDownloadResumption(item);
        }
    }

    /**
     * Helper method to schedule a download for resumption.
     * @param item DownloadItem to resume.
     */
    private void scheduleDownloadResumption(final DownloadItem item) {
        removeAutoResumableDownload(item.getId());
        // Post a delayed task to avoid an issue that when connectivity status just changed
        // to CONNECTED, immediately establishing a connection will sometimes fail.
        mHandler.postDelayed(
                () -> resumeDownload(LegacyHelpers.buildLegacyContentId(false, item.getId()),
                        item, false), mUpdateDelayInMillis);
    }

    /**
     * Called when browser activity is launched. For background resumption and cancellation, this
     * will not be called.
     */
    public void onActivityLaunched() {
        if (!mActivityLaunched) {
            mInfoBarController = new DownloadInfoBarController(false);
            mIncognitoInfoBarController = new DownloadInfoBarController(true);

            DownloadNotificationService2.clearResumptionAttemptLeft();

            DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                    /*isOffTheRecord=*/false);
            mActivityLaunched = true;
        }
    }

    /**
     * Broadcast that a download was successful.
     * @param downloadInfo info about the download.
     */
    protected void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {}

    /**
     * Gets download information from SharedPreferences.
     * @param sharedPrefs The SharedPreferences object to parse.
     * @param type Type of the information to retrieve.
     * @return download information saved to the SharedPrefs for the given type.
     */
    @VisibleForTesting
    protected static Set<String> getStoredDownloadInfo(SharedPreferences sharedPrefs, String type) {
        return new HashSet<String>(sharedPrefs.getStringSet(type, new HashSet<String>()));
    }

    /**
     * Stores download information to shared preferences. The information can be
     * either pending download IDs, or pending OMA downloads.
     *
     * @param sharedPrefs   SharedPreferences to update.
     * @param type          Type of the information.
     * @param downloadInfo  Information to be saved.
     * @param forceCommit   Whether to synchronously update shared preferences.
     */
    @SuppressLint({"ApplySharedPref", "CommitPrefEdits"})
    static void storeDownloadInfo(SharedPreferences sharedPrefs, String type,
            Set<String> downloadInfo, boolean forceCommit) {
        SharedPreferences.Editor editor = sharedPrefs.edit();
        if (downloadInfo.isEmpty()) {
            editor.remove(type);
        } else {
            editor.putStringSet(type, downloadInfo);
        }

        if (forceCommit) {
            // Write synchronously because it might be used on restart and needs to stay up-to-date.
            if (!editor.commit()) {
                Log.e(TAG, "Failed to write DownloadInfo " + type);
            }
        } else {
            editor.apply();
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
                onDownloadFailed(item, DownloadManager.ERROR_UNKNOWN);
                break;
            case DownloadStatus.IN_PROGRESS:
                if (info.isPaused()) {
                    mDownloadNotifier.notifyDownloadPaused(info);
                    recordDownloadResumption(UmaDownloadResumption.MANUAL_PAUSE);
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

    /**
     * Helper method to schedule a task to update the download success notification.
     * @param progress Download progress to update.
     * @return True if the task can be scheduled, or false otherwise.
     */
    private boolean updateDownloadSuccessNotification(DownloadProgress progress) {
        final boolean isSupportedMimeType = progress.mIsSupportedMimeType;
        final DownloadItem item = progress.mDownloadItem;
        AsyncTask<Pair<Long, Boolean>> task = new AsyncTask<Pair<Long, Boolean>>() {
            @Override
            public Pair<Long, Boolean> doInBackground() {
                boolean success = addCompletedDownload(item);
                boolean canResolve = success
                        && (isOMADownloadDescription(item.getDownloadInfo())
                                   || canResolveDownloadItem(ContextUtils.getApplicationContext(),
                                              item, isSupportedMimeType));
                return Pair.create(item.getSystemDownloadId(), canResolve);
            }

            @Override
            protected void onPostExecute(Pair<Long, Boolean> result) {
                DownloadInfo info = item.getDownloadInfo();
                if (result.first != DownloadItem.INVALID_DOWNLOAD_ID) {
                    mDownloadNotifier.notifyDownloadSuccessful(
                            info, result.first, result.second, isSupportedMimeType);
                    broadcastDownloadSuccessful(info);
                } else {
                    info = DownloadInfo.Builder.fromDownloadInfo(info)
                                   .setFailState(FailState.CANNOT_DOWNLOAD)
                                   .build();
                    mDownloadNotifier.notifyDownloadFailed(info);
                    // TODO(qinmin): get the failure message from native.
                    onDownloadFailed(item, DownloadManager.ERROR_UNKNOWN);
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

    /**
     * Adds a completed download into Android DownloadManager.
     *
     * @param downloadItem Information of the downloaded.
     * @return true if the download is added to the DownloadManager, or false otherwise.
     */
    protected boolean addCompletedDownload(DownloadItem downloadItem) {
        assert !ThreadUtils.runningOnUiThread();
        DownloadInfo downloadInfo = downloadItem.getDownloadInfo();
        String description = downloadInfo.getDescription();
        if (TextUtils.isEmpty(description)) description = downloadInfo.getFileName();
        try {
            // Exceptions can be thrown when calling this, although it is not
            // documented on Android SDK page.
            long downloadId = mDownloadManagerDelegate.addCompletedDownload(
                    downloadInfo.getFileName(), description, downloadInfo.getMimeType(),
                    downloadInfo.getFilePath(), downloadInfo.getBytesReceived(),
                    downloadInfo.getOriginalUrl(), downloadInfo.getReferrer(),
                    downloadInfo.getDownloadGuid());
            downloadItem.setSystemDownloadId(downloadId);
            return true;
        } catch (RuntimeException e) {
            Log.w(TAG, "Failed to add the download item to DownloadManager: ", e);
            if (downloadInfo.getFilePath() != null) {
                File file = new File(downloadInfo.getFilePath());
                if (!file.delete()) {
                    Log.w(TAG, "Failed to remove the unsuccessful download");
                }
            }
        }
        return false;
    }

    /**
     * Handle auto opennable files after download completes.
     * TODO(qinmin): move this to DownloadManagerDelegate.
     *
     * @param download A download item.
     */
    private void handleAutoOpenAfterDownload(DownloadItem download) {
        if (isOMADownloadDescription(download.getDownloadInfo())) {
            mOMADownloadHandler.handleOMADownload(
                    download.getDownloadInfo(), download.getSystemDownloadId());
            return;
        }
        openDownloadedContent(download.getDownloadInfo(), download.getSystemDownloadId(),
                DownloadMetrics.DownloadOpenSource.AUTO_OPEN);
    }

    /**
     * Schedule an update if there is no update scheduled.
     */
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

        Runnable scheduleNextUpdateTask = () -> {
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
    private void updateDownloadProgress(
            DownloadItem downloadItem, @DownloadStatus int downloadStatus) {
        boolean isSupportedMimeType = downloadStatus == DownloadStatus.COMPLETE
                && isSupportedMimeType(downloadItem.getDownloadInfo().getMimeType());
        String id = downloadItem.getId();
        DownloadProgress progress = mDownloadProgressMap.get(id);
        long bytesReceived = downloadItem.getDownloadInfo().getBytesReceived();
        if (progress == null) {
            if (!downloadItem.getDownloadInfo().isPaused()) {
                long startTime = System.currentTimeMillis();
                progress = new DownloadProgress(startTime,
                        isActiveNetworkMetered(ContextUtils.getApplicationContext()), downloadItem,
                        downloadStatus);
                progress.mIsUpdated = true;
                progress.mIsSupportedMimeType = isSupportedMimeType;
                mDownloadProgressMap.put(id, progress);
                sFirstSeenDownloadIds.add(id);
                DownloadUmaStatsEntry entry = getUmaStatsEntry(downloadItem.getId());
                if (entry == null) {
                    addUmaStatsEntry(new DownloadUmaStatsEntry(downloadItem.getId(), startTime,
                            downloadStatus == DownloadStatus.INTERRUPTED ? 1 : 0, false, false,
                            bytesReceived, 0));
                } else if (updateBytesReceived(entry, bytesReceived)) {
                    storeUmaEntries();
                }

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
        DownloadUmaStatsEntry entry;
        switch (downloadStatus) {
            case DownloadStatus.COMPLETE:
            case DownloadStatus.FAILED:
            case DownloadStatus.CANCELLED:
                recordDownloadFinishedUMA(
                        downloadStatus, id, downloadItem.getDownloadInfo().getBytesReceived());
                clearDownloadRetryCount(id, true);
                clearDownloadRetryCount(id, false);
                updateNotification(progress);
                sFirstSeenDownloadIds.remove(id);
                break;
            case DownloadStatus.INTERRUPTED:
                entry = getUmaStatsEntry(id);
                entry.numInterruptions++;
                updateBytesReceived(entry, bytesReceived);
                storeUmaEntries();
                updateNotification(progress);
                break;
            case DownloadStatus.IN_PROGRESS:
                entry = getUmaStatsEntry(id);
                if (entry.isPaused != downloadItem.getDownloadInfo().isPaused()
                        || updateBytesReceived(entry, bytesReceived)) {
                    entry.isPaused = downloadItem.getDownloadInfo().isPaused();
                    storeUmaEntries();
                }

                if (downloadItem.getDownloadInfo().isPaused()) {
                    updateNotification(progress);
                }
                break;
            default:
                assert false;
        }
    }

    /**
     * Helper method to update the received bytes and wasted bytes for UMA reporting.
     * @param  entry UMA entry to update.
     * @param bytesReceived The current received bytes.
     * @return true if the UMA stats is updated, or false otherwise.
     */
    private boolean updateBytesReceived(DownloadUmaStatsEntry entry, long bytesReceived) {
        if (bytesReceived == UNKNOWN_BYTES_RECEIVED || bytesReceived == entry.lastBytesReceived) {
            return false;
        }
        if (bytesReceived < entry.lastBytesReceived) {
            entry.bytesWasted += entry.lastBytesReceived - bytesReceived;
        }
        entry.lastBytesReceived = bytesReceived;
        return true;
    }
    /**
     * Sets the download handler for OMA downloads, for testing purpose.
     *
     * @param omaDownloadHandler Download handler for OMA contents.
     */
    @VisibleForTesting
    protected void setOMADownloadHandler(OMADownloadHandler omaDownloadHandler) {
        mOMADownloadHandler = omaDownloadHandler;
    }

    /** See {@link DownloadManagerDelegate.EnqueueDownloadRequestTask}. */
    public void enqueueDownloadManagerRequest(final DownloadItem item, boolean notifyCompleted) {
        if (mDownloadManagerRequestInterceptor != null) {
            mDownloadManagerRequestInterceptor.interceptDownloadRequest(item, notifyCompleted);
            return;
        }

        mDownloadManagerDelegate.enqueueDownloadManagerRequest(item, notifyCompleted, this);
    }

    @Override
    public void onDownloadEnqueued(
            boolean result, int failureReason, DownloadItem downloadItem, long downloadId) {
        if (!result) {
            onDownloadFailed(downloadItem, failureReason);
            recordDownloadCompletionStats(
                    true, DownloadManagerService.DownloadStatus.FAILED, 0, 0, 0, 0);
            return;
        }

        DownloadUtils.showDownloadStartToast(ContextUtils.getApplicationContext());
        addUmaStatsEntry(new DownloadUmaStatsEntry(
                String.valueOf(downloadId), downloadItem.getStartTime(), 0, false, true, 0, 0));
    }

    /**
     * Determines if the download should be immediately opened after
     * downloading.
     *
     * @param downloadInfo Information about the download.
     * @return true if the downloaded content should be opened, or false otherwise.
     */
    @VisibleForTesting
    static boolean shouldOpenAfterDownload(DownloadInfo downloadInfo) {
        String type = downloadInfo.getMimeType();
        return downloadInfo.hasUserGesture() && MIME_TYPES_TO_OPEN.contains(type);
    }

    /**
     * Returns true if the download is for OMA download description file.
     *
     * @param downloadInfo Information about the download.
     * @return true if the downloaded is OMA download description, or false otherwise.
     */
    static boolean isOMADownloadDescription(DownloadInfo downloadInfo) {
        return OMADownloadHandler.OMA_DOWNLOAD_DESCRIPTOR_MIME.equalsIgnoreCase(
                downloadInfo.getMimeType());
    }

    /**
     * Return the intent to launch for a given download item.
     *
     * @param context    Context of the app.
     * @param filePath   Path to the file.
     * @param downloadId ID of the download item in DownloadManager.
     * @param isSupportedMimeType Whether the MIME type is supported by browser.
     * @param downloadId ID of the download item in DownloadManager.
     * @param originalUrl The original url of the downloaded file
     * @param referrer   Referrer of the downloaded file.
     * @return the intent to launch for the given download item.
     */
    @Nullable
    static Intent getLaunchIntentFromDownloadId(
            Context context, @Nullable String filePath, long downloadId,
            boolean isSupportedMimeType, String originalUrl, String referrer) {
        assert !ThreadUtils.runningOnUiThread();
        Uri contentUri = filePath == null
                ? DownloadManagerDelegate.getContentUriFromDownloadManager(context, downloadId)
                : ApiCompatibilityUtils.getUriForDownloadedFile(new File(filePath));
        if (contentUri == null) return null;

        DownloadManager manager =
                (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
        String mimeType = manager.getMimeTypeForDownloadedFile(downloadId);
        if (isSupportedMimeType) {
            // Redirect the user to an internal media viewer.  The file path is necessary to show
            // the real file path to the user instead of a content:// download ID.
            Uri fileUri = contentUri;
            if (filePath != null) fileUri = Uri.fromFile(new File(filePath));
            return MediaViewerUtils.getMediaViewerIntent(
                    fileUri, contentUri, mimeType, true /* allowExternalAppHandlers */);
        }
        return MediaViewerUtils.createViewIntentForUri(contentUri, mimeType, originalUrl, referrer);
    }

    /**
     * Return whether a download item can be resolved to any activity.
     *
     * @param context Context of the app.
     * @param download A download item.
     * @param isSupportedMimeType Whether the MIME type is supported by browser.
     * @return true if the download item can be resolved, or false otherwise.
     */
    static boolean canResolveDownloadItem(Context context, DownloadItem download,
            boolean isSupportedMimeType) {
        assert !ThreadUtils.runningOnUiThread();
        Intent intent = getLaunchIntentFromDownloadId(
                context, download.getDownloadInfo().getFilePath(),
                download.getSystemDownloadId(), isSupportedMimeType, null, null);
        return (intent == null)
                ? false : ExternalNavigationDelegateImpl.resolveIntent(intent, true);
    }

    /** See {@link #openDownloadedContent(Context, String, boolean, boolean, String, long)}. */
    protected void openDownloadedContent(final DownloadInfo downloadInfo, final long downloadId,
            @DownloadOpenSource int source) {
        openDownloadedContent(ContextUtils.getApplicationContext(), downloadInfo.getFilePath(),
                isSupportedMimeType(downloadInfo.getMimeType()), downloadInfo.isOffTheRecord(),
                downloadInfo.getDownloadGuid(), downloadId, downloadInfo.getOriginalUrl(),
                downloadInfo.getReferrer(), source);
    }

    /**
     * Launch the intent for a given download item, or Download Home if that's not possible.
     * TODO(qinmin): Move this to DownloadManagerDelegate.
     *
     * @param context             Context to use.
     * @param filePath            Path to the downloaded item.
     * @param isSupportedMimeType MIME type of the downloaded item.
     * @param isOffTheRecord      Whether the download was for a off the record profile.
     * @param downloadGuid        GUID of the download item in DownloadManager.
     * @param downloadId          ID of the download item in DownloadManager.
     * @param originalUrl         The original url of the downloaded file.
     * @param referrer            Referrer of the downloaded file.
     * @param source              The source that tries to open the download.
     */
    protected static void openDownloadedContent(final Context context, final String filePath,
            final boolean isSupportedMimeType, final boolean isOffTheRecord,
            final String downloadGuid, final long downloadId, final String originalUrl,
            final String referrer, @DownloadOpenSource int source) {
        new AsyncTask<Intent>() {
            @Override
            public Intent doInBackground() {
                return getLaunchIntentFromDownloadId(
                        context, filePath, downloadId, isSupportedMimeType, originalUrl, referrer);
            }

            @Override
            protected void onPostExecute(Intent intent) {
                boolean didLaunchIntent = intent != null
                        && ExternalNavigationDelegateImpl.resolveIntent(intent, true)
                        && DownloadUtils.fireOpenIntentForDownload(context, intent);

                if (!didLaunchIntent) {
                    openDownloadsPage(context);
                    return;
                }

                if (didLaunchIntent && hasDownloadManagerService()) {
                    DownloadManagerService.getDownloadManagerService().updateLastAccessTime(
                            downloadGuid, isOffTheRecord);
                    DownloadManager manager =
                            (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
                    String mimeType = manager.getMimeTypeForDownloadedFile(downloadId);
                    DownloadMetrics.recordDownloadOpen(source, mimeType);
                }
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Called when a download fails.
     *
     * @param fileName Name of the download file.
     * @param reason Reason of failure reported by android DownloadManager
     */
    @VisibleForTesting
    protected void onDownloadFailed(DownloadItem item, int reason) {
        String failureMessage =
                getDownloadFailureMessage(item.getDownloadInfo().getFileName(), reason);
        // TODO(shaktisahu): Notify infobar controller of the failure.
        if (FeatureUtilities.isDownloadProgressInfoBarEnabled()) return;

        if (mDownloadSnackbarController.getSnackbarManager() != null) {
            mDownloadSnackbarController.onDownloadFailed(
                    failureMessage,
                    reason == DownloadManager.ERROR_FILE_ALREADY_EXISTS);
        } else {
            Toast.makeText(ContextUtils.getApplicationContext(), failureMessage, Toast.LENGTH_SHORT)
                    .show();
        }
    }

    /**
     * Set the DownloadSnackbarController for testing purpose.
     */
    @VisibleForTesting
    protected void setDownloadSnackbarController(
            DownloadSnackbarController downloadSnackbarController) {
        mDownloadSnackbarController = downloadSnackbarController;
    }

    /**
     * Open the Activity which shows a list of all downloads.
     * @param context Application context
     */
    public static void openDownloadsPage(Context context) {
        if (DownloadUtils.showDownloadManager(null, null)) return;

        // Open the Android Download Manager.
        Intent pageView = new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS);
        pageView.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(pageView);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find Downloads app", e);
        }
    }

    @Override
    public void resumeDownload(ContentId id, DownloadItem item, boolean hasUserGesture) {
        DownloadProgress progress = mDownloadProgressMap.get(item.getId());
        if (progress != null && progress.mDownloadStatus == DownloadStatus.IN_PROGRESS
                && !progress.mDownloadItem.getDownloadInfo().isPaused()) {
            // Download already in progress, do nothing
            return;
        }
        int uma =
                hasUserGesture ? UmaDownloadResumption.CLICKED : UmaDownloadResumption.AUTO_STARTED;
        recordDownloadResumption(uma);
        if (progress == null) {
            assert !item.getDownloadInfo().isPaused();
            // If the download was not resumed before, the browser must have been killed while the
            // download is active.
            if (!sFirstSeenDownloadIds.contains(item.getId())) {
                sFirstSeenDownloadIds.add(item.getId());
                recordDownloadResumption(UmaDownloadResumption.BROWSER_KILLED);
            }
            updateDownloadProgress(item, DownloadStatus.IN_PROGRESS);
            progress = mDownloadProgressMap.get(item.getId());
        }
        if (hasUserGesture) {
            // If user manually resumes a download, update the connection type that the download
            // can start. If the previous connection type is metered, manually resuming on an
            // unmetered network should not affect the original connection type.
            if (!progress.mCanDownloadWhileMetered) {
                progress.mCanDownloadWhileMetered =
                        isActiveNetworkMetered(ContextUtils.getApplicationContext());
            }
            incrementDownloadRetryCount(item.getId(), true);
            clearDownloadRetryCount(item.getId(), true);
        } else {
            // TODO(qinmin): Consolidate this logic with the logic in notification service that
            // throttles browser restarts.
            SharedPreferences sharedPrefs = getAutoRetryCountSharedPreference();
            int count = sharedPrefs.getInt(item.getId(), 0);
            if (count >= getAutoResumptionLimit()) {
                removeAutoResumableDownload(item.getId());
                onDownloadInterrupted(item.getDownloadInfo(), false);
                return;
            }
            incrementDownloadRetryCount(item.getId(), false);
        }
        nativeResumeDownload(getNativeDownloadManagerService(), item.getId(),
                item.getDownloadInfo().isOffTheRecord());
    }

    /**
     * Called to cancel a download.
     * @param id The {@link ContentId} of the download to cancel.
     * @param isOffTheRecord Whether the download is off the record.
     */
    @Override
    public void cancelDownload(ContentId id, boolean isOffTheRecord) {
        nativeCancelDownload(getNativeDownloadManagerService(), id.id, isOffTheRecord);
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
        recordDownloadFinishedUMA(DownloadStatus.CANCELLED, id.id, 0);
    }

    /**
     * Called to pause a download.
     * @param id The {@link ContentId} of the download to pause.
     * @param isOffTheRecord Whether the download is off the record.
     */
    @Override
    public void pauseDownload(ContentId id, boolean isOffTheRecord) {
        nativePauseDownload(getNativeDownloadManagerService(), id.id, isOffTheRecord);
        DownloadProgress progress = mDownloadProgressMap.get(id.id);
        // Calling pause will stop listening to the download item. Update its progress now.
        // If download is already completed, canceled or failed, there is no need to update the
        // download notification.
        if (progress != null
                && (progress.mDownloadStatus == DownloadStatus.INTERRUPTED
                           || progress.mDownloadStatus == DownloadStatus.IN_PROGRESS)) {
            DownloadInfo info = DownloadInfo.Builder.fromDownloadInfo(
                    progress.mDownloadItem.getDownloadInfo()).setIsPaused(true)
                    .setBytesReceived(UNKNOWN_BYTES_RECEIVED).build();
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
     * @param isOffTheRecord Whether the download is off the record.
     * @param externallyRemoved If the file is externally removed by other applications.
     */
    @Override
    public void removeDownload(
            final String downloadGuid, boolean isOffTheRecord, boolean externallyRemoved) {
        mHandler.post(() -> {
            nativeRemoveDownload(getNativeDownloadManagerService(), downloadGuid, isOffTheRecord);
            removeDownloadProgress(downloadGuid);
        });

        new AsyncTask<Void>() {
            @Override
            public Void doInBackground() {
                mDownloadManagerDelegate.removeCompletedDownload(downloadGuid, externallyRemoved);
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Checks whether the download can be opened by the browser.
     * @param isOffTheRecord Whether the download is off the record.
     * @param mimeType MIME type of the file.
     * @return Whether the download is openable by the browser.
     */
    @Override
    public boolean isDownloadOpenableInBrowser(boolean isOffTheRecord, String mimeType) {
        // TODO(qinmin): for audio and video, check if the codec is supported by Chrome.
        return isSupportedMimeType(mimeType);
    }

    /**
     * Checks whether a file with the given MIME type can be opened by the browser.
     * @param mimeType MIME type of the file.
     * @return Whether the file would be openable by the browser.
     */
    public static boolean isSupportedMimeType(String mimeType) {
        return nativeIsSupportedMimeType(mimeType);
    }

    /**
     * Helper method to create and retrieve the native DownloadManagerService when needed.
     * @return pointer to native DownloadManagerService.
     */
    private long getNativeDownloadManagerService() {
        if (mNativeDownloadManagerService == 0) {
            boolean startupCompleted =
                    BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isStartupSuccessfullyCompleted();
            mNativeDownloadManagerService = nativeInit(startupCompleted);
            if (!startupCompleted) {
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .addStartupCompletedObserver(this);
            }
        }
        return mNativeDownloadManagerService;
    }

    @Override
    public void onSuccess() {
        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()) {
            nativeOnFullBrowserStarted(mNativeDownloadManagerService);
        }
    }

    @Override
    public void onFailure() {}

    @CalledByNative
    void onResumptionFailed(String downloadGuid) {
        mDownloadNotifier.notifyDownloadFailed(new DownloadInfo.Builder()
                                                       .setDownloadGuid(downloadGuid)
                                                       .setFailState(FailState.CANNOT_DOWNLOAD)
                                                       .build());
        removeDownloadProgress(downloadGuid);
        recordDownloadResumption(UmaDownloadResumption.FAILED);
        recordDownloadFinishedUMA(DownloadStatus.FAILED, downloadGuid, 0);
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
        if (canResolve && shouldOpenAfterDownload(info)) {
            DownloadItem item = new DownloadItem(false, info);
            item.setSystemDownloadId(systemDownloadId);
            handleAutoOpenAfterDownload(item);
        } else {
            DownloadInfoBarController infobarController =
                    getInfoBarController(info.isOffTheRecord());
            if (infobarController != null) {
                infobarController.onNotificationShown(info.getContentId(), notificationId);
            }
            mDownloadSnackbarController.onDownloadSucceeded(
                    info, notificationId, systemDownloadId, canResolve, false);
        }

        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()) {
            Profile profile = info.isOffTheRecord()
                    ? Profile.getLastUsedProfile().getOffTheRecordProfile()
                    : Profile.getLastUsedProfile().getOriginalProfile();
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.notifyEvent(EventConstants.DOWNLOAD_COMPLETED);
        }
    }

    /**
     * Helper method to record the download resumption UMA.
     * @param type UMA type to be recorded.
     */
    private void recordDownloadResumption(@UmaDownloadResumption int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.DownloadResumption", type, UmaDownloadResumption.NUM_ENTRIES);
    }

    /**
     * Helper method to record the metrics when a download completes.
     * @param useDownloadManager Whether the download goes through Android DownloadManager.
     * @param status Download completion status.
     * @param totalDuration Total time in milliseconds to download the file.
     * @param bytesDownloaded Total bytes downloaded.
     * @param numInterruptions Number of interruptions during the download.
     */
    private void recordDownloadCompletionStats(boolean useDownloadManager, int status,
            long totalDuration, long bytesDownloaded, int numInterruptions, long bytesWasted) {
        switch (status) {
            case DownloadStatus.COMPLETE:
                if (useDownloadManager) {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.DownloadManager.Success",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.DownloadManager.Success",
                            (int) ConversionUtils.bytesToKilobytes(bytesDownloaded));
                } else {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.ChromeNetworkStack.Success",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.ChromeNetworkStack.Success",
                            (int) ConversionUtils.bytesToKilobytes(bytesDownloaded));
                    RecordHistogram.recordCountHistogram(
                            "MobileDownload.InterruptionsCount.ChromeNetworkStack.Success",
                            numInterruptions);
                    recordBytesWasted(
                            "MobileDownload.BytesWasted.ChromeNetworkStack.Success", bytesWasted);
                }
                break;
            case DownloadStatus.FAILED:
                if (useDownloadManager) {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.DownloadManager.Failure",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.DownloadManager.Failure",
                            (int) ConversionUtils.bytesToKilobytes(bytesDownloaded));
                } else {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.ChromeNetworkStack.Failure",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCount1000Histogram(
                            "MobileDownload.BytesDownloaded.ChromeNetworkStack.Failure",
                            (int) ConversionUtils.bytesToKilobytes(bytesDownloaded));
                    RecordHistogram.recordCountHistogram(
                            "MobileDownload.InterruptionsCount.ChromeNetworkStack.Failure",
                            numInterruptions);
                    recordBytesWasted(
                            "MobileDownload.BytesWasted.ChromeNetworkStack.Failure", bytesWasted);
                }
                break;
            case DownloadStatus.CANCELLED:
                if (!useDownloadManager) {
                    RecordHistogram.recordLongTimesHistogram(
                            "MobileDownload.DownloadTime.ChromeNetworkStack.Cancel",
                            totalDuration, TimeUnit.MILLISECONDS);
                    RecordHistogram.recordCountHistogram(
                            "MobileDownload.InterruptionsCount.ChromeNetworkStack.Cancel",
                            numInterruptions);
                    recordBytesWasted(
                            "MobileDownload.BytesWasted.ChromeNetworkStack.Cancel", bytesWasted);
                }
                break;
            default:
                break;
        }
    }

    /**
     * Helper method to record the bytes wasted metrics when a download completes.
     * @param name Histogram name
     * @param bytesWasted Bytes wasted during download.
     */
    private void recordBytesWasted(String name, long bytesWasted) {
        RecordHistogram.recordCustomCountHistogram(name,
                (int) ConversionUtils.bytesToKilobytes(bytesWasted), 1,
                ConversionUtils.KILOBYTES_PER_GIGABYTE, 50);
    }

    @Override
    public void onQueryCompleted(
            DownloadManagerDelegate.DownloadQueryResult result, boolean showNotification) {
        if (result.downloadStatus == DownloadStatus.IN_PROGRESS) return;
        if (showNotification) {
            switch (result.downloadStatus) {
                case DownloadStatus.COMPLETE:
                    if (shouldOpenAfterDownload(result.item.getDownloadInfo())
                            && result.canResolve) {
                        handleAutoOpenAfterDownload(result.item);
                    } else {
                        mDownloadSnackbarController.onDownloadSucceeded(
                                result.item.getDownloadInfo(),
                                DownloadSnackbarController.INVALID_NOTIFICATION_ID,
                                result.item.getSystemDownloadId(), result.canResolve, true);
                    }
                    break;
                case DownloadStatus.FAILED:
                    onDownloadFailed(result.item, result.failureReason);
                    break;
                default:
                    break;
            }
        }
        recordDownloadCompletionStats(true, result.downloadStatus,
                result.downloadTimeInMilliseconds, result.bytesDownloaded, 0, 0);
        removeUmaStatsEntry(result.item.getId());
    }

    /**
     * Called by tests to disable listening to network connection changes.
     */
    @VisibleForTesting
    static void disableNetworkListenerForTest() {
        sIsNetworkListenerDisabled = true;
    }

    /**
     * Called by tests to set the network type.
     * @isNetworkMetered Whether the network should appear to be metered.
     */
    @VisibleForTesting
    static void setIsNetworkMeteredForTest(boolean isNetworkMetered) {
        sIsNetworkMetered = isNetworkMetered;
    }

    /**
     * Helper method to add an auto resumable download.
     * @param guid Id of the download item.
     */
    private void addAutoResumableDownload(String guid) {
        if (mAutoResumableDownloadIds.isEmpty() && !sIsNetworkListenerDisabled) {
            mNetworkChangeNotifier = new NetworkChangeNotifierAutoDetect(
                    this, new RegistrationPolicyAlwaysRegister());
        }
        if (!mAutoResumableDownloadIds.contains(guid)) {
            mAutoResumableDownloadIds.add(guid);
        }
    }

    /**
     * Helper method to remove an auto resumable download.
     * @param guid Id of the download item.
     */
    private void removeAutoResumableDownload(String guid) {
        if (mAutoResumableDownloadIds.isEmpty()) return;
        mAutoResumableDownloadIds.remove(guid);
        stopListenToConnectionChangeIfNotNeeded();
    }

    /**
     * Helper method to remove a download from |mDownloadProgressMap|.
     * @param guid Id of the download item.
     */
    private void removeDownloadProgress(String guid) {
        mDownloadProgressMap.remove(guid);
        removeAutoResumableDownload(guid);
        sFirstSeenDownloadIds.remove(guid);
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        if (mAutoResumableDownloadIds.isEmpty()) return;
        if (connectionType == ConnectionType.CONNECTION_NONE) return;
        boolean isMetered = isActiveNetworkMetered(ContextUtils.getApplicationContext());
        // Make a copy of |mAutoResumableDownloadIds| as scheduleDownloadResumption() may delete
        // elements inside the array.
        List<String> copies = new ArrayList<String>(mAutoResumableDownloadIds);
        Iterator<String> iterator = copies.iterator();
        while (iterator.hasNext()) {
            final String id = iterator.next();
            final DownloadProgress progress = mDownloadProgressMap.get(id);
            // Introduce some delay in each resumption so we don't start all of them immediately.
            if (progress != null && (progress.mCanDownloadWhileMetered || !isMetered)) {
                scheduleDownloadResumption(progress.mDownloadItem);
            }
        }
        stopListenToConnectionChangeIfNotNeeded();
    }

    /**
     * Helper method to stop listening to the connection type change
     * if it is no longer needed.
     */
    private void stopListenToConnectionChangeIfNotNeeded() {
        if (mAutoResumableDownloadIds.isEmpty() && mNetworkChangeNotifier != null) {
            mNetworkChangeNotifier.destroy();
            mNetworkChangeNotifier = null;
        }
    }

    static boolean isActiveNetworkMetered(Context context) {
        if (sIsNetworkListenerDisabled) return sIsNetworkMetered;
        ConnectivityManager cm =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        return cm.isActiveNetworkMetered();
    }

    /**
     * Adds a DownloadUmaStatsEntry to |mUmaEntries| and SharedPrefs.
     * @param umaEntry A DownloadUmaStatsEntry to be added.
     */
    private void addUmaStatsEntry(DownloadUmaStatsEntry umaEntry) {
        mUmaEntries.add(umaEntry);
        storeUmaEntries();
    }

    /**
     * Gets a DownloadUmaStatsEntry from |mUmaEntries| given by its ID.
     * @param id ID of the UMA entry.
     */
    private DownloadUmaStatsEntry getUmaStatsEntry(String id) {
        Iterator<DownloadUmaStatsEntry> iterator = mUmaEntries.iterator();
        while (iterator.hasNext()) {
            DownloadUmaStatsEntry entry = iterator.next();
            if (entry.id.equals(id)) {
                return entry;
            }
        }
        return null;
    }

    /**
     * Removes a DownloadUmaStatsEntry from SharedPrefs given by the id.
     * @param id ID to be removed.
     */
    private void removeUmaStatsEntry(String id) {
        Iterator<DownloadUmaStatsEntry> iterator = mUmaEntries.iterator();
        boolean found = false;
        while (iterator.hasNext()) {
            DownloadUmaStatsEntry entry = iterator.next();
            if (entry.id.equals(id)) {
                iterator.remove();
                found = true;
                break;
            }
        }
        if (found) {
            storeUmaEntries();
        }
    }

    /**
     * Helper method to store all the DownloadUmaStatsEntry into SharedPreferences.
     */
    private void storeUmaEntries() {
        Set<String> entries = new HashSet<String>();
        for (int i = 0; i < mUmaEntries.size(); ++i) {
            entries.add(mUmaEntries.get(i).getSharedPreferenceString());
        }
        storeDownloadInfo(mSharedPrefs, DOWNLOAD_UMA_ENTRY, entries, false /* forceCommit */);
    }

    /**
     * Helper method to record the download completion UMA and remove the SharedPreferences entry.
     */
    private void recordDownloadFinishedUMA(
            int downloadStatus, String entryId, long bytesDownloaded) {
        DownloadUmaStatsEntry entry = getUmaStatsEntry(entryId);
        if (entry == null) return;
        long currentTime = System.currentTimeMillis();
        long totalTime = Math.max(0, currentTime - entry.downloadStartTime);
        recordDownloadCompletionStats(
                false, downloadStatus, totalTime, bytesDownloaded, entry.numInterruptions,
                entry.bytesWasted);
        removeUmaStatsEntry(entryId);
    }

    /**
     * Parse the DownloadUmaStatsEntry from the shared preference.
     */
    private void parseUMAStatsEntriesFromSharedPrefs() {
        if (mSharedPrefs.contains(DOWNLOAD_UMA_ENTRY)) {
            Set<String> entries =
                    DownloadManagerService.getStoredDownloadInfo(mSharedPrefs, DOWNLOAD_UMA_ENTRY);
            for (String entryString : entries) {
                DownloadUmaStatsEntry entry = DownloadUmaStatsEntry.parseFromString(entryString);
                if (entry != null) mUmaEntries.add(entry);
            }
        }
    }

    /** Adds a new DownloadObserver to the list. */
    @Override
    public void addDownloadObserver(DownloadObserver observer) {
        mDownloadObservers.addObserver(observer);
        DownloadSharedPreferenceHelper.getInstance().addObserver(observer);
    }

    /** Removes a DownloadObserver from the list. */
    @Override
    public void removeDownloadObserver(DownloadObserver observer) {
        mDownloadObservers.removeObserver(observer);
        DownloadSharedPreferenceHelper.getInstance().removeObserver(observer);
    }

    /**
     * Begins sending back information about all entries in the user's DownloadHistory via
     * {@link #onAllDownloadsRetrieved}.  If the DownloadHistory is not initialized yet, the
     * callback will be delayed.
     *
     * @param isOffTheRecord Whether or not to get downloads for the off the record profile.
     */
    @Override
    public void getAllDownloads(boolean isOffTheRecord) {
        nativeGetAllDownloads(getNativeDownloadManagerService(), isOffTheRecord);
    }

    /**
     * Fires an Intent that alerts the DownloadNotificationService that an action must be taken
     * for a particular item.
     */
    @Override
    public void broadcastDownloadAction(DownloadItem downloadItem, String action) {
        Context appContext = ContextUtils.getApplicationContext();
            Intent intent = DownloadNotificationFactory.buildActionIntent(appContext, action,
                    LegacyHelpers.buildLegacyContentId(false, downloadItem.getId()),
                    downloadItem.getDownloadInfo().isOffTheRecord());
            addCancelExtra(intent, downloadItem);
            appContext.startService(intent);
    }

    /**
     * Add an Intent extra for StateAtCancel UMA to know the state of a request prior to a
     * user-initated cancel.
     * @param intent The Intent associated with the download action.
     * @param downloadItem The download associated with download action.
     */
    private void addCancelExtra(Intent intent, DownloadItem downloadItem) {
        if (intent.getAction().equals(DownloadNotificationService2.ACTION_DOWNLOAD_CANCEL)) {
            int state;
            if (DownloadUtils.isDownloadPaused(downloadItem)) {
                state = DownloadNotificationUmaHelper.StateAtCancel.PAUSED;
            } else if (DownloadUtils.isDownloadPending(downloadItem)) {
                if (downloadItem.getDownloadInfo().getPendingState()
                        == PendingState.PENDING_NETWORK) {
                    state = DownloadNotificationUmaHelper.StateAtCancel.PENDING_NETWORK;
                } else {
                    state = DownloadNotificationUmaHelper.StateAtCancel.PENDING_ANOTHER_DOWNLOAD;
                }
            } else {
                state = DownloadNotificationUmaHelper.StateAtCancel.DOWNLOADING;
            }
            intent.putExtra(DownloadNotificationService2.EXTRA_DOWNLOAD_STATE_AT_CANCEL, state);
        }
    }

    /**
     * Checks if the files associated with any downloads have been removed by an external action.
     * @param isOffTheRecord Whether or not to check downloads for the off the record profile.
     */
    @Override
    public void checkForExternallyRemovedDownloads(boolean isOffTheRecord) {
        nativeCheckForExternallyRemovedDownloads(getNativeDownloadManagerService(), isOffTheRecord);
    }

    @CalledByNative
    private List<DownloadItem> createDownloadItemList() {
        return new ArrayList<DownloadItem>();
    }

    @CalledByNative
    private void addDownloadItemToList(List<DownloadItem> list, DownloadItem item) {
        list.add(item);
    }

    @CalledByNative
    private void onAllDownloadsRetrieved(final List<DownloadItem> list, boolean isOffTheRecord) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onAllDownloadsRetrieved(list, isOffTheRecord);
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
    private void maybeShowMissingSdCardError(List<DownloadItem> list) {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        // Only show the missing directory snackbar once.
        if (!prefServiceBridge.getBoolean(Pref.SHOW_MISSING_SD_CARD_ERROR_ANDROID)) return;

        DownloadDirectoryProvider provider = DownloadDirectoryProvider.getInstance();
        provider.getAllDirectoriesOptions((ArrayList<DirectoryOption> dirs) -> {
            if (dirs.size() > 1) return;
            String externalStorageDir = provider.getExternalStorageDirectory();

            for (DownloadItem item : list) {
                boolean missingOnSDCard = isFilePathOnMissingExternalDrive(
                        item.getDownloadInfo().getFilePath(), externalStorageDir, dirs);
                if (!isUnresumableOrCancelled(item) && missingOnSDCard) {
                    mHandler.post(() -> {
                        // TODO(shaktisahu): Show it on infobar in the right way.
                        mDownloadSnackbarController.onDownloadDirectoryNotFound();
                    });
                    prefServiceBridge.setBoolean(Pref.SHOW_MISSING_SD_CARD_ERROR_ANDROID, false);
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
        @DownloadState
        int state = downloadItem.getDownloadInfo().state();
        return (state == DownloadState.INTERRUPTED && !downloadItem.getDownloadInfo().isResumable())
                || state == DownloadState.CANCELLED;
    }

    /**
     * Returns whether a given file path is in a directory that is no longer available, most likely
     * because it is on an SD card that was removed.
     *
     * @param filePath  The file path to check.
     * @param externalStorageDir  The absolute path of external storage directory for primary
     * storage.
     * @param directoryOptions  All available download directories including primary storage and
     * secondary storage.
     *
     * @return          Whether this file path is in a directory that is no longer available.
     */
    private boolean isFilePathOnMissingExternalDrive(String filePath, String externalStorageDir,
            ArrayList<DirectoryOption> directoryOptions) {
        if (TextUtils.isEmpty(filePath) || filePath.contains(externalStorageDir)) {
            return false;
        }

        for (DirectoryOption directory : directoryOptions) {
            if (TextUtils.isEmpty(directory.location)) continue;
            if (filePath.contains(directory.location)) return false;
        }

        return true;
    }

    @CalledByNative
    private void onDownloadItemCreated(DownloadItem item) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onDownloadItemCreated(item);
        }
        DownloadInfoBarController infobarController =
                getInfoBarController(item.getDownloadInfo().isOffTheRecord());
        if (infobarController != null) infobarController.onDownloadItemUpdated(item);
    }

    @CalledByNative
    private void onDownloadItemUpdated(DownloadItem item) {
        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onDownloadItemUpdated(item);
        }

        DownloadInfoBarController infobarController =
                getInfoBarController(item.getDownloadInfo().isOffTheRecord());
        if (infobarController != null) infobarController.onDownloadItemUpdated(item);
    }

    @CalledByNative
    private void onDownloadItemRemoved(String guid, boolean isOffTheRecord) {
        DownloadInfoBarController infobarController = getInfoBarController(isOffTheRecord);
        if (infobarController != null) {
            infobarController.onDownloadItemRemoved(
                    LegacyHelpers.buildLegacyContentId(false, guid));
        }

        for (DownloadObserver adapter : mDownloadObservers) {
            adapter.onDownloadItemRemoved(guid, isOffTheRecord);
        }
    }

    @CalledByNative
    private void showDownloadManager(boolean showPrefetchedContent) {
        DownloadUtils.showDownloadManager(null, null, showPrefetchedContent);
    }

    @CalledByNative
    private void openDownloadItem(
            DownloadItem downloadItem, @DownloadMetrics.DownloadOpenSource int source) {
        DownloadInfo downloadInfo = downloadItem.getDownloadInfo();
        boolean canOpen = DownloadUtils.openFile(new File(downloadInfo.getFilePath()),
                downloadInfo.getMimeType(), downloadInfo.getDownloadGuid(),
                downloadInfo.isOffTheRecord(), downloadInfo.getOriginalUrl(),
                downloadInfo.getReferrer(), source);
        if (!canOpen) {
            openDownloadsPage(ContextUtils.getApplicationContext());
        }
    }

    /**
     * Opens a download. If the download cannot be opened, download home will be opened instead.
     * @param id The {@link ContentId} of the download to be opened.
     * @param source The source where the user opened this download.
     */
    public void openDownload(
            ContentId id, boolean isOffTheRecord, @DownloadMetrics.DownloadOpenSource int source) {
        nativeOpenDownload(getNativeDownloadManagerService(), id.id, isOffTheRecord, source);
    }

    /**
     * Checks whether the download will be immediately opened after completion.
     * @param downloadItem The download item to be opened.
     * @return True if the download will be auto-opened, false otherwise.
     */
    public void checkIfDownloadWillAutoOpen(DownloadItem downloadItem, Callback<Boolean> callback) {
        assert(downloadItem.getDownloadInfo().state() == DownloadState.COMPLETE);

        AsyncTask<Boolean> task = new AsyncTask<Boolean>() {
            @Override
            public Boolean doInBackground() {
                boolean isSupportedMimeType =
                        isSupportedMimeType(downloadItem.getDownloadInfo().getMimeType());
                boolean canResolve = isOMADownloadDescription(downloadItem.getDownloadInfo())
                        || canResolveDownloadItem(ContextUtils.getApplicationContext(),
                                   downloadItem, isSupportedMimeType);
                return canResolve && shouldOpenAfterDownload(downloadItem.getDownloadInfo());
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
        int reason = isExternalStorageMissing ? DownloadManager.ERROR_DEVICE_NOT_FOUND
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
        return ContextUtils.getApplicationContext().getSharedPreferences(
                DOWNLOAD_RETRY_COUNT_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Increments the interruption count for a download. If the interruption count reaches a certain
     * threshold, the download will no longer auto resume unless user click the resume button to
     * clear the count.
     *
     * @param downloadGuid Download GUID.
     * @param hasUserGesture Whether the retry is caused by user gesture.
     */
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
    private void clearDownloadRetryCount(String downloadGuid, boolean isAutoRetryOnly) {
        SharedPreferences sharedPrefs = getAutoRetryCountSharedPreference();
        String name = getDownloadRetryCountSharedPrefName(downloadGuid, !isAutoRetryOnly, false);
        int count = Math.min(sharedPrefs.getInt(name, 0), 200);
        assert count >= 0;
        SharedPreferences.Editor editor = sharedPrefs.edit();
        editor.remove(name);
        if (isAutoRetryOnly) {
            RecordHistogram.recordSparseSlowlyHistogram(
                    "MobileDownload.ResumptionsCount.Automatic", count);
        } else {
            RecordHistogram.recordSparseSlowlyHistogram(
                    "MobileDownload.ResumptionsCount.Manual", count);
            name = getDownloadRetryCountSharedPrefName(downloadGuid, false, true);
            count = sharedPrefs.getInt(name, 0);
            assert count >= 0;
            RecordHistogram.recordSparseSlowlyHistogram(
                    "MobileDownload.ResumptionsCount.Total", Math.min(count, 500));
            editor.remove(name);
        }
        editor.apply();
    }

    int getAutoResumptionLimit() {
        if (mAutoResumptionLimit < 0) {
            mAutoResumptionLimit = nativeGetAutoResumptionLimit();
        }
        return mAutoResumptionLimit;
    }

    /**
     * Updates the last access time of a download.
     * @param downloadGuid Download GUID.
     * @param isOffTheRecord Whether the download is off the record.
     */
    @Override
    public void updateLastAccessTime(String downloadGuid, boolean isOffTheRecord) {
        if (TextUtils.isEmpty(downloadGuid)) return;

        nativeUpdateLastAccessTime(getNativeDownloadManagerService(), downloadGuid, isOffTheRecord);
    }

    @Override
    public void onConnectionSubtypeChanged(int newConnectionSubtype) {}

    @Override
    public void onNetworkConnect(long netId, int connectionType) {}

    @Override
    public void onNetworkSoonToDisconnect(long netId) {}

    @Override
    public void onNetworkDisconnect(long netId) {}

    @Override
    public void purgeActiveNetworkList(long[] activeNetIds) {}

    private static native boolean nativeIsSupportedMimeType(String mimeType);
    private static native int nativeGetAutoResumptionLimit();

    private native long nativeInit(boolean isFullBrowserStarted);
    private native void nativeOpenDownload(long nativeDownloadManagerService, String downloadGuid,
            boolean isOffTheRecord, int source);
    private native void nativeResumeDownload(
            long nativeDownloadManagerService, String downloadGuid, boolean isOffTheRecord);
    private native void nativeCancelDownload(
            long nativeDownloadManagerService, String downloadGuid, boolean isOffTheRecord);
    private native void nativePauseDownload(long nativeDownloadManagerService, String downloadGuid,
            boolean isOffTheRecord);
    private native void nativeRemoveDownload(long nativeDownloadManagerService, String downloadGuid,
            boolean isOffTheRecord);
    private native void nativeGetAllDownloads(
            long nativeDownloadManagerService, boolean isOffTheRecord);
    private native void nativeCheckForExternallyRemovedDownloads(
            long nativeDownloadManagerService, boolean isOffTheRecord);
    private native void nativeUpdateLastAccessTime(
            long nativeDownloadManagerService, String downloadGuid, boolean isOffTheRecord);
    private native void nativeOnFullBrowserStarted(long nativeDownloadManagerService);
}
