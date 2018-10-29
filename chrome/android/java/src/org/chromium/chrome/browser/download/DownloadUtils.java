// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.net.Uri;
import android.os.Environment;
import android.os.StrictMode;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.home.metrics.FileExtensions;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.ui.DownloadFilter;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.OfflineItemWrapper;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.util.IntentUtils;
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
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.TimeUnit;

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
    static final long SECONDS_PER_MINUTE = TimeUnit.MINUTES.toSeconds(1);
    @VisibleForTesting
    static final long SECONDS_PER_HOUR = TimeUnit.HOURS.toSeconds(1);
    @VisibleForTesting
    static final long SECONDS_PER_DAY = TimeUnit.DAYS.toSeconds(1);

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
     * @return Whether the UI was shown.
     */
    public static boolean showDownloadManager(@Nullable Activity activity, @Nullable Tab tab) {
        return showDownloadManager(activity, tab, false);
    }

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param showPrefetchedContent Whether the manager should start with prefetched content section
     * expanded.
     * @return Whether the UI was shown.
     */
    public static boolean showDownloadManager(
            @Nullable Activity activity, @Nullable Tab tab, boolean showPrefetchedContent) {
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
                Intent intent = Tab.createBringTabToFrontIntent(tab.getId());
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
                        .isStartupSuccessfullyCompleted()) {
            Profile profile = (tab == null ? Profile.getLastUsedProfile() : tab.getProfile());
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.notifyEvent(EventConstants.DOWNLOAD_HOME_OPENED);
        }

        return true;
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
        RecordHistogram.recordPercentageHistogram("OfflinePages.SavePage.PercentLoaded",
                tab.getProgress());
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
     * Creates an Intent to share {@code items} with another app by firing an Intent to Android.
     *
     * Sharing a DownloadItem shares the file itself. Sharing an OfflinePageItem shares the archive
     * file if the sharing is enabled. Otherwise, the URL is shared.
     *
     * @param items Items to share.
     * @param newOfflineFilePathMap Map of id to new file path for those offline pages that are
     *        published before sharing.
     * @return      Intent that can be used to share the items.
     */
    public static Intent createShareIntent(
            List<DownloadHistoryItemWrapper> items, Map<String, String> newOfflineFilePathMap) {
        Intent shareIntent = new Intent();
        String intentAction;
        ArrayList<Uri> itemUris = new ArrayList<Uri>();
        StringBuilder offlinePagesString = new StringBuilder();
        @DownloadFilter.Type
        int selectedItemsFilterType = items.get(0).getFilterType();

        String intentMimeType = "";
        String[] intentMimeParts = {"", ""};

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity != null && activity instanceof ChromeTabbedActivity) {
            ChromeTabbedActivity chromeActivity = ((ChromeTabbedActivity) activity);
            ScreenshotTabObserver tabObserver =
                    ScreenshotTabObserver.from(chromeActivity.getActivityTab());
            if (tabObserver != null) {
                tabObserver.onActionPerformedAfterScreenshot(
                        ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
            }
        }

        for (int i = 0; i < items.size(); i++) {
            DownloadHistoryItemWrapper wrappedItem  = items.get(i);
            String mimeType = Intent.normalizeMimeType(wrappedItem.getMimeType());

            if (wrappedItem.isOfflinePage()) {
                // Attempt to share the mhtml file.  If that fails, share by URL.
                OfflineItemWrapper wrappedOfflineItem = (OfflineItemWrapper) wrappedItem;
                Uri uriToShare =
                        getUriToShareOfflinePage(wrappedOfflineItem, newOfflineFilePathMap);

                if (uriToShare == null) {
                    // Share the URL, instead of the file, if publishing the file failed.
                    if (offlinePagesString.length() != 0) {
                        offlinePagesString.append("\n");
                    }
                    offlinePagesString.append(wrappedItem.getUrl());
                    mimeType = MIME_TYPE_SHARING_URL;
                } else {
                    itemUris.add(uriToShare);
                    RecordUserAction.record("OfflinePages.Sharing.SharePageFromDownloadHome");
                }
            } else {
                // If not sharing an offline page, generate the URI for the file being shared.
                itemUris.add(getUriForItem(wrappedItem.getFile()));
            }

            if (selectedItemsFilterType != wrappedItem.getFilterType()) {
                selectedItemsFilterType = DownloadFilter.Type.ALL;
            }
            if (wrappedItem.getFilterType() == DownloadFilter.Type.OTHER) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.DownloadManager.OtherExtensions.Share",
                        wrappedItem.getFileExtensionType(), FileExtensions.Type.NUM_ENTRIES);
            }

            // If a mime type was not retrieved from the backend or could not be normalized,
            // set the mime type to the default.
            if (TextUtils.isEmpty(mimeType)) {
                intentMimeType = DEFAULT_MIME_TYPE;
                continue;
            }

            // If the intent mime type has not been set yet, set it to the mime type for this item.
            if (TextUtils.isEmpty(intentMimeType)) {
                intentMimeType = mimeType;
                if (!TextUtils.isEmpty(intentMimeType)) {
                    intentMimeParts = intentMimeType.split(MIME_TYPE_DELIMITER);
                    // Guard against invalid mime types.
                    if (intentMimeParts.length != 2) intentMimeType = DEFAULT_MIME_TYPE;
                }
                continue;
            }

            // Either the mime type is already the default or it matches the current item's mime
            // type. In either case, intentMimeType is already the correct value.
            if (TextUtils.equals(intentMimeType, DEFAULT_MIME_TYPE)
                    || TextUtils.equals(intentMimeType, mimeType)) {
                continue;
            }

            String[] mimeParts = mimeType.split(MIME_TYPE_DELIMITER);
            if (!TextUtils.equals(intentMimeParts[0], mimeParts[0])) {
                // The top-level types don't match; fallback to the default mime type.
                intentMimeType = DEFAULT_MIME_TYPE;
            } else {
                // The mime type should be {top-level type}/*
                intentMimeType = intentMimeParts[0] + MIME_TYPE_DELIMITER + "*";
            }
        }

        // Use Action_SEND if there is only one downloaded item or only text to share.
        if (itemUris.size() == 0 || (itemUris.size() == 1 && offlinePagesString.length() == 0)) {
            intentAction = Intent.ACTION_SEND;
        } else {
            intentAction = Intent.ACTION_SEND_MULTIPLE;
        }

        if (itemUris.size() == 1) {
            // Sharing a downloaded item or an offline page.
            shareIntent.putExtra(Intent.EXTRA_STREAM, itemUris.get(0));
        } else if (itemUris.size() > 1) {
            shareIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, itemUris);
        }

        if (offlinePagesString.length() != 0) {
            shareIntent.putExtra(Intent.EXTRA_TEXT, offlinePagesString.toString());
        }

        // If there is exactly one item shared, set the mail title.
        if (items.size() == 1) {
            shareIntent.putExtra(Intent.EXTRA_SUBJECT, items.get(0).getDisplayFileName());
        }

        shareIntent.setAction(intentAction);
        shareIntent.setType(intentMimeType);
        shareIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        recordShareHistograms(items.size(), selectedItemsFilterType);

        return shareIntent;
    }

    /**
     * Compute the URI to use for sharing this page.
     * @param wrappedOfflineItem OfflineItem to be shared.
     * @param newOfflineFilePathMap map of offline id to the file path now that publishing is done.
     * @return Uri to use for sharing this offline page, or null if we cannot build one.
     */
    private static Uri getUriToShareOfflinePage(
            OfflineItemWrapper wrappedOfflineItem, Map<String, String> newOfflineFilePathMap) {
        if (!OfflinePageBridge.isPageSharingEnabled()) {
            return null;
        }
        String newFilePath = wrappedOfflineItem.getFilePath();

        if (wrappedOfflineItem.isSuggested()) {
            // If we have a temporary page, share it by content URI.  Today this only
            // supports suggested pages, since they are the only type of temporary pages
            // shown in DownloadsHome.  If we support other types of pages someday,
            // we'll need to add support for them here too.
            try {
                return (new FileProviderHelper()).getContentUriFromFile(new File(newFilePath));
            } catch (Exception e) {
                return null;
            }
        }

        if (newOfflineFilePathMap == null) {
            // If the file was already in the public directory, use the existing file path.
            return getUriForItem(wrappedOfflineItem.getFile());
        }

        String publishedFilePath = newOfflineFilePathMap.get(wrappedOfflineItem.getId());
        if (!TextUtils.isEmpty(publishedFilePath)) {
            // If we moved the file to publish it, use the new path.
            return getUriForItem(new File(publishedFilePath));
        }

        // If publishing failed, return null, and we will share by original URL.
        return null;
    }

    /**
     * Performs all the necessary work needed to share download items. For offline pages, we may
     * need to publish the internal archive file to public location first.
     *
     * @param items Items to share.
     * @return True if the work is done or not needed and the sharing can start immediately.
     *         False if the asynchronous work is in progress. After it is done, |callback| will be
     *         invoked to inform the result.
     */
    public static boolean prepareForSharing(
            List<DownloadHistoryItemWrapper> items, Callback<Map<String, String>> callback) {
        if (!OfflinePageBridge.isPageSharingEnabled()) return true;

        OfflinePageBridge offlinePageBridge =
                OfflinePageBridge.getForProfile(Profile.getLastUsedProfile().getOriginalProfile());

        // If the sharing of offline pages is enabled, we need to publish the archive files if they
        // are still located in the internal directory, and not temporary pages.
        List<OfflineItemWrapper> offlinePagesToPublish = new ArrayList<OfflineItemWrapper>();
        for (int i = 0; i < items.size(); i++) {
            DownloadHistoryItemWrapper wrappedItem = items.get(i);
            if (wrappedItem.isOfflinePage()) {
                OfflineItemWrapper wrappedOfflineItem = (OfflineItemWrapper) wrappedItem;
                if (!wrappedOfflineItem.isSuggested()
                        && offlinePageBridge.isInPrivateDirectory(
                                   wrappedOfflineItem.getFilePath())) {
                    offlinePagesToPublish.add(wrappedOfflineItem);
                }
            }
        }

        if (offlinePagesToPublish.isEmpty()) return true;

        publishOfflinePagesForSharing(offlinePageBridge, offlinePagesToPublish, callback);
        return false;
    }

    static void publishOfflinePagesForSharing(OfflinePageBridge offlinePageBridge,
            List<OfflineItemWrapper> offlinePages, Callback<Map<String, String>> callback) {
        DownloadController.requestFileAccessPermission(granted -> {
            if (!granted) {
                OfflinePageUtils.recordPublishPageResult(SavePageResult.PERMISSION_DENIED);
                return;
            }
            publishOfflinePageForSharing(
                    offlinePageBridge, offlinePages, 0, new HashMap<String, String>(), callback);
        });
    }

    static void publishOfflinePageForSharing(OfflinePageBridge offlinePageBridge,
            final List<OfflineItemWrapper> offlinePages, final int index,
            final Map<String, String> newFilePathMap, Callback<Map<String, String>> callback) {
        assert index < offlinePages.size();

        final OfflineItemWrapper wrappedItem = offlinePages.get(index);
        assert wrappedItem.isOfflinePage();

        offlinePageBridge.publishInternalPageByGuid(wrappedItem.getId(), (newFilePath) -> {
            if (!newFilePath.isEmpty()) {
                newFilePathMap.put(wrappedItem.getId(), newFilePath);
            }

            int nextIndex = index + 1;
            if (nextIndex >= offlinePages.size()) {
                callback.onResult(newFilePathMap);
                return;
            }

            publishOfflinePageForSharing(
                    offlinePageBridge, offlinePages, nextIndex, newFilePathMap, callback);
        });
    }

    /**
     * Returns a URI that points at the file.
     * @param file File to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    public static Uri getUriForItem(File file) {
        Uri uri = null;

        // FileUtils.getUriForFile() causes a disk read when it calls into
        // FileProvider#getUriForFile. Obtaining a content URI is on the critical path for creating
        // a share intent after the user taps on the share button, so even if we were to run this
        // method on a background thread we would have to wait. As it depends on user-selected
        // items, we cannot know/preload which URIs we need until the user presses share.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        uri = FileUtils.getUriForFile(file);
        StrictMode.setThreadPolicy(oldPolicy);

        return uri;
    }

    @CalledByNative
    private static String getUriStringForPath(String filePath) {
        Uri uri = null;
        File file = new File(filePath);
        uri = getUriForItem(file);

        return uri != null ? uri.toString() : new String();
    }

    /**
     * Utility method to open an {@link OfflineItem}, which can be a chrome download, offline page.
     * Falls back to open download home.
     * @param contentId The {@link ContentId} of the associated offline item.
     */
    public static void openItem(ContentId contentId, boolean isOffTheRecord,
            @DownloadMetrics.DownloadOpenSource int source) {
        if (LegacyHelpers.isLegacyOfflinePage(contentId)) {
            OfflineContentAggregatorFactory.forProfile(Profile.getLastUsedProfile())
                    .openItem(LaunchLocation.PROGRESS_BAR, contentId);
        } else {
            DownloadManagerService.getDownloadManagerService().openDownload(
                    contentId, isOffTheRecord, source);
        }
    }

    /**
     * Opens a file in Chrome or in another app if appropriate.
     * @param file path to the file to open.
     * @param mimeType mime type of the file.
     * @param downloadGuid The associated download GUID.
     * @param isOffTheRecord whether we are in an off the record context.
     * @param originalUrl The original url of the downloaded file.
     * @param referrer Referrer of the downloaded file.
     * @param source The source that tries to open the download file.
     * @return whether the file could successfully be opened.
     */
    public static boolean openFile(File file, String mimeType, String downloadGuid,
            boolean isOffTheRecord, String originalUrl, String referrer,
            @DownloadMetrics.DownloadOpenSource int source) {
        DownloadMetrics.recordDownloadOpen(source, mimeType);
        Context context = ContextUtils.getApplicationContext();
        DownloadManagerService service = DownloadManagerService.getDownloadManagerService();

        // Check if Chrome should open the file itself.
        if (service.isDownloadOpenableInBrowser(isOffTheRecord, mimeType)) {
            // Share URIs use the content:// scheme when able, which looks bad when displayed
            // in the URL bar.
            Uri fileUri = Uri.fromFile(file);
            Uri contentUri = getUriForItem(file);
            String normalizedMimeType = Intent.normalizeMimeType(mimeType);

            Intent intent = MediaViewerUtils.getMediaViewerIntent(
                    fileUri, contentUri, normalizedMimeType, true /* allowExternalAppHandlers */);
            IntentHandler.startActivityForTrustedIntent(intent);
            service.updateLastAccessTime(downloadGuid, isOffTheRecord);
            return true;
        }

        // Check if any apps can open the file.
        try {
            // TODO(qinmin): Move this to an AsyncTask so we don't need to temper with strict mode.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            Uri uri = ApiCompatibilityUtils.getUriForDownloadedFile(file);
            StrictMode.setThreadPolicy(oldPolicy);
            Intent viewIntent =
                    MediaViewerUtils.createViewIntentForUri(uri, mimeType, originalUrl, referrer);
            context.startActivity(viewIntent);
            service.updateLastAccessTime(downloadGuid, isOffTheRecord);
            return true;
        } catch (ActivityNotFoundException e) {
            // Can't launch the Intent.
            if (source != DownloadMetrics.DownloadOpenSource.DOWNLOAD_PROGRESS_INFO_BAR) {
                Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                             Toast.LENGTH_SHORT)
                        .show();
            }
            return false;
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
        if (secondsLong >= SECONDS_PER_DAY) {
            days = (int) (secondsLong / SECONDS_PER_DAY);
            secondsLong -= days * SECONDS_PER_DAY;
        }
        if (secondsLong >= SECONDS_PER_HOUR) {
            hours = (int) (secondsLong / SECONDS_PER_HOUR);
            secondsLong -= hours * SECONDS_PER_HOUR;
        }
        if (secondsLong >= SECONDS_PER_MINUTE) {
            minutes = (int) (secondsLong / SECONDS_PER_MINUTE);
            secondsLong -= minutes * SECONDS_PER_MINUTE;
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
                        .isStartupSuccessfullyCompleted()) {
            return nativeGetFailStateMessage(failState);
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
        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()
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
     * @param failState Why the download failed.
     * @return The resume mode for the current fail state.
     */
    public static @ResumeMode int getResumeMode(@FailState int failState) {
        return nativeGetResumeMode(failState);
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
        File primaryDir = null;
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            primaryDir = Environment.getExternalStorageDirectory();
        }
        if (primaryDir == null || path == null) return false;
        String primaryPath = primaryDir.getAbsolutePath();
        return primaryPath == null ? false : path.contains(primaryPath);
    }

    private static native String nativeGetFailStateMessage(@FailState int failState);
    private static native int nativeGetResumeMode(@FailState int failState);
}
