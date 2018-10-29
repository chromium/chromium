// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.Activity;
import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.support.annotation.IntDef;
import android.text.TextUtils;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * A class holding static util functions for offline pages.
 */
public class OfflinePageUtils {
    private static final String TAG = "OfflinePageUtils";
    /** Background task tag to differentiate from other task types */
    public static final String TASK_TAG = "OfflinePageUtils";

    private static final int DEFAULT_SNACKBAR_DURATION_MS = 6 * 1000; // 6 second

    /**
     * Bit flags to be OR-ed together to build the context of a tab restore to be used to identify
     * the appropriate TabRestoreType in a lookup table.
     */
    private static final int BIT_ONLINE = 1;
    private static final int BIT_CANT_SAVE_OFFLINE = 1 << 2;
    private static final int BIT_OFFLINE_PAGE = 1 << 3;
    private static final int BIT_LAST_N = 1 << 4;

    // Used instead of the constant so tests can override the value.
    private static int sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;

    /** Instance carrying actual implementation of utility methods. */
    private static Internal sInstance;

    /**
     * Tracks the observers of ChromeActivity's TabModelSelectors. This is weak so the activity can
     * be garbage collected without worrying about this map.  The RecentTabTracker is held here so
     * that it can be destroyed when the ChromeActivity gets a new TabModelSelector.
     */
    private static Map<ChromeActivity, RecentTabTracker> sTabModelObservers = new HashMap<>();

    /**
     * Interface for implementation of offline page utilities, that can be implemented for testing.
     * We are using an internal interface, so that instance methods can have the same names as
     * static methods.
     */
    @VisibleForTesting
    interface Internal {
        /** Returns offline page bridge for specified profile. */
        OfflinePageBridge getOfflinePageBridge(Profile profile);

        /** Returns whether the network is connected. */
        boolean isConnected();

        /**
         * Checks if an offline page is shown for the tab. This page could be either trusted or
         * untrusted.
         * @param tab The tab to be reloaded.
         * @return True if the offline page is opened.
         */
        boolean isOfflinePage(Tab tab);

        /**
         * Returns whether the tab is showing trusted offline page.
         * @param tab The current tab.
         * @return True if a trusted offline page is shown in the tab.
         */
        boolean isShowingTrustedOfflinePage(Tab tab);

        /**
         * Returns whether the tab is showing offline preview.
         * @param tab The current tab.
         */
        boolean isShowingOfflinePreview(Tab tab);

        /**
         * Shows the "reload" snackbar for the given tab.
         * @param context The application context.
         * @param snackbarManager Class that shows the snackbar.
         * @param snackbarController Class to control the snackbar.
         * @param tabId Id of a tab that the snackbar is related to.
         */
        void showReloadSnackbar(Context context, SnackbarManager snackbarManager,
                final SnackbarController snackbarController, int tabId);
    }

    private static class OfflinePageUtilsImpl implements Internal {
        @Override
        public OfflinePageBridge getOfflinePageBridge(Profile profile) {
            return OfflinePageBridge.getForProfile(profile);
        }

        @Override
        public boolean isConnected() {
            return NetworkChangeNotifier.isOnline();
        }

        @Override
        public boolean isOfflinePage(Tab tab) {
            if (tab == null) return false;

            WebContents webContents = tab.getWebContents();
            if (webContents == null) return false;

            OfflinePageBridge offlinePageBridge =
                    getInstance().getOfflinePageBridge(tab.getProfile());
            if (offlinePageBridge == null) return false;

            return offlinePageBridge.isOfflinePage(webContents);
        }

        @Override
        public boolean isShowingOfflinePreview(Tab tab) {
            OfflinePageBridge offlinePageBridge = getOfflinePageBridge(tab.getProfile());
            if (offlinePageBridge == null) return false;
            return offlinePageBridge.isShowingOfflinePreview(tab.getWebContents());
        }

        @Override
        public void showReloadSnackbar(Context context, SnackbarManager snackbarManager,
                final SnackbarController snackbarController, int tabId) {
            if (tabId == Tab.INVALID_TAB_ID) return;

            Log.d(TAG, "showReloadSnackbar called with controller " + snackbarController);
            Snackbar snackbar =
                    Snackbar.make(context.getString(R.string.offline_pages_viewing_offline_page),
                                    snackbarController, Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_OFFLINE_PAGE_RELOAD)
                            .setSingleLine(false)
                            .setAction(context.getString(R.string.reload), tabId);
            snackbar.setDuration(sSnackbarDurationMs);
            snackbarManager.showSnackbar(snackbar);
        }

        @Override
        public boolean isShowingTrustedOfflinePage(Tab tab) {
            if (tab == null) return false;

            WebContents webContents = tab.getWebContents();
            if (webContents == null) return false;

            OfflinePageBridge offlinePageBridge = getOfflinePageBridge(tab.getProfile());
            if (offlinePageBridge == null) return false;
            return offlinePageBridge.isShowingTrustedOfflinePage(tab.getWebContents());
        }
    }

    /**
     * Contains values from the histogram enum OfflinePagesTabRestoreType used for reporting the
     * OfflinePages.TabRestore metric.
     */
    @IntDef({TabRestoreType.WHILE_ONLINE, TabRestoreType.WHILE_ONLINE_CANT_SAVE_FOR_OFFLINE_USAGE,
            TabRestoreType.WHILE_ONLINE_TO_OFFLINE_PAGE,
            TabRestoreType.WHILE_ONLINE_TO_OFFLINE_PAGE_FROM_LAST_N, TabRestoreType.WHILE_OFFLINE,
            TabRestoreType.WHILE_OFFLINE_CANT_SAVE_FOR_OFFLINE_USAGE,
            TabRestoreType.WHILE_OFFLINE_TO_OFFLINE_PAGE,
            TabRestoreType.WHILE_OFFLINE_TO_OFFLINE_PAGE_FROM_LAST_N, TabRestoreType.FAILED,
            TabRestoreType.CRASHED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabRestoreType {
        int WHILE_ONLINE = 0;
        int WHILE_ONLINE_CANT_SAVE_FOR_OFFLINE_USAGE = 1;
        int WHILE_ONLINE_TO_OFFLINE_PAGE = 2;
        int WHILE_ONLINE_TO_OFFLINE_PAGE_FROM_LAST_N = 3;
        int WHILE_OFFLINE = 4;
        int WHILE_OFFLINE_CANT_SAVE_FOR_OFFLINE_USAGE = 5;
        int WHILE_OFFLINE_TO_OFFLINE_PAGE = 6;
        int WHILE_OFFLINE_TO_OFFLINE_PAGE_FROM_LAST_N = 7;
        int FAILED = 8;
        int CRASHED = 9;
        // NOTE: always keep this entry at the end. Add new result types only immediately above this
        // line. Make sure to update the corresponding histogram enum accordingly.
        int NUM_ENTRIES = 10;
    }

    private static Internal getInstance() {
        if (sInstance == null) {
            sInstance = new OfflinePageUtilsImpl();
        }
        return sInstance;
    }

    /**
     * Returns the number of free bytes on the storage.
     */
    public static long getFreeSpaceInBytes() {
        return Environment.getDataDirectory().getUsableSpace();
    }

    /**
     * Returns the number of total bytes on the storage.
     */
    public static long getTotalSpaceInBytes() {
        return Environment.getDataDirectory().getTotalSpace();
    }

    /** Returns whether the network is connected. */
    public static boolean isConnected() {
        return getInstance().isConnected();
    }

    /*
     * Save an offline copy for the bookmarked page asynchronously.
     *
     * @param bookmarkId The ID of the page to save an offline copy.
     * @param tab A {@link Tab} object.
     * @param callback The callback to be invoked when the offline copy is saved.
     */
    public static void saveBookmarkOffline(BookmarkId bookmarkId, Tab tab) {
        // If bookmark ID is missing there is nothing to save here.
        if (bookmarkId == null) return;

        // Making sure the feature is enabled.
        if (!OfflinePageBridge.isOfflineBookmarksEnabled()) return;

        // Making sure tab is worth keeping.
        if (shouldSkipSavingTabOffline(tab)) return;

        OfflinePageBridge offlinePageBridge = getInstance().getOfflinePageBridge(tab.getProfile());
        if (offlinePageBridge == null) return;

        WebContents webContents = tab.getWebContents();
        ClientId clientId = ClientId.createClientIdForBookmarkId(bookmarkId);

        offlinePageBridge.savePage(webContents, clientId, new OfflinePageBridge.SavePageCallback() {
            @Override
            public void onSavePageDone(int savePageResult, String url, long offlineId) {
                // Result of the call is ignored.
            }
        });
    }

    /**
     * Indicates whether we should skip saving the given tab as an offline page.
     * A tab shouldn't be saved offline if it shows an error page or a sad tab page.
     */
    private static boolean shouldSkipSavingTabOffline(Tab tab) {
        WebContents webContents = tab.getWebContents();
        return tab.isShowingErrorPage() || SadTab.isShowing(tab) || webContents == null
                || webContents.isDestroyed() || webContents.isIncognito();
    }

    /**
     * Shows the snackbar for the current tab to provide offline specific information if needed.
     * @param tab The current tab.
     */
    public static void showOfflineSnackbarIfNecessary(Tab tab) {
        // Set up the tab observer to watch for the tab being shown (not hidden) and a valid
        // connection. When both conditions are met a snackbar is shown.
        OfflinePageTabObserver.addObserverForTab(tab);
    }

    protected void showReloadSnackbarInternal(Context context, SnackbarManager snackbarManager,
            final SnackbarController snackbarController, int tabId) {}

    /**
     * Shows the "reload" snackbar for the given tab.
     * @param context The application context.
     * @param snackbarManager Class that shows the snackbar.
     * @param snackbarController Class to control the snackbar.
     * @param tabId Id of a tab that the snackbar is related to.
     */
    public static void showReloadSnackbar(Context context, SnackbarManager snackbarManager,
            final SnackbarController snackbarController, int tabId) {
        getInstance().showReloadSnackbar(context, snackbarManager, snackbarController, tabId);
    }

    /**
     * Records UMA data when the Offline Pages Background Load service awakens.
     * @param context android context
     */
    public static void recordWakeupUMA(Context context, long taskScheduledTimeMillis) {
        DeviceConditions deviceConditions = DeviceConditions.getCurrent(context);
        if (deviceConditions == null) return;

        // Report charging state.
        RecordHistogram.recordBooleanHistogram(
                "OfflinePages.Wakeup.ConnectedToPower", deviceConditions.isPowerConnected());

        // Report battery percentage.
        RecordHistogram.recordPercentageHistogram(
                "OfflinePages.Wakeup.BatteryPercentage", deviceConditions.getBatteryPercentage());

        // Report the default network found (or none, if we aren't connected).
        int connectionType = deviceConditions.getNetConnectionType();
        Log.d(TAG, "Found default network of type " + connectionType);
        RecordHistogram.recordEnumeratedHistogram("OfflinePages.Wakeup.NetworkAvailable",
                connectionType, ConnectionType.CONNECTION_LAST + 1);

        // Collect UMA on the time since the request started.
        long nowMillis = System.currentTimeMillis();
        long delayInMilliseconds = nowMillis - taskScheduledTimeMillis;
        if (delayInMilliseconds <= 0) {
            return;
        }
        RecordHistogram.recordLongTimesHistogram(
                "OfflinePages.Wakeup.DelayTime",
                delayInMilliseconds,
                TimeUnit.MILLISECONDS);
    }

    /**
     * Records UMA data for publishing internal page during sharing.
     * Most of the recording are in JNI layer, since it's a point that can be used by both ways of
     * sharing a page.
     * TODO(romax): See if we can merge that.
     * @param result The result for publishing file.
     */
    public static void recordPublishPageResult(int result) {
        // TODO(https://crbug.com/894714): Find a safer way to define the boundary value when
        // using MAX_VALUE.
        RecordHistogram.recordEnumeratedHistogram("OfflinePages.Sharing.PublishInternalPageResult",
                result, SavePageResult.MAX_VALUE + 1);
    }

    /**
     * Save the page loaded in current tab and share the saved page.
     *
     * @param activity The activity used for sharing and file provider interaction.
     * @param currentTab The current tab from which the page is being shared.
     * @param shareCallback The callback to be used to send the ShareParams. This will only be
     *                      called if this function call returns true.
     * @return true if the sharing of the page is possible. The callback will be invoked if
     *                      saving the page succeeds.
     */
    public static boolean saveAndSharePage(
            final Activity activity, Tab tab, final Callback<ShareParams> shareCallback) {
        OfflinePageBridge offlinePageBridge = getInstance().getOfflinePageBridge(tab.getProfile());

        if (offlinePageBridge == null) {
            Log.e(TAG, "Unable to share current tab as an offline page.");
            return false;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        GetPagesByNamespaceForLivePageSharingCallback callback =
                new GetPagesByNamespaceForLivePageSharingCallback(
                        activity, tab, shareCallback, offlinePageBridge);
        offlinePageBridge.getPagesByNamespace(
                OfflinePageBridge.LIVE_PAGE_SHARING_NAMESPACE, callback);

        return true;
    }

    /**
     * If possible, creates the ShareParams needed to share the current offline page loaded in the
     * provided tab as a MHTML file.
     *
     * @param activity The activity used for sharing and file provider interaction.
     * @param currentTab The current tab from which the page is being shared.
     * @param shareCallback The callback to be used to send the ShareParams. This will only be
     *                      called if this function call returns true.
     * @return true if the sharing of the page is possible.  The callback will be invoked if
     *                      publishing the page succeeds.
     */
    public static boolean maybeShareOfflinePage(
            final Activity activity, Tab tab, final Callback<ShareParams> shareCallback) {
        if (!OfflinePageBridge.isPageSharingEnabled()) return false;

        if (tab == null) return false;

        boolean isOfflinePage = OfflinePageUtils.isOfflinePage(tab);
        RecordHistogram.recordBooleanHistogram("OfflinePages.SharedPageWasOffline", isOfflinePage);

        // If the current tab is not showing an offline page, try to see if we should do live page
        // sharing.
        if (!isOfflinePage) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_PAGES_LIVE_PAGE_SHARING)) {
                return saveAndSharePage(activity, tab, shareCallback);
            } else {
                return false;
            }
        }

        OfflinePageBridge offlinePageBridge = getInstance().getOfflinePageBridge(tab.getProfile());

        if (offlinePageBridge == null) {
            Log.e(TAG, "Unable to share current tab as an offline page.");
            return false;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        OfflinePageItem offlinePage = offlinePageBridge.getOfflinePage(webContents);
        if (offlinePage == null) return false;

        String offlinePath = offlinePage.getFilePath();

        final String pageUrl = tab.getUrl();
        // We share temporary pages by content URI to prevent unanticipated side effects in the
        // public directory. Temporary pages are ones not in a user requested download namespace.
        Uri uri;
        boolean isPageUserRequested = offlinePageBridge.isUserRequestedDownloadNamespace(
                offlinePage.getClientId().getNamespace());
        // Ensure that we have a file path that is longer than just "/".
        if (!isPageUserRequested && offlinePath.length() > 1) {
            File file = new File(offlinePath);
            // We might get an exception if chrome does not have sharing roots configured.  If so,
            // just share by URL of the original page instead of sharing the offline page.
            try {
                uri = (new FileProviderHelper()).getContentUriFromFile(file);
            } catch (Exception e) {
                uri = Uri.parse(pageUrl);
            }
        } else {
            uri = Uri.parse(pageUrl);
        }

        if (!isOfflinePageShareable(offlinePageBridge, offlinePage, uri)) return false;

        if (!isPageUserRequested || !offlinePageBridge.isInPrivateDirectory(offlinePath)) {
            // Share pages temporary pages and pages already in a public location.
            final String pageTitle = tab.getTitle();
            final File offlinePageFile = new File(offlinePath);
            sharePage(activity, uri.toString(), pageTitle, offlinePath, offlinePageFile,
                    shareCallback);
            return true;
        }

        // The file access permission is needed since we may need to publish the archive file
        // if it resides in internal directory.
        offlinePageBridge.acquireFileAccessPermission(webContents, (granted) -> {
            if (!granted) {
                recordPublishPageResult(SavePageResult.PERMISSION_DENIED);
                return;
            }

            // If a user requested page is not in a public location, we must publish it before
            // sharing it.
            publishThenShareInternalPage(activity, offlinePageBridge, offlinePage, shareCallback);
        });

        return true;
    }

    /**
     * Check to see if the offline page is sharable.
     * @param offlinePageBridge Bridge to native code for offline pages use.
     * @param offlinePage Page to check for sharability.
     * @param pageUri Uri of the page to check.
     * @return true if this page can be shared.
     */
    public static boolean isOfflinePageShareable(
            OfflinePageBridge offlinePageBridge, OfflinePageItem offlinePage, Uri uri) {
        // Return false if there is no offline page.
        if (offlinePage == null) return false;

        String offlinePath = offlinePage.getFilePath();

        // If we have a content or file Uri, then we can share the page.
        if (isSchemeContentOrFile(uri)) {
            return true;
        }

        // If the scheme is not one we recognize, return false.
        if (!TextUtils.equals(uri.getScheme(), UrlConstants.HTTP_SCHEME)
                && !TextUtils.equals(uri.getScheme(), UrlConstants.HTTPS_SCHEME))
            return false;

        // If we have a http or https page with no file path, we cannot share it.
        if (offlinePath.isEmpty()) {
            Log.w(TAG, "Tried to share a page with no path.");
            return false;
        }

        return true;
    }

    // Returns true if the scheme of the URI is either content or file.
    private static boolean isSchemeContentOrFile(Uri uri) {
        boolean isContentScheme = TextUtils.equals(uri.getScheme(), UrlConstants.CONTENT_SCHEME);
        boolean isFileScheme = TextUtils.equals(uri.getScheme(), UrlConstants.FILE_SCHEME);

        return isContentScheme || isFileScheme;
    }


    /**
     * For internal pages, we must publish them, then share them.
     * @param offlinePageBridge Bridge to native code for offline pages use.
     * @param offlinePage Page to publish and share.
     * @param shareCallback The callback to be used to send the ShareParams.
     */
    public static void publishThenShareInternalPage(final Activity activity,
            OfflinePageBridge offlinePageBridge, OfflinePageItem offlinePage,
            final Callback<ShareParams> shareCallback) {
        PublishPageCallback publishPageCallback =
                new PublishPageCallback(activity, offlinePage, shareCallback);
        offlinePageBridge.publishInternalPageByOfflineId(
                offlinePage.getOfflineId(), publishPageCallback);
    }

    /**
     * Called when publishing is done.  Continues with processing to share.
     */
    public static void publishCompleted(OfflinePageItem page, final Activity activity,
            final Callback<ShareParams> shareCallback) {
        sharePublishedPage(page, activity, shareCallback);
    }

    /**
     * This will take a page in a public directory, and share it.
     */
    public static void sharePublishedPage(OfflinePageItem page, final Activity activity,
            final Callback<ShareParams> shareCallback) {
        if (page == null) {
            // For errors, we don't call the shareCallback.  The callback only causes the page to be
            // shared, and does not report errors, and is not needed to continue processing.
            return;
        }
        final String pageUrl = page.getUrl();
        final String pageTitle = page.getTitle();
        final File offlinePageFile = new File(page.getFilePath());
        sharePage(activity, pageUrl, pageTitle, page.getFilePath(), offlinePageFile, shareCallback);
    }

    /**
     * Share the page.
     */
    public static void sharePage(Activity activity, String pageUrl, String pageTitle,
            String offlinePath, File offlinePageFile, final Callback<ShareParams> shareCallback) {
        RecordUserAction.record("OfflinePages.Sharing.SharePageFromOverflowMenu");
        AsyncTask<Uri> task = new AsyncTask<Uri>() {
            @Override
            protected Uri doInBackground() {
                // If we have a content or file URI, we will not have a filename, just return the
                // URI.
                if (offlinePath.isEmpty()) {
                    Uri uri = Uri.parse(pageUrl);
                    assert(isSchemeContentOrFile(uri));
                    return uri;
                }
                return ChromeFileProvider.generateUri(activity, offlinePageFile);
            }
            @Override
            protected void onPostExecute(Uri uri) {
                ShareParams shareParams = new ShareParams.Builder(activity, pageTitle, pageUrl)
                                                  .setShareDirectly(false)
                                                  .setOfflineUri(uri)
                                                  .build();
                shareCallback.onResult(shareParams);
            }
        };
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Retrieves the extra request header to reload the offline page.
     * @param tab The current tab.
     * @return The extra request header string.
     */
    public static String getOfflinePageHeaderForReload(Tab tab) {
        OfflinePageBridge offlinePageBridge = getInstance().getOfflinePageBridge(tab.getProfile());
        if (offlinePageBridge == null) return "";
        return offlinePageBridge.getOfflinePageHeaderForReload(tab.getWebContents());
    }

    /**
     * A load url parameters to open offline version of the offline page. If the offline page is
     * trusted, the URL (http/https) of the offline page is to be opened. Otherwise, the file URL
     * pointing to the archive file is to be opened. In both cases, a custom header is passed with
     * the URL to ensure loading a specific version of offline page.
     * @param url       The url of the offline page to open.
     * @param offlineId The ID of the offline page to open.
     * @param location  Indicates where the offline page is launched.
     * @param callback  The callback to pass back the LoadUrlParams for launching an URL.
     */
    public static void getLoadUrlParamsForOpeningOfflineVersion(final String url, long offlineId,
            final @LaunchLocation int location, Callback<LoadUrlParams> callback) {
        OfflinePageBridge offlinePageBridge =
                getInstance().getOfflinePageBridge(Profile.getLastUsedProfile());
        if (offlinePageBridge == null) {
            callback.onResult(null);
            return;
        }

        offlinePageBridge.getLoadUrlParamsByOfflineId(
                offlineId, location, (loadUrlParams) -> { callback.onResult(loadUrlParams); });
    }

    /**
     * A load url parameters to handle the intent for viewing MHTML file or content. If the
     * trusted offline page is found, the URL (http/https) of the offline page is to be opened.
     * Otherwise, the file or content URL from the intent will be launched.
     * @param intentUrl URL from the intent.
     * @param callback  The callback to pass back the launching URL and extra headers.
     */
    public static void getLoadUrlParamsForOpeningMhtmlFileOrContent(
            final String intentUrl, Callback<LoadUrlParams> callback) {
        OfflinePageBridge offlinePageBridge =
                getInstance().getOfflinePageBridge(Profile.getLastUsedProfile());
        if (offlinePageBridge == null) {
            callback.onResult(new LoadUrlParams(intentUrl));
            return;
        }

        offlinePageBridge.getLoadUrlParamsForOpeningMhtmlFileOrContent(intentUrl, callback);
    }

    /**
     * @return True if an offline preview is being shown.
     * @param tab The current tab.
     */
    public static boolean isShowingOfflinePreview(Tab tab) {
        return getInstance().isShowingOfflinePreview(tab);
    }

    /**
     * Checks if an offline page is shown for the tab.
     * @param tab The tab to be reloaded.
     * @return True if the offline page is opened.
     */
    public static boolean isOfflinePage(Tab tab) {
        return getInstance().isOfflinePage(tab);
    }

    /**
     * Retrieves the offline page that is shown for the tab.
     * @param tab The tab to be reloaded.
     * @return The offline page if tab currently displays it, null otherwise.
     */
    public static OfflinePageItem getOfflinePage(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;
        OfflinePageBridge offlinePageBridge = getInstance().getOfflinePageBridge(tab.getProfile());
        if (offlinePageBridge == null) return null;
        return offlinePageBridge.getOfflinePage(webContents);
    }

    /**
     * Returns whether the tab is showing a trusted offline page.
     * @param tab The current tab.
     * @return True if the tab is showing a trusted offline page.
     */
    public static boolean isShowingTrustedOfflinePage(Tab tab) {
        return getInstance().isShowingTrustedOfflinePage(tab);
    }

    /**
     * Reloads specified tab, which should allow to open an online version of the page.
     * @param tab The tab to be reloaded.
     */
    public static void reload(Tab tab) {
        OfflinePageItem offlinePage = getOfflinePage(tab);
        if (isShowingTrustedOfflinePage(tab) || offlinePage == null) {
            // If current page is an offline page, reload it with custom behavior defined in extra
            // header respected.
            LoadUrlParams params = new LoadUrlParams(tab.getOriginalUrl(), PageTransition.RELOAD);
            params.setVerbatimHeaders(getOfflinePageHeaderForReload(tab));
            tab.loadUrl(params);
            return;
        }

        LoadUrlParams params = new LoadUrlParams(offlinePage.getUrl(), PageTransition.RELOAD);
        tab.loadUrl(params);
    }

    /**
     * Tracks tab creation and closure for the Recent Tabs feature.  UI needs to stop showing
     * recent offline pages as soon as the tab is closed.  The TabModel is used to get profile
     * information because Tab's profile is tied to the native WebContents, which may not exist at
     * tab adding or tab closing time.
     */
    private static class RecentTabTracker extends TabModelSelectorTabModelObserver {
        /**
         * The single, stateless TabRestoreTracker instance to monitor all tab restores.
         */
        private static final TabRestoreTracker sTabRestoreTracker = new TabRestoreTracker();

        private TabModelSelector mTabModelSelector;

        public RecentTabTracker(TabModelSelector selector) {
            super(selector);
            mTabModelSelector = selector;
        }

        @Override
        public void didAddTab(Tab tab, @TabModel.TabLaunchType int type) {
            tab.addObserver(sTabRestoreTracker);
        }

        @Override
        public void willCloseTab(Tab tab, boolean animate) {
            Profile profile = mTabModelSelector.getModel(tab.isIncognito()).getProfile();
            OfflinePageBridge bridge = OfflinePageBridge.getForProfile(profile);
            if (bridge == null) return;

            WebContents webContents = tab.getWebContents();
            if (webContents != null) bridge.willCloseTab(webContents);
        }

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            Profile profile = mTabModelSelector.getModel(incognito).getProfile();
            OfflinePageBridge bridge = OfflinePageBridge.getForProfile(profile);
            if (bridge == null) return;

            // Delete any "Last N" offline pages as well. This is an optimization because
            // the UI will no longer show the page, and the page would also be cleaned up by GC
            // given enough time.
            ClientId clientId =
                    new ClientId(OfflinePageBridge.LAST_N_NAMESPACE, Integer.toString(tabId));
            List<ClientId> clientIds = new ArrayList<>();
            clientIds.add(clientId);

            bridge.deletePagesByClientId(clientIds, new Callback<Integer>() {
                @Override
                public void onResult(Integer result) {
                    // Result is ignored.
                }
            });
        }
    }

    /**
     * Starts tracking the tab models in the given selector for tab addition and closure,
     * destroying obsolete observers as necessary.
     */
    public static void observeTabModelSelector(
            ChromeActivity activity, TabModelSelector tabModelSelector) {
        RecentTabTracker previousObserver =
                sTabModelObservers.put(activity, new RecentTabTracker(tabModelSelector));
        if (previousObserver != null) {
            previousObserver.destroy();
        } else {
            // This is the 1st time we see this activity so register a state listener with it.
            ApplicationStatus.registerStateListenerForActivity(
                    new ApplicationStatus.ActivityStateListener() {
                        @Override
                        public void onActivityStateChange(Activity activity, int newState) {
                            if (newState == ActivityState.DESTROYED) {
                                sTabModelObservers.remove(activity).destroy();
                                ApplicationStatus.unregisterActivityStateListener(this);
                            }
                        }
                    },
                    activity);
        }
    }

    private static class TabRestoreTracker extends EmptyTabObserver {
        /**
         * If the tab was being restored, reports that it successfully finished reloading its
         * contents.
         */
        @Override
        public void onPageLoadFinished(Tab tab) {
            if (!tab.isBeingRestored()) return;

            // We first compute the bitwise tab restore context.
            int tabRestoreContext = 0;
            if (isConnected()) tabRestoreContext |= BIT_ONLINE;
            OfflinePageItem page = getOfflinePage(tab);
            if (page != null) {
                tabRestoreContext |= BIT_OFFLINE_PAGE;
                if (page.getClientId().getNamespace().equals(OfflinePageBridge.LAST_N_NAMESPACE)) {
                    tabRestoreContext |= BIT_LAST_N;
                }
            } else if (!OfflinePageBridge.canSavePage(tab.getUrl()) || tab.isIncognito()) {
                tabRestoreContext |= BIT_CANT_SAVE_OFFLINE;
            }

            // Now determine the correct tab restore type based on the context.
            int tabRestoreType;
            switch (tabRestoreContext) {
                case BIT_ONLINE:
                    tabRestoreType = TabRestoreType.WHILE_ONLINE;
                    break;
                case BIT_ONLINE | BIT_CANT_SAVE_OFFLINE:
                    tabRestoreType = TabRestoreType.WHILE_ONLINE_CANT_SAVE_FOR_OFFLINE_USAGE;
                    break;
                case BIT_ONLINE | BIT_OFFLINE_PAGE:
                    tabRestoreType = TabRestoreType.WHILE_ONLINE_TO_OFFLINE_PAGE;
                    break;
                case BIT_ONLINE | BIT_OFFLINE_PAGE | BIT_LAST_N:
                    tabRestoreType = TabRestoreType.WHILE_ONLINE_TO_OFFLINE_PAGE_FROM_LAST_N;
                    break;
                case 0: // offline (not BIT_ONLINE present).
                    tabRestoreType = TabRestoreType.WHILE_OFFLINE;
                    break;
                case BIT_CANT_SAVE_OFFLINE:
                    tabRestoreType = TabRestoreType.WHILE_OFFLINE_CANT_SAVE_FOR_OFFLINE_USAGE;
                    break;
                case BIT_OFFLINE_PAGE:
                    tabRestoreType = TabRestoreType.WHILE_OFFLINE_TO_OFFLINE_PAGE;
                    break;
                case BIT_OFFLINE_PAGE | BIT_LAST_N:
                    tabRestoreType = TabRestoreType.WHILE_OFFLINE_TO_OFFLINE_PAGE_FROM_LAST_N;
                    break;
                default:
                    assert false;
                    return;
            }
            recordTabRestoreHistogram(tabRestoreType, tab.getUrl());
        }

        /**
         * If the tab was being restored, reports that it failed reloading its contents.
         */
        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            if (tab.isBeingRestored()) recordTabRestoreHistogram(TabRestoreType.FAILED, null);
        }

        /**
         * If the tab was being restored, reports that it crashed while doing so.
         */
        @Override
        public void onCrash(Tab tab) {
            if (tab.isBeingRestored()) recordTabRestoreHistogram(TabRestoreType.CRASHED, null);
        }
    }

    private static void recordTabRestoreHistogram(int tabRestoreType, String url) {
        Log.d(TAG, "Concluded tab restore: type=" + tabRestoreType + ", url=" + url);
        RecordHistogram.recordEnumeratedHistogram(
                "OfflinePages.TabRestore", tabRestoreType, TabRestoreType.NUM_ENTRIES);
    }

    @VisibleForTesting
    static void setInstanceForTesting(Internal instance) {
        sInstance = instance;
    }

    @VisibleForTesting
    public static void setSnackbarDurationForTesting(int durationMs) {
        sSnackbarDurationMs = durationMs;
    }
}
