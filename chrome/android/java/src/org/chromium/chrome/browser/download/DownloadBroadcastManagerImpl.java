// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.DownloadManager.ACTION_NOTIFICATION_CLICKED;

import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_CANCEL;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_OPEN;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_PAUSE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_RESUME;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_DOWNLOAD_CONTENTID_ID;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_DOWNLOAD_CONTENTID_NAMESPACE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_IS_OFF_THE_RECORD;
import static org.chromium.chrome.browser.notifications.NotificationConstants.EXTRA_NOTIFICATION_ID;

import android.app.DownloadManager;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.IBinder;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Class that spins up native when an interaction with a notification happens and passes the
 * relevant information on to native.
 */
public class DownloadBroadcastManagerImpl extends DownloadBroadcastManager.Impl {
    private static final int WAIT_TIME_MS = 5000;

    private final DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper =
            DownloadSharedPreferenceHelper.getInstance();

    private final DownloadNotificationService mDownloadNotificationService;
    private final Handler mHandler = new Handler();
    private final Runnable mStopSelfRunnable =
            new Runnable() {
                @Override
                public void run() {
                    getService().stopSelf();
                }
            };

    public static <T> void checkNotNull(T reference) {
        if (reference == null) {
            throw new NullPointerException();
        }
    }

    public DownloadBroadcastManagerImpl() {
        mDownloadNotificationService = DownloadNotificationService.getInstance();
    }

    // The service is only explicitly started in the resume case.
    // TODO(dtrainor): Start DownloadBroadcastManager explicitly in resumption refactor.
    public static void startDownloadBroadcastManager(Context context, Intent source) {
        Intent intent = source != null ? new Intent(source) : new Intent();
        intent.setComponent(new ComponentName(context, DownloadBroadcastManager.class));
        context.startService(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Handle the download operation.
        onNotificationInteraction(intent);

        // If Chrome gets killed, do not restart the service.
        return Service.START_NOT_STICKY;
    }

    /**
     * Passes down information about a notification interaction to native.
     * @param intent with information about the notification interaction (action, contentId, etc).
     */
    public void onNotificationInteraction(final Intent intent) {
        if (!isActionHandled(intent)) return;

        // Remove delayed stop of service until after native library is loaded.
        mHandler.removeCallbacks(mStopSelfRunnable);

        // Update notification appearance immediately in case it takes a while for native to load.
        updateNotification(intent);

        // Handle the intent and propagate it through the native library.
        loadNativeAndPropagateInteraction(intent);
    }

    /**
     * Immediately update notification appearance without changing stored notification state.
     * @param intent with information about the notification.
     */
    void updateNotification(Intent intent) {
        String action = intent.getAction();
        if (!immediateNotificationUpdateNeeded(action)) return;

        final DownloadSharedPreferenceEntry entry = getDownloadEntryFromIntent(intent);
        final ContentId contentId = getContentIdFromIntent(intent);

        switch (action) {
            case ACTION_DOWNLOAD_PAUSE:
                if (entry != null) {
                    mDownloadNotificationService.notifyDownloadPaused(
                            entry.id,
                            entry.fileName,
                            true,
                            false,
                            entry.otrProfileID,
                            entry.isTransient,
                            null,
                            null,
                            false,
                            true,
                            false,
                            PendingState.NOT_PENDING);
                }
                break;

            case ACTION_DOWNLOAD_CANCEL:
                int notificationId = IntentUtils.safeGetIntExtra(intent, EXTRA_NOTIFICATION_ID, -1);
                // For old build, notification needs to be retrieved from the
                // DownloadSharedPreferenceEntry.
                if (notificationId < 0 && entry != null) {
                    notificationId = entry.notificationId;
                }
                if (notificationId >= 0 && contentId != null) {
                    mDownloadNotificationService.notifyDownloadCanceled(
                            contentId, notificationId, true);
                }
                break;

            case ACTION_DOWNLOAD_RESUME:
                if (entry != null) {
                    // If user manually resumes a download, update the network type if it
                    // is not metered previously.
                    boolean canDownloadWhileMetered =
                            entry.canDownloadWhileMetered
                                    || DownloadManagerService.isActiveNetworkMetered(
                                            ContextUtils.getApplicationContext());
                    // Update the SharedPreference entry.
                    mDownloadSharedPreferenceHelper.addOrReplaceSharedPreferenceEntry(
                            new DownloadSharedPreferenceEntry(
                                    entry.id,
                                    entry.notificationId,
                                    entry.otrProfileID,
                                    canDownloadWhileMetered,
                                    entry.fileName,
                                    true,
                                    entry.isTransient));

                    mDownloadNotificationService.notifyDownloadPending(
                            entry.id,
                            entry.fileName,
                            entry.otrProfileID,
                            entry.canDownloadWhileMetered,
                            entry.isTransient,
                            null,
                            null,
                            false,
                            true,
                            PendingState.PENDING_NETWORK);
                }
                break;

            default:
                // No-op.
                break;
        }
    }

    boolean immediateNotificationUpdateNeeded(String action) {
        return ACTION_DOWNLOAD_PAUSE.equals(action)
                || ACTION_DOWNLOAD_CANCEL.equals(action)
                || ACTION_DOWNLOAD_RESUME.equals(action);
    }

    /**
     * Helper function that loads the native and runs given runnable.
     * @param intent that is propagated when the native is loaded.
     */
    @VisibleForTesting
    void loadNativeAndPropagateInteraction(final Intent intent) {
        final ContentId id = getContentIdFromIntent(intent);
        final BrowserParts parts =
                new EmptyBrowserParts() {
                    @Override
                    public void finishNativeInitialization() {
                        // Delay the stop of the service by WAIT_TIME_MS after native library is
                        // loaded.
                        mHandler.postDelayed(mStopSelfRunnable, WAIT_TIME_MS);

                        DownloadStartupUtils.ensureDownloadSystemInitialized(
                                BrowserStartupController.getInstance().isFullBrowserStarted(),
                                IntentUtils.safeGetBooleanExtra(
                                        intent, EXTRA_IS_OFF_THE_RECORD, false));
                        propagateInteraction(intent);
                    }

                    @Override
                    public boolean startMinimalBrowser() {
                        if (!LegacyHelpers.isLegacyDownload(id)) return false;
                        return !ACTION_DOWNLOAD_OPEN.equals(intent.getAction());
                    }
                };

        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }

    @VisibleForTesting
    void propagateInteraction(Intent intent) {
        String action = intent.getAction();
        DownloadNotificationUmaHelper.recordNotificationInteractionHistogram(action);
        final ContentId id = getContentIdFromIntent(intent);
        final DownloadSharedPreferenceEntry entry = getDownloadEntryFromIntent(intent);
        boolean isOffTheRecord =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);

        OTRProfileID otrProfileID;
        if (entry != null) {
            otrProfileID = entry.otrProfileID;
        } else {
            // If the profile doesn't exist, then do not perform any action.
            if (!DownloadUtils.doesProfileExistFromIntent(intent)) return;
            otrProfileID = DownloadUtils.getOTRProfileIDFromIntent(intent);
        }
        assert !isOffTheRecord || otrProfileID != null;

        // Handle actions that do not require a specific entry or service delegate.
        switch (action) {
            case ACTION_NOTIFICATION_CLICKED:
                openDownload(ContextUtils.getApplicationContext(), intent, otrProfileID, id);
                return;

            case ACTION_DOWNLOAD_OPEN:
                if (id != null) {
                    OpenParams openParams = new OpenParams(LaunchLocation.NOTIFICATION);
                    openParams.openInIncognito =
                            IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);
                    OfflineContentAggregatorNotificationBridgeUiFactory.instance()
                            .openItem(openParams, id);
                }
                return;
        }

        DownloadServiceDelegate downloadServiceDelegate = getServiceDelegate(id);

        checkNotNull(downloadServiceDelegate);
        checkNotNull(id);

        // Handle all remaining actions.
        switch (action) {
            case ACTION_DOWNLOAD_CANCEL:
                downloadServiceDelegate.cancelDownload(id, otrProfileID);
                break;

            case ACTION_DOWNLOAD_PAUSE:
                downloadServiceDelegate.pauseDownload(id, otrProfileID);
                break;

            case ACTION_DOWNLOAD_RESUME:
                DownloadItem item =
                        (entry != null)
                                ? entry.buildDownloadItem()
                                : new DownloadItem(
                                        false,
                                        new DownloadInfo.Builder()
                                                .setDownloadGuid(id.id)
                                                .setOTRProfileId(otrProfileID)
                                                .build());
                downloadServiceDelegate.resumeDownload(id, item);
                break;

            default:
                // No-op.
                break;
        }

        downloadServiceDelegate.destroyServiceDelegate();
    }

    static boolean isActionHandled(Intent intent) {
        if (intent == null) return false;
        String action = intent.getAction();
        return ACTION_DOWNLOAD_CANCEL.equals(action)
                || ACTION_DOWNLOAD_PAUSE.equals(action)
                || ACTION_DOWNLOAD_RESUME.equals(action)
                || ACTION_DOWNLOAD_OPEN.equals(action)
                || ACTION_NOTIFICATION_CLICKED.equals(action);
    }

    /**
     * Retrieves DownloadSharedPreferenceEntry from a download action intent.
     * TODO(crbug.com/40506285): Instead of getting entire entry, pass only id/isOffTheRecord, after
     * consolidating all downloads-related objects.
     *
     * @param intent Intent that contains the download action.
     */
    private DownloadSharedPreferenceEntry getDownloadEntryFromIntent(Intent intent) {
        return mDownloadSharedPreferenceHelper.getDownloadSharedPreferenceEntry(
                getContentIdFromIntent(intent));
    }

    /**
     * @param intent The {@link Intent} to pull from and build a {@link ContentId}.
     * @return A {@link ContentId} built by pulling extras from {@code intent}.  This will be
     *         {@code null} if {@code intent} is missing any required extras.
     */
    static ContentId getContentIdFromIntent(Intent intent) {
        if (!intent.hasExtra(EXTRA_DOWNLOAD_CONTENTID_ID)
                || !intent.hasExtra(EXTRA_DOWNLOAD_CONTENTID_NAMESPACE)) {
            return null;
        }

        return new ContentId(
                IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_NAMESPACE),
                IntentUtils.safeGetStringExtra(intent, EXTRA_DOWNLOAD_CONTENTID_ID));
    }

    /**
     * Gets appropriate download delegate that can handle interactions with download item referred
     * to by the entry.
     * @param id The {@link ContentId} to grab the delegate for.
     * @return delegate for interactions with the entry
     */
    static DownloadServiceDelegate getServiceDelegate(ContentId id) {
        return OfflineContentAggregatorNotificationBridgeUiFactory.instance();
    }

    /**
     * Called to open a particular download item. Falls back to opening Download Home if
     * the download cannot be found by android DownloadManager.
     * @param context Context of the receiver.
     * @param intent Intent from the notification.
     * @param otrProfileID The {@link OTRProfileID} to determine whether to open download page
     * in incognito profile.
     * @param contentId Content ID of the download.
     */
    private void openDownload(
            Context context, Intent intent, OTRProfileID otrProfileID, ContentId contentId) {
        String downloadFilePath =
                IntentUtils.safeGetStringExtra(
                        intent, DownloadNotificationService.EXTRA_DOWNLOAD_FILE_PATH);
        if (ContentUriUtils.isContentUri(downloadFilePath)) {
            // On Q+, content URI is being used and there is no download ID.
            openDownloadWithId(context, intent, DownloadConstants.INVALID_DOWNLOAD_ID, contentId);
        } else {
            long[] ids =
                    intent.getLongArrayExtra(DownloadManager.EXTRA_NOTIFICATION_CLICK_DOWNLOAD_IDS);
            if (ids == null || ids.length == 0) {
                DownloadManagerService.openDownloadsPage(
                        otrProfileID, DownloadOpenSource.NOTIFICATION);
                return;
            }

            long id = ids[0];
            DownloadManagerBridge.queryDownloadResult(
                    id,
                    result -> {
                        if (result.contentUri == null) {
                            DownloadManagerService.openDownloadsPage(
                                    otrProfileID, DownloadOpenSource.NOTIFICATION);
                            return;
                        }
                        openDownloadWithId(context, intent, id, contentId);
                    });
        }
    }

    /**
     * Called to open a particular download item with the given ID.
     * @param context Context of the receiver.
     * @param intent Intent from the notification.
     * @param id ID from the Android DownloadManager, or DownloadConstants.INVALID_DOWNLOAD_ID on
     *         Q+.
     * @param contentId Content ID of the download.
     */
    private void openDownloadWithId(Context context, Intent intent, long id, ContentId contentId) {
        String downloadFilePath =
                IntentUtils.safeGetStringExtra(
                        intent, DownloadNotificationService.EXTRA_DOWNLOAD_FILE_PATH);
        boolean isSupportedMimeType =
                IntentUtils.safeGetBooleanExtra(
                        intent, DownloadNotificationService.EXTRA_IS_SUPPORTED_MIME_TYPE, false);
        boolean isOffTheRecord =
                IntentUtils.safeGetBooleanExtra(
                        intent, DownloadNotificationService.EXTRA_IS_OFF_THE_RECORD, false);
        // If the profile doesn't exist, then do not open the download.
        if (!DownloadUtils.doesProfileExistFromIntent(intent)) return;
        OTRProfileID otrProfileID = DownloadUtils.getOTRProfileIDFromIntent(intent);
        assert !isOffTheRecord || otrProfileID != null;
        Uri originalUrl = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_ORIGINATING_URI);
        Uri referrer = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_REFERRER);
        DownloadManagerService.openDownloadedContent(
                context,
                downloadFilePath,
                isSupportedMimeType,
                otrProfileID,
                contentId.id,
                id,
                originalUrl == null ? null : originalUrl.toString(),
                referrer == null ? null : referrer.toString(),
                DownloadOpenSource.NOTIFICATION,
                null);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        // Since this service does not need to be bound, just return null.
        return null;
    }
}
