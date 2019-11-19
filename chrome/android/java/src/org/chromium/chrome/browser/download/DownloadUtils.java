// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.StrictMode;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TimeUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeIntentUtil;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.components.download.DownloadState;
import org.chromium.components.download.ResumeMode;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.NumberFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Locale;

/**
 * A class containing some utility static methods.
 */
public class DownloadUtils {

    /** Strings indicating how many bytes have been downloaded for different units. */
    @VisibleForTesting
    static final int[] BYTES_DOWNLOADED_STRINGS = {
        R.string.file_size_downloaded_kb,
        R.string.file_size_downloaded_mb,
        R.string.file_size_downloaded_gb
    };

    private static final int[] BYTES_AVAILABLE_STRINGS = {
            R.string.download_manager_ui_space_free_kb, R.string.download_manager_ui_space_free_mb,
            R.string.download_manager_ui_space_free_gb};

    private static final int[] BYTES_STRINGS = {
            R.string.download_ui_kb, R.string.download_ui_mb, R.string.download_ui_gb};

    private static final String TAG = "download";

    private static final String DEFAULT_MIME_TYPE = "*/*";
    private static final String MIME_TYPE_DELIMITER = "/";
    private static final String MIME_TYPE_SHARING_URL = "text/plain";

    private static final String EXTRA_IS_OFF_THE_RECORD =
            "org.chromium.chrome.browser.download.IS_OFF_THE_RECORD";
    public static final String EXTRA_SHOW_PREFETCHED_CONTENT =
            "org.chromium.chrome.browser.download.SHOW_PREFETCHED_CONTENT";

    @VisibleForTesting
    static final String ELLIPSIS = "\u2026";

    /**
     * Possible sizes of type-based icons.
     */
    @IntDef({IconSize.DP_24, IconSize.DP_36})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconSize {
        int DP_24 = 24;
        int DP_36 = 36;
    }

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param source The source where the user action is coming from.
     * @return Whether the UI was shown.
     */
    public static boolean showDownloadManager(
            @Nullable Activity activity, @Nullable Tab tab, @DownloadOpenSource int source) {
        return showDownloadManager(activity, tab, source, false);
    }

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param source The source where the user action is coming from.
     * @param showPrefetchedContent Whether the manager should start with prefetched content section
     * expanded.
     * @return Whether the UI was shown.
     */
    @CalledByNative
    public static boolean showDownloadManager(@Nullable Activity activity, @Nullable Tab tab,
            @DownloadOpenSource int source, boolean showPrefetchedContent) {
        // Figure out what tab was last being viewed by the user.
        if (activity == null) activity = ApplicationStatus.getLastTrackedFocusedActivity();
        Context appContext = ContextUtils.getApplicationContext();
        boolean isTablet;

        if (tab == null && activity instanceof ChromeTabbedActivity) {
            ChromeTabbedActivity chromeActivity = ((ChromeTabbedActivity) activity);
            tab = chromeActivity.getActivityTab();
            isTablet = chromeActivity.isTablet();
        } else {
            Context displayContext = activity != null ? activity : appContext;
            isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(displayContext);
        }

        if (isTablet) {
            // Download Home shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.DOWNLOADS_URL);
            if (tab == null || !tab.isInitialized()) {
                // Open a new tab, which pops Chrome into the foreground.
                TabDelegate delegate = new TabDelegate(false);
                delegate.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                // Download Home shows up inside an existing tab, but only if the last Activity was
                // the ChromeTabbedActivity.
                tab.loadUrl(params);

                // Bring Chrome to the foreground, if possible.
                Intent intent = ChromeIntentUtil.createBringTabToFrontIntent(tab.getId());
                if (intent != null) {
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    IntentUtils.safeStartActivity(appContext, intent);
                }
            }
        } else {
            // Download Home shows up as a new Activity on phones.
            Intent intent = new Intent();
            intent.setClass(appContext, DownloadActivity.class);
            intent.putExtra(EXTRA_SHOW_PREFETCHED_CONTENT, showPrefetchedContent);
            if (tab != null) intent.putExtra(EXTRA_IS_OFF_THE_RECORD, tab.isIncognito());
            if (activity == null) {
                // Stands alone in its own task.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                appContext.startActivity(intent);
            } else {
                // Sits on top of another Activity.
                intent.addFlags(
                        Intent.FLAG_ACTIVITY_MULTIPLE_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
                activity.startActivity(intent);
            }
        }

        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isFullBrowserStarted()) {
            Profile profile = (tab == null ? Profile.getLastUsedProfile() : tab.getProfile());
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.notifyEvent(EventConstants.DOWNLOAD_HOME_OPENED);
        }
        DownloadMetrics.recordDownloadPageOpen(source);
        return true;
    }

    /**
     * @return Whether Chrome is currently offline and Offline Home should be shown for downloads.
     */
    public static boolean shouldShowOfflineHome() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_HOME)
                && NetworkChangeNotifier.isInitialized() && !NetworkChangeNotifier.isOnline();
    }

    /**
     * @return Whether or not the Intent corresponds to a DownloadActivity that should show off the
     *         record downloads.
     */
    public static boolean shouldShowOffTheRecordDownloads(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_IS_OFF_THE_RECORD, false);
    }

    /**
     * @return Whether or not the prefetched content section should be expanded on launch of the
     * DownloadActivity.
     */
    public static boolean shouldShowPrefetchContent(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_SHOW_PREFETCHED_CONTENT, false);
    }

    /**
     * Records metrics related to downloading a page. Should be called after a tap on the download
     * page button.
     * @param tab The Tab containing the page being downloaded.
     */
    public static void recordDownloadPageMetrics(Tab tab) {
        RecordHistogram.recordPercentageHistogram(
                "OfflinePages.SavePage.PercentLoaded", Math.round(tab.getProgress() * 100));
    }

    /**
     * Shows a "Downloading..." toast. Should be called after a download has been started.
     * @param context The {@link Context} used to make the toast.
     */
    public static void showDownloadStartToast(Context context) {
        Toast.makeText(context, R.string.download_started, Toast.LENGTH_SHORT).show();
    }

    /**
     * Issues a request to the {@link DownloadManagerService} associated to check for externally
     * removed downloads.
     * See {@link DownloadManagerService#checkForExternallyRemovedDownloads}.
     * @param isOffTheRecord  Whether to check downloads for the off the record profile.
     */
    public static void checkForExternallyRemovedDownloads(boolean isOffTheRecord) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return;
        }

        if (isOffTheRecord) {
            DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                    true);
        }
        DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                false);
        RecordUserAction.record(
                "Android.DownloadManager.CheckForExternallyRemovedItems");
    }

    /**
     * Trigger the download of an Offline Page.
     * @param context Context to pull resources from.
     */
    public static void downloadOfflinePage(Context context, Tab tab) {
        OfflinePageOrigin origin = new OfflinePageOrigin(context, tab);

        if (tab.isShowingErrorPage()) {
            // The download needs to be scheduled to happen at later time due to current network
            // error.
            final OfflinePageBridge bridge = OfflinePageBridge.getForProfile(tab.getProfile());
            bridge.scheduleDownload(tab.getWebContents(), OfflinePageBridge.ASYNC_NAMESPACE,
                    tab.getUrl(), DownloadUiActionFlags.PROMPT_DUPLICATE, origin);
        } else {
            // Otherwise, the download can be started immediately.
            OfflinePageDownloadBridge.startDownload(tab, origin);
            DownloadUtils.recordDownloadPageMetrics(tab);
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
        tracker.notifyEvent(EventConstants.DOWNLOAD_PAGE_STARTED);
    }

    /**
     * Whether the user should be allowed to download the current page.
     * @param tab Tab displaying the page that will be downloaded.
     * @return    Whether the "Download Page" button should be enabled.
     */
    public static boolean isAllowedToDownloadPage(Tab tab) {
        if (tab == null) return false;

        // Offline pages isn't supported in Incognito. This should be checked before calling
        // OfflinePageBridge.getForProfile because OfflinePageBridge instance will not be found
        // for incognito profile.
        if (tab.isIncognito()) return false;

        // Check if the page url is supported for saving. Only HTTP and HTTPS pages are allowed.
        if (!OfflinePageBridge.canSavePage(tab.getUrl())) return false;

        // Download will only be allowed for the error page if download button is shown in the page.
        if (tab.isShowingErrorPage()) {
            final OfflinePageBridge bridge = OfflinePageBridge.getForProfile(tab.getProfile());
            return bridge.isShowingDownloadButtonInErrorPage(tab.getWebContents());
        }

        if (tab.isShowingInterstitialPage()) return false;

        // Don't allow re-downloading the currently displayed offline page.
        if (OfflinePageUtils.isOfflinePage(tab)) return false;

        return true;
    }

    /**
     * Returns a URI that points at the file.
     * @param filePath File path to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    @MainThread
    public static Uri getUriForItem(String filePath) {
        if (ContentUriUtils.isContentUri(filePath)) return Uri.parse(filePath);

        // It's ok to use blocking calls on main thread here, since the user is waiting to open or
        // share the file to other apps.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        Uri uri = null;

        try {
            boolean isOnSDCard = DownloadDirectoryProvider.isDownloadOnSDCard(filePath);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_FILE_PROVIDER)
                    && isOnSDCard) {
                // Use custom file provider to generate content URI for download on SD card.
                uri = DownloadFileProvider.createContentUri(filePath);
            } else {
                // Use FileProvider to generate content URI or file URI.
                uri = FileUtils.getUriForFile(new File(filePath));
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        return uri;
    }

    /**
     * Get the URI when shared or opened by other apps.
     *
     * @param filePath Downloaded file path.
     * @return URI for other apps to use the file via {@link android.content.ContentResolver}.
     */
    public static Uri getUriForOtherApps(String filePath) {
        // Some old Samsung devices with Android M- must use file URI. See https://crbug.com/705748.
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.M ? getUriForItem(filePath)
                                                             : Uri.fromFile(new File(filePath));
    }

    @CalledByNative
    private static String getUriStringForPath(String filePath) {
        if (ContentUriUtils.isContentUri(filePath)) return filePath;
        Uri uri = getUriForItem(filePath);
        return uri != null ? uri.toString() : new String();
    }

    /**
     * Utility method to open an {@link OfflineItem}, which can be a chrome download, offline page.
     * Falls back to open download home.
     * @param contentId The {@link ContentId} of the associated offline item.
     */
    public static void openItem(
            ContentId contentId, boolean isOffTheRecord, @DownloadOpenSource int source) {
        if (LegacyHelpers.isLegacyAndroidDownload(contentId)) {
            ContextUtils.getApplicationContext().startActivity(
                    new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS)
                            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } else if (LegacyHelpers.isLegacyOfflinePage(contentId)) {
            OfflineContentAggregatorFactory.get().openItem(LaunchLocation.PROGRESS_BAR, contentId);
        } else {
            DownloadManagerService.getDownloadManagerService().openDownload(
                    contentId, isOffTheRecord, source);
        }
    }

    /**
     * Opens a file in Chrome or in another app if appropriate.
     * @param filePath Path to the file to open, can be a content Uri.
     * @param mimeType mime type of the file.
     * @param downloadGuid The associated download GUID.
     * @param isOffTheRecord whether we are in an off the record context.
     * @param originalUrl The original url of the downloaded file.
     * @param referrer Referrer of the downloaded file.
     * @param source The source that tries to open the download file.
     * @return whether the file could successfully be opened.
     */
    public static boolean openFile(String filePath, String mimeType, String downloadGuid,
            boolean isOffTheRecord, String originalUrl, String referrer,
            @DownloadOpenSource int source) {
        DownloadMetrics.recordDownloadOpen(source, mimeType);
        Context context = ContextUtils.getApplicationContext();
        DownloadManagerService service = DownloadManagerService.getDownloadManagerService();

        // Check if Chrome should open the file itself.
        if (service.isDownloadOpenableInBrowser(isOffTheRecord, mimeType)) {
            // Share URIs use the content:// scheme when able, which looks bad when displayed
            // in the URL bar.
            Uri contentUri = getUriForItem(filePath);
            Uri fileUri = contentUri;
            if (!ContentUriUtils.isContentUri(filePath)) {
                File file = new File(filePath);
                fileUri = Uri.fromFile(file);
            }
            String normalizedMimeType = Intent.normalizeMimeType(mimeType);

            Intent intent = MediaViewerUtils.getMediaViewerIntent(fileUri /*displayUri*/,
                    contentUri /*contentUri*/, normalizedMimeType,
                    true /* allowExternalAppHandlers */);
            IntentHandler.startActivityForTrustedIntent(intent);
            service.updateLastAccessTime(downloadGuid, isOffTheRecord);
            return true;
        }

        // Check if any apps can open the file.
        try {
            // TODO(qinmin): Move this to an AsyncTask so we don't need to temper with strict mode.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            Uri uri = ContentUriUtils.isContentUri(filePath) ? Uri.parse(filePath)
                                                             : getUriForOtherApps(filePath);
            StrictMode.setThreadPolicy(oldPolicy);
            Intent viewIntent =
                    MediaViewerUtils.createViewIntentForUri(uri, mimeType, originalUrl, referrer);
            context.startActivity(viewIntent);
            service.updateLastAccessTime(downloadGuid, isOffTheRecord);
            return true;
        } catch (Exception e) {
            // Can't launch the Intent.
            if (source != DownloadOpenSource.DOWNLOAD_PROGRESS_INFO_BAR) {
                Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                             Toast.LENGTH_SHORT)
                        .show();
            }
            return false;
        }
    }

    @CalledByNative
    private static void openDownload(String filePath, String mimeType, String downloadGuid,
            boolean isOffTheRecord, String originalUrl, String referer,
            @DownloadOpenSource int source) {
        boolean canOpen = DownloadUtils.openFile(
                filePath, mimeType, downloadGuid, isOffTheRecord, originalUrl, referer, source);
        if (!canOpen) {
            DownloadUtils.showDownloadManager(null, null, source);
        }
    }

    private static void recordShareHistograms(int count, int filterType) {
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Share.FileTypes",
                filterType, DownloadFilter.Type.NUM_ENTRIES);

        RecordHistogram.recordLinearCountHistogram("Android.DownloadManager.Share.Count",
                count, 1, 20, 20);
    }

    /**
     * Fires an Intent to open a downloaded item.
     * @param context Context to use.
     * @param intent  Intent that can be fired.
     * @return Whether an Activity was successfully started for the Intent.
     */
    static boolean fireOpenIntentForDownload(Context context, Intent intent) {
        try {
            if (TextUtils.equals(intent.getPackage(), context.getPackageName())) {
                IntentHandler.startActivityForTrustedIntent(intent);
            } else {
                context.startActivity(intent);
            }
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.d(TAG, "Activity not found for " + intent.getType() + " over "
                    + intent.getData().getScheme(), ex);
        } catch (SecurityException ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        } catch (Exception ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        }

        return false;
    }

    /**
     * Helper method to determine the progress text to use for an in progress download notification.
     * @param progress The {@link Progress} struct that represents the current state of an in
     *                 progress download.
     * @return         The {@link String} that represents the progress.
     */
    public static String getProgressTextForNotification(Progress progress) {
        Context context = ContextUtils.getApplicationContext();

        if (progress.isIndeterminate() && progress.value == 0) {
            return context.getResources().getString(R.string.download_started);
        }

        switch (progress.unit) {
            case OfflineItemProgressUnit.PERCENTAGE:
                return progress.isIndeterminate()
                        ? context.getResources().getString(R.string.download_started)
                        : getPercentageString(progress.getPercentage());
            case OfflineItemProgressUnit.BYTES:
                String bytes = getStringForBytes(context, progress.value);
                if (progress.isIndeterminate()) {
                    return context.getResources().getString(
                            R.string.download_ui_indeterminate_bytes, bytes);
                } else {
                    String total = getStringForBytes(context, progress.max);
                    return context.getResources().getString(
                            R.string.download_ui_determinate_bytes, bytes, total);
                }
            case OfflineItemProgressUnit.FILES:
                if (progress.isIndeterminate()) {
                    int fileCount = (int) Math.min(Integer.MAX_VALUE, progress.value);
                    return context.getResources().getQuantityString(
                            R.plurals.download_ui_files_downloaded, fileCount, fileCount);
                } else {
                    return formatRemainingFiles(context, progress);
                }
            default:
                assert false;
        }

        return "";
    }

    /**
     * Create a string that represents the percentage of the file that has downloaded.
     * @param percentage Current percentage of the file.
     * @return String representing the percentage of the file that has been downloaded.
     */
    public static String getPercentageString(int percentage) {
        NumberFormat formatter = NumberFormat.getPercentInstance(Locale.getDefault());
        return formatter.format(percentage / 100.0);
    }

    /**
     * Creates a string that shows the time left or number of files left.
     * @param context The application context.
     * @param progress The download progress.
     * @param timeRemainingInMillis The remaining time in milli seconds.
     * @return Formatted string representing the time left or the number of files left.
     */
    public static String getTimeOrFilesLeftString(
            Context context, Progress progress, long timeRemainingInMillis) {
        return progress.unit == OfflineItemProgressUnit.FILES
                ? formatRemainingFiles(context, progress)
                : formatRemainingTime(context, timeRemainingInMillis);
    }

    /**
     * Creates a string that represents the number of files left to be downloaded.
     * @param progress Current download progress.
     * @return String representing the number of files left.
     */
    public static String formatRemainingFiles(Context context, Progress progress) {
        int filesLeft = (int) (progress.max - progress.value);
        return filesLeft == 1 ? context.getResources().getString(R.string.one_file_left)
                              : context.getResources().getString(R.string.files_left, filesLeft);
    }

    /**
     * Format remaining time for the given millis, in the following format:
     * 5 hours; will include 1 unit, can go down to seconds precision.
     * This is similar to what android.java.text.Formatter.formatShortElapsedTime() does. Don't use
     * ui::TimeFormat::Simple() as it is very expensive.
     *
     * @param context the application context.
     * @param millis the remaining time in milli seconds.
     * @return the formatted remaining time.
     */
    public static String formatRemainingTime(Context context, long millis) {
        long secondsLong = millis / 1000;

        int days = 0;
        int hours = 0;
        int minutes = 0;
        if (secondsLong >= TimeUtils.SECONDS_PER_DAY) {
            days = (int) (secondsLong / TimeUtils.SECONDS_PER_DAY);
            secondsLong -= days * TimeUtils.SECONDS_PER_DAY;
        }
        if (secondsLong >= TimeUtils.SECONDS_PER_HOUR) {
            hours = (int) (secondsLong / TimeUtils.SECONDS_PER_HOUR);
            secondsLong -= hours * TimeUtils.SECONDS_PER_HOUR;
        }
        if (secondsLong >= TimeUtils.SECONDS_PER_MINUTE) {
            minutes = (int) (secondsLong / TimeUtils.SECONDS_PER_MINUTE);
            secondsLong -= minutes * TimeUtils.SECONDS_PER_MINUTE;
        }
        int seconds = (int) secondsLong;

        if (days >= 2) {
            days += (hours + 12) / 24;
            return context.getString(R.string.remaining_duration_days, days);
        } else if (days > 0) {
            return context.getString(R.string.remaining_duration_one_day);
        } else if (hours >= 2) {
            hours += (minutes + 30) / 60;
            return context.getString(R.string.remaining_duration_hours, hours);
        } else if (hours > 0) {
            return context.getString(R.string.remaining_duration_one_hour);
        } else if (minutes >= 2) {
            minutes += (seconds + 30) / 60;
            return context.getString(R.string.remaining_duration_minutes, minutes);
        } else if (minutes > 0) {
            return context.getString(R.string.remaining_duration_one_minute);
        } else if (seconds == 1) {
            return context.getString(R.string.remaining_duration_one_second);
        } else {
            return context.getString(R.string.remaining_duration_seconds, seconds);
        }
    }

    /**
     * Determine what String to show for a given offline page in download home.
     * @param item The offline item representing the offline page.
     * @return String representing the current status.
     */
    public static String getOfflinePageStatusString(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        switch (item.state) {
            case OfflineItemState.COMPLETE:
                return context.getString(R.string.download_notification_completed);
            case OfflineItemState.PENDING:
                return getPendingStatusString(item.pendingState);
            case OfflineItemState.PAUSED:
                return context.getString(R.string.download_notification_paused);
            case OfflineItemState.IN_PROGRESS: // intentional fall through
            case OfflineItemState.CANCELLED: // intentional fall through
            case OfflineItemState.INTERRUPTED: // intentional fall through
            case OfflineItemState.FAILED:
                break;
            // case OfflineItemState.MAX_DOWNLOAD_STATE:
            default:
                assert false : "Unexpected OfflineItemState: " + item.state;
        }

        long bytesReceived = item.receivedBytes;
        if (bytesReceived == 0) {
            return context.getString(R.string.download_started);
        }

        return DownloadUtils.getStringForDownloadedBytes(context, bytesReceived);
    }

    /**
     * Determine what String to show for a given download in download home.
     * @param item Download to check the status of.
     * @return String representing the current download status.
     */
    public static String getStatusString(DownloadItem item) {
        Context context = ContextUtils.getApplicationContext();
        DownloadInfo info = item.getDownloadInfo();
        Progress progress = info.getProgress();

        int state = info.state();
        if (state == DownloadState.COMPLETE) {
            return context.getString(R.string.download_notification_completed);
        }

        if (isDownloadPending(item)) {
            // All pending, non-offline page downloads are by default waiting for network.
            // The other pending reason (i.e. waiting for another download to complete) applies
            // only to offline page requests because offline pages download one at a time.
            return getPendingStatusString(PendingState.PENDING_NETWORK);
        } else if (isDownloadPaused(item)) {
            return context.getString(R.string.download_notification_paused);
        }

        if (info.getBytesReceived() == 0
                || (!item.isIndeterminate() && info.getTimeRemainingInMillis() < 0)) {
            // We lack enough information about the download to display a useful string.
            return context.getString(R.string.download_started);
        } else if (item.isIndeterminate()) {
            // Count up the bytes.
            long bytes = info.getBytesReceived();
            return DownloadUtils.getStringForDownloadedBytes(context, bytes);
        } else {
            // Count down the time or number of files.
            return getTimeOrFilesLeftString(context, progress, info.getTimeRemainingInMillis());
        }
    }

    /**
     * Determine the status string for a failed download.
     *
     * @param failState Reason download failed.
     * @return String representing the current download status.
     */
    public static String getFailStatusString(@FailState int failState) {
        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isFullBrowserStarted()) {
            return DownloadUtilsJni.get().getFailStateMessage(failState);
        }
        Context context = ContextUtils.getApplicationContext();
        return context.getString(R.string.download_notification_failed);
    }

    /**
     * Determine the status string for a pending download.
     *
     * @param pendingState Reason download is pending.
     * @return String representing the current download status.
     */
    public static String getPendingStatusString(@PendingState int pendingState) {
        Context context = ContextUtils.getApplicationContext();
        // When foreground service restarts and there is no connection to native, use the default
        // pending status. The status will be replaced when connected to native.
        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER).isFullBrowserStarted()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.OFFLINE_PAGES_DESCRIPTIVE_PENDING_STATUS)) {
            switch (pendingState) {
                case PendingState.PENDING_NETWORK:
                    return context.getString(R.string.download_notification_pending_network);
                case PendingState.PENDING_ANOTHER_DOWNLOAD:
                    return context.getString(
                            R.string.download_notification_pending_another_download);
                default:
                    return context.getString(R.string.download_notification_pending);
            }
        } else {
            return context.getString(R.string.download_notification_pending);
        }
    }

    /**
     * Get the resume mode based on the current fail state, to distinguish the case where download
     * cannot be resumed at all or can be resumed in the middle, or should be restarted from the
     * beginning.
     * @param url URL of the download.
     * @param failState Why the download failed.
     * @return The resume mode for the current fail state.
     */
    public static @ResumeMode int getResumeMode(String url, @FailState int failState) {
        return DownloadUtilsJni.get().getResumeMode(url, failState);
    }

    /**
     * Query the Download backends about whether a download is paused.
     *
     * The Java-side contains more information about the status of a download than is persisted
     * by the native backend, so it is queried first.
     *
     * @param item Download to check the status of.
     * @return Whether the download is paused or not.
     */
    public static boolean isDownloadPaused(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry =
                helper.getDownloadSharedPreferenceEntry(item.getContentId());

        if (entry != null) {
            // The Java downloads backend knows more about the download than the native backend.
            return !entry.isAutoResumable;
        } else {
            // Only the native downloads backend knows about the download.
            if (item.getDownloadInfo().state() == DownloadState.IN_PROGRESS) {
                return item.getDownloadInfo().isPaused();
            } else {
                return item.getDownloadInfo().state() == DownloadState.INTERRUPTED;
            }
        }
    }

    /**
     * Return whether a download is pending.
     * @param item Download to check the status of.
     * @return Whether the download is pending or not.
     */
    public static boolean isDownloadPending(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry =
                helper.getDownloadSharedPreferenceEntry(item.getContentId());
        return entry != null && item.getDownloadInfo().state() == DownloadState.INTERRUPTED
                && entry.isAutoResumable;
    }

    /**
     * Format the number of bytes into KB, or MB, or GB and return the corresponding string
     * resource. Uses default download-related set of strings.
     * @param context Context to use.
     * @param bytes Number of bytes.
     * @return A formatted string to be displayed.
     */
    public static String getStringForDownloadedBytes(Context context, long bytes) {
        return getStringForBytes(context, BYTES_DOWNLOADED_STRINGS, bytes);
    }

    /**
     * Format the number of available bytes into KB, MB, or GB and return the corresponding string
     * resource. Uses default format "20 KB available."
     *
     * @param context   Context to use.
     * @param bytes     Number of bytes needed to display.
     * @return          The formatted string to be displayed.
     */
    public static String getStringForAvailableBytes(Context context, long bytes) {
        return getStringForBytes(context, BYTES_AVAILABLE_STRINGS, bytes);
    }

    /**
     * Format the number of bytes into KB, MB, or GB and return the corresponding generated string.
     * @param context Context to use.
     * @param bytes   Number of bytes needed to display.
     * @return        The formatted string to be displayed.
     */
    public static String getStringForBytes(Context context, long bytes) {
        return getStringForBytes(context, BYTES_STRINGS, bytes);
    }

    /**
     * Format the number of bytes into KB, or MB, or GB and return the corresponding string
     * resource.
     * @param context Context to use.
     * @param stringSet The string resources for displaying bytes in KB, MB and GB.
     * @param bytes Number of bytes.
     * @return A formatted string to be displayed.
     */
    public static String getStringForBytes(Context context, int[] stringSet, long bytes) {
        int resourceId;
        float bytesInCorrectUnits;

        if (ConversionUtils.bytesToMegabytes(bytes) < 1) {
            resourceId = stringSet[0];
            bytesInCorrectUnits = bytes / (float) ConversionUtils.BYTES_PER_KILOBYTE;
        } else if (ConversionUtils.bytesToGigabytes(bytes) < 1) {
            resourceId = stringSet[1];
            bytesInCorrectUnits = bytes / (float) ConversionUtils.BYTES_PER_MEGABYTE;
        } else {
            resourceId = stringSet[2];
            bytesInCorrectUnits = bytes / (float) ConversionUtils.BYTES_PER_GIGABYTE;
        }

        return context.getResources().getString(resourceId, bytesInCorrectUnits);
    }

    /**
     * Abbreviate a file name into a given number of characters with ellipses.
     * e.g. "thisisaverylongfilename.txt" => "thisisave....txt".
     * @param fileName File name to abbreviate.
     * @param limit Character limit.
     * @return Abbreviated file name.
     */
    public static String getAbbreviatedFileName(String fileName, int limit) {
        assert limit >= 1;  // Abbreviated file name should at least be 1 characters (a...)

        if (TextUtils.isEmpty(fileName) || fileName.length() <= limit) return fileName;

        // Find the file name extension
        int index = fileName.lastIndexOf(".");
        int extensionLength = fileName.length() - index;

        // If the extension is too long, just use truncate the string from beginning.
        if (extensionLength >= limit) {
            return fileName.substring(0, limit) + ELLIPSIS;
        }
        int remainingLength = limit - extensionLength;
        return fileName.substring(0, remainingLength) + ELLIPSIS + fileName.substring(index);
    }

    /**
     * Return an icon for a given file type.
     * @param fileType Type of the file as returned by DownloadFilter.
     * @param iconSize Size of the returned icon.
     * @return Resource ID of the corresponding icon.
     */
    public static int getIconResId(int fileType, @IconSize int iconSize) {
        // TODO(huayinz): Make image view size same as icon size so that 36dp icons can be removed.
        switch (fileType) {
            case DownloadFilter.Type.PAGE:
                return iconSize == IconSize.DP_24 ? R.drawable.ic_globe_24dp
                                                  : R.drawable.ic_globe_36dp;
            case DownloadFilter.Type.VIDEO:
                return iconSize == IconSize.DP_24 ? R.drawable.ic_videocam_24dp
                                                  : R.drawable.ic_videocam_36dp;
            case DownloadFilter.Type.AUDIO:
                return iconSize == IconSize.DP_24 ? R.drawable.ic_music_note_24dp
                                                  : R.drawable.ic_music_note_36dp;
            case DownloadFilter.Type.IMAGE:
                return iconSize == IconSize.DP_24 ? R.drawable.ic_drive_image_24dp
                                                  : R.drawable.ic_drive_image_36dp;
            case DownloadFilter.Type.DOCUMENT:
                return iconSize == IconSize.DP_24 ? R.drawable.ic_drive_document_24dp
                                                  : R.drawable.ic_drive_document_36dp;
            default:
                return iconSize == IconSize.DP_24 ? R.drawable.ic_drive_file_24dp
                                                  : R.drawable.ic_drive_file_36dp;
        }
    }

    /**
     * Return a background color for the file type icon.
     * @param context Context from which to extract the resources.
     * @return Background color.
     */
    public static int getIconBackgroundColor(Context context) {
        return ApiCompatibilityUtils.getColor(context.getResources(), R.color.light_active_color);
    }

    /**
     * Return a foreground color list for the file type icon.
     * @param context Context from which to extract the resources.
     * @return a foreground color list.
     */
    public static ColorStateList getIconForegroundColorList(Context context) {
        return AppCompatResources.getColorStateList(context, R.color.white_mode_tint);
    }

    /**
     * Return if a download item is already viewed by the user. Will return false if the last
     * access time is not available.
     * @param item The download item.
     * @return true if the item is viewed by the user.
     */
    public static boolean isDownloadViewed(DownloadItem item) {
        if (item == null || item.getDownloadInfo() == null) return false;
        return item.getDownloadInfo().getLastAccessTime() != 0;
    }

    /**
     * Returns |true| if the offline item is not null and has already been viewed by the user.
     * @param offlineItem The offline item to check.
     * @return true if the item is valid has been viewed by the user.
     */
    public static boolean isOfflineItemViewed(OfflineItem offlineItem) {
        return offlineItem != null && offlineItem.lastAccessedTimeMs > offlineItem.completionTimeMs;
    }

    /**
     * Given two timestamps, calculates if both occur on the same date.
     * @return True if they belong in the same day. False otherwise.
     */
    public static boolean isSameDay(long timestamp1, long timestamp2) {
        return getDateAtMidnight(timestamp1).equals(getDateAtMidnight(timestamp2));
    }

    /**
     * Calculates the {@link Date} for midnight of the date represented by the |timestamp|.
     */
    public static Date getDateAtMidnight(long timestamp) {
        Calendar cal = Calendar.getInstance();
        cal.setTimeInMillis(timestamp);
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);
        return cal.getTime();
    }

    /**
     * Returns if the path is in the download directory on primary storage.
     * @param path The directory to check.
     * @return If the path is in the download directory on primary storage.
     */
    public static boolean isInPrimaryStorageDownloadDirectory(String path) {
        // Only primary storage can have content URI as file path.
        if (ContentUriUtils.isContentUri(path)) return true;

        // Check if the file path contains the external public directory.
        File primaryDir = null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            primaryDir = Environment.getExternalStorageDirectory();
        }
        if (primaryDir == null || path == null) return false;
        String primaryPath = primaryDir.getAbsolutePath();
        return primaryPath == null ? false : path.contains(primaryPath);
    }

    /**
     * @return The status of prompt for download pref, defined by {@link DownloadPromptStatus}.
     */
    @DownloadPromptStatus
    public static int getPromptForDownloadAndroid() {
        return PrefServiceBridge.getInstance().getInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public static void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        PrefServiceBridge.getInstance().setInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, status);
    }

    @NativeMethods
    interface Natives {
        String getFailStateMessage(@FailState int failState);
        int getResumeMode(String url, @FailState int failState);
    }
}
