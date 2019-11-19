// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offlinepages.DeletePageResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Access gate to C++ side offline pages functionalities.
 */
@JNINamespace("offline_pages::android")
public class OfflinePageBridge {
    // These constants must be kept in sync with the constants defined in
    // //components/offline_pages/core/client_namespace_constants.cc
    public static final String ASYNC_NAMESPACE = "async_loading";
    public static final String BOOKMARK_NAMESPACE = "bookmark";
    public static final String LAST_N_NAMESPACE = "last_n";
    public static final String SHARE_NAMESPACE = "share";
    public static final String CCT_NAMESPACE = "custom_tabs";
    public static final String DOWNLOAD_NAMESPACE = "download";
    public static final String NTP_SUGGESTIONS_NAMESPACE = "ntp_suggestions";
    public static final String SUGGESTED_ARTICLES_NAMESPACE = "suggested_articles";
    public static final String BROWSER_ACTIONS_NAMESPACE = "browser_actions";
    public static final String LIVE_PAGE_SHARING_NAMESPACE = "live_page_sharing";

    private long mNativeOfflinePageBridge;
    private boolean mIsNativeOfflinePageModelLoaded;
    private final ObserverList<OfflinePageModelObserver> mObservers = new ObserverList<>();

    /**
     * Retrieves the OfflinePageBridge for the given profile, creating it the first time
     * getForProfile or getForProfileKey is called for the profile.  Must be called on the UI
     * thread.
     *
     * @param profile The profile associated with the OfflinePageBridge to get.
     */
    public static OfflinePageBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (profile == null) {
            return null;
        }

        return getForProfileKey(profile.getProfileKey());
    }

    /**
     * Retrieves the OfflinePageBridge for the profile with the given key, creating it the first
     * time getForProfile or getForProfileKey is called for the profile.  Must be called on the UI
     * thread.
     *
     * @param profileKey Key of the profile associated with the OfflinePageBridge to get.
     */
    public static OfflinePageBridge getForProfileKey(ProfileKey profileKey) {
        ThreadUtils.assertOnUiThread();

        return OfflinePageBridgeJni.get().getOfflinePageBridgeForProfileKey(profileKey);
    }

    /**
     * Callback used when saving an offline page.
     */
    public interface SavePageCallback {
        /**
         * Delivers result of saving a page.
         *
         * @param savePageResult Result of the saving. Uses {@see
         *         org.chromium.components.offlinepages.SavePageResult} enum.
         * @param url URL of the saved page.
         * @see OfflinePageBridge#savePage(WebContents, ClientId, OfflinePageOrigin,
         *         SavePageCallback)
         */
        @CalledByNative("SavePageCallback")
        void onSavePageDone(int savePageResult, String url, long offlineId);
    }

    /**
     * Base observer class listeners to be notified of changes to the offline page model.
     */
    public abstract static class OfflinePageModelObserver {
        /**
         * Called when the native side of offline pages is loaded and now in usable state.
         */
        public void offlinePageModelLoaded() {}

        /**
         * Called when the native side of offline pages is changed due to adding, removing or
         * update an offline page.
         */
        public void offlinePageAdded(OfflinePageItem addedPage) {}

        /**
         * Called when an offline page is deleted. This can be called as a result of
         * #checkOfflinePageMetadata().
         * @param deletedPage Info about the deleted offline page.
         */
        public void offlinePageDeleted(DeletedPageInfo deletedPage) {}
    }

    /**
     * Creates an offline page bridge for a given profile.
     */
    @VisibleForTesting
    protected OfflinePageBridge(long nativeOfflinePageBridge) {
        mNativeOfflinePageBridge = nativeOfflinePageBridge;
    }

    /**
     * Called by the native OfflinePageBridge so that it can cache the new Java OfflinePageBridge.
     */
    @CalledByNative
    private static OfflinePageBridge create(long nativeOfflinePageBridge) {
        return new OfflinePageBridge(nativeOfflinePageBridge);
    }

    /**
     * @return True if an offline copy of the given URL can be saved.
     */
    public static boolean canSavePage(String url) {
        return OfflinePageBridgeJni.get().canSavePage(url);
    }

    /**
     * @return the string representing the origin of the tab.
     */
    @CalledByNative
    private static String getEncodedOriginApp(Tab tab) {
        return new OfflinePageOrigin(ContextUtils.getApplicationContext(), tab)
                .encodeAsJsonString();
    }

    /**
     * Adds an observer to offline page model changes.
     * @param observer The observer to be added.
     */
    public void addObserver(OfflinePageModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to offline page model changes.
     * @param observer The observer to be removed.
     */
    public void removeObserver(OfflinePageModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Gets all available offline pages, returning results via the provided callback.
     *
     * @param callback The callback to run when the operation completes.
     */
    @VisibleForTesting
    public void getAllPages(final Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        OfflinePageBridgeJni.get().getAllPages(
                mNativeOfflinePageBridge, OfflinePageBridge.this, result, callback);
    }

    /**
     * Gets the offline pages associated with the provided client IDs.
     *
     * @param clientIds Client's IDs associated with offline pages.
     * @param callback The callback to run when the response is ready. It will be passed a list of
     *         {@link OfflinePageItem} matching the provided IDs, or an empty list if none exist.
     */
    @VisibleForTesting
    public void getPagesByClientIds(
            final List<ClientId> clientIds, final Callback<List<OfflinePageItem>> callback) {
        String[] namespaces = new String[clientIds.size()];
        String[] ids = new String[clientIds.size()];

        for (int i = 0; i < clientIds.size(); i++) {
            namespaces[i] = clientIds.get(i).getNamespace();
            ids[i] = clientIds.get(i).getId();
        }

        List<OfflinePageItem> result = new ArrayList<>();
        OfflinePageBridgeJni.get().getPagesByClientId(mNativeOfflinePageBridge,
                OfflinePageBridge.this, result, namespaces, ids, callback);
    }

    /**
     * Gets the offline pages associated with the provided origin.
     * @param origin The JSON-like string of the app's package name and encrypted signature hash.
     * @param callback The callback to run when the response is ready. It will be passed a list of
     *         {@link OfflinePageItem} matching the provided origin, or an empty list if none exist.
     */
    public void getPagesByRequestOrigin(String origin, Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        OfflinePageBridgeJni.get().getPagesByRequestOrigin(
                mNativeOfflinePageBridge, OfflinePageBridge.this, result, origin, callback);
    }

    /**
     * Gets the offline pages associated with the provided namespace.
     *
     * @param namespace The string form of the namespace to query.
     * @param callback The callback to run when the response is ready. It will be passed a list of
     *         {@link OfflinePageItem} matching the provided namespace, or an empty list if none
     * exist.
     */
    public void getPagesByNamespace(
            final String namespace, final Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        OfflinePageBridgeJni.get().getPagesByNamespace(
                mNativeOfflinePageBridge, OfflinePageBridge.this, result, namespace, callback);
    }

    /**
     * Get the offline page associated with the provided offline URL.
     *
     * @param onlineUrl URL of the page.
     * @param tabId Android tab ID.
     * @param callback callback to pass back the matching {@link OfflinePageItem} if found. Will
     *         pass back null if not.
     */
    public void selectPageForOnlineUrl(String onlineUrl, int tabId,
            Callback<OfflinePageItem> callback) {
        OfflinePageBridgeJni.get().selectPageForOnlineUrl(
                mNativeOfflinePageBridge, OfflinePageBridge.this, onlineUrl, tabId, callback);
    }

    /**
     * Get the offline page associated with the provided offline ID.
     *
     * @param offlineId ID of the offline page.
     * @param callback callback to pass back the matching {@link OfflinePageItem} if found. Will
     *         pass back <code>null</code> if not.
     */
    public void getPageByOfflineId(final long offlineId, final Callback<OfflinePageItem> callback) {
        OfflinePageBridgeJni.get().getPageByOfflineId(
                mNativeOfflinePageBridge, OfflinePageBridge.this, offlineId, callback);
    }

    /**
     * Saves the web page loaded into web contents offline.
     * Retrieves the origin of the page from the WebContents.
     *
     * @param webContents Contents of the page to save.
     * @param clientId Client ID of the bookmark related to the offline page.
     * @param callback Interface that contains a callback. This may be called synchronously, e.g. if
     *         the web contents is already destroyed.
     * @see SavePageCallback
     */
    public void savePage(final WebContents webContents, final ClientId clientId,
            final SavePageCallback callback) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        OfflinePageOrigin origin;
        if (activity != null && activity.getActivityTab() != null) {
            origin = new OfflinePageOrigin(
                    ContextUtils.getApplicationContext(), activity.getActivityTab());
        } else {
            origin = new OfflinePageOrigin();
        }
        savePage(webContents, clientId, origin, callback);
    }

    /**
     * Saves the web page loaded into web contents offline.
     *
     * @param webContents Contents of the page to save.
     * @param clientId Client ID of the bookmark related to the offline page.
     * @param origin The app that initiated the download.
     * @param callback Interface that contains a callback. This may be called synchronously, e.g. if
     *         the web contents is already destroyed.
     * @see SavePageCallback
     */
    public void savePage(final WebContents webContents, final ClientId clientId,
            final OfflinePageOrigin origin, final SavePageCallback callback) {
        assert mIsNativeOfflinePageModelLoaded;
        assert webContents != null;
        assert origin != null;

        OfflinePageBridgeJni.get().savePage(mNativeOfflinePageBridge, OfflinePageBridge.this,
                callback, webContents, clientId.getNamespace(), clientId.getId(),
                origin.encodeAsJsonString());
    }

    /**
     * Deletes an offline page related to a specified bookmark.
     *
     * @param clientId Client ID for which the offline copy will be deleted.
     * @param callback Interface that contains a callback.
     */
    @VisibleForTesting
    public void deletePage(final ClientId clientId, Callback<Integer> callback) {
        assert mIsNativeOfflinePageModelLoaded;
        ArrayList<ClientId> ids = new ArrayList<ClientId>();
        ids.add(clientId);

        deletePagesByClientId(ids, callback);
    }

    /**
     * Deletes offline pages based on the list of provided client IDs. Calls the callback
     * when operation is complete. Requires that the model is already loaded.
     *
     * @param clientIds A list of Client IDs for which the offline pages will be deleted.
     * @param callback A callback that will be called once operation is completed.
     */
    public void deletePagesByClientId(List<ClientId> clientIds, Callback<Integer> callback) {
        String[] namespaces = new String[clientIds.size()];
        String[] ids = new String[clientIds.size()];

        for (int i = 0; i < clientIds.size(); i++) {
            namespaces[i] = clientIds.get(i).getNamespace();
            ids[i] = clientIds.get(i).getId();
        }

        OfflinePageBridgeJni.get().deletePagesByClientId(
                mNativeOfflinePageBridge, OfflinePageBridge.this, namespaces, ids, callback);
    }

    /**
     * Deletes offline pages based on the list of provided client IDs only if they originate
     * from the same origin. Calls the callback when operation is complete. Requires that the
     * model is already loaded.
     *
     * @param clientIds A list of Client IDs for which the offline pages will be deleted.
     * @param callback A callback that will be called once operation is completed.
     */
    public void deletePagesByClientIdAndOrigin(
            List<ClientId> clientIds, OfflinePageOrigin origin, Callback<Integer> callback) {
        String[] namespaces = new String[clientIds.size()];
        String[] ids = new String[clientIds.size()];

        for (int i = 0; i < clientIds.size(); i++) {
            namespaces[i] = clientIds.get(i).getNamespace();
            ids[i] = clientIds.get(i).getId();
        }

        OfflinePageBridgeJni.get().deletePagesByClientIdAndOrigin(mNativeOfflinePageBridge,
                OfflinePageBridge.this, namespaces, ids, origin.encodeAsJsonString(), callback);
    }

    /**
     * Deletes offline pages based on the list of offline IDs. Calls the callback
     * when operation is complete. Note that offline IDs are not intended to be saved across
     * restarts of Chrome; they should be obtained by querying the model for the appropriate client
     * ID.
     *
     * @param offlineIdList A list of offline IDs of pages that will be deleted.
     * @param callback A callback that will be called once operation is completed, called with the
     *         DeletePageResult of the operation..
     */
    public void deletePagesByOfflineId(List<Long> offlineIdList, Callback<Integer> callback) {
        if (offlineIdList == null) {
            callback.onResult(Integer.valueOf(DeletePageResult.SUCCESS));
            return;
        }

        long[] offlineIds = new long[offlineIdList.size()];
        for (int i = 0; i < offlineIdList.size(); i++) {
            offlineIds[i] = offlineIdList.get(i).longValue();
        }
        OfflinePageBridgeJni.get().deletePagesByOfflineId(
                mNativeOfflinePageBridge, OfflinePageBridge.this, offlineIds, callback);
    }

    /**
     * Ask the native code to publish the internal page asychronously.
     * @param offlineId ID of the offline page to publish.
     * @param publishedCallback Function to call when publishing is done.  This will be called with
     *         the new path of the file.
     */
    public void publishInternalPageByOfflineId(long offlineId, Callback<String> publishedCallback) {
        OfflinePageBridgeJni.get().publishInternalPageByOfflineId(
                mNativeOfflinePageBridge, OfflinePageBridge.this, offlineId, publishedCallback);
    }

    /**
     * Ask the native code to publish the internal page asychronously.
     * @param guid Client ID of the offline page to publish.
     * @param publishedCallback Function to call when publishing is done.
     */
    public void publishInternalPageByGuid(String guid, Callback<String> publishedCallback) {
        OfflinePageBridgeJni.get().publishInternalPageByGuid(
                mNativeOfflinePageBridge, OfflinePageBridge.this, guid, publishedCallback);
    }

    /**
     * Whether or not the underlying offline page model is loaded.
     */
    public boolean isOfflinePageModelLoaded() {
        return mIsNativeOfflinePageModelLoaded;
    }

    /**
     * Retrieves the extra request header to reload the offline page.
     * @param webContents Contents of the page to reload.
     * @return The extra request header string.
     */
    public String getOfflinePageHeaderForReload(WebContents webContents) {
        return OfflinePageBridgeJni.get().getOfflinePageHeaderForReload(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /**
     * @param webContents Contents of the page to check.
     * @return True if an offline preview is being shown.
     */
    public boolean isShowingOfflinePreview(WebContents webContents) {
        return OfflinePageBridgeJni.get().isShowingOfflinePreview(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /**
     * @param webContents Contents of the page to check.
     * @return True if download button is being shown in the error page.
     */
    public boolean isShowingDownloadButtonInErrorPage(WebContents webContents) {
        return OfflinePageBridgeJni.get().isShowingDownloadButtonInErrorPage(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /** Tells the native side that the tab of |webContents| will be closed. */
    void willCloseTab(WebContents webContents) {
        OfflinePageBridgeJni.get().willCloseTab(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /**
     * Schedules to download a page from |url| and categorize under |nameSpace|.
     * The duplicate pages or requests will be checked.
     * Origin is presumed to be Chrome.
     *
     * @param webContents Web contents upon which the infobar is shown.
     * @param nameSpace Namespace of the page to save.
     * @param url URL of the page to save.
     * @param uiAction UI action, like showing infobar or toast on certain case.
     */
    public void scheduleDownload(
            WebContents webContents, String nameSpace, String url, int uiAction) {
        scheduleDownload(webContents, nameSpace, url, uiAction, new OfflinePageOrigin());
    }

    /**
     * Schedules to download a page from |url| and categorize under |namespace| from |origin|.
     * The duplicate pages or requests will be checked.
     *
     * @param webContents Web contents upon which the infobar is shown.
     * @param nameSpace Namespace of the page to save.
     * @param url URL of the page to save.
     * @param uiAction UI action, like showing infobar or toast on certain case.
     * @param origin Origin of the page.
     */
    public void scheduleDownload(WebContents webContents, String nameSpace, String url,
            int uiAction, OfflinePageOrigin origin) {
        OfflinePageBridgeJni.get().scheduleDownload(mNativeOfflinePageBridge,
                OfflinePageBridge.this, webContents, nameSpace, url, uiAction,
                origin.encodeAsJsonString());
    }

    /**
     * Checks if an offline page is shown for the webContents.
     * @param webContents Web contents used to find the offline page.
     * @return True if the offline page is opened.
     */
    public boolean isOfflinePage(WebContents webContents) {
        return OfflinePageBridgeJni.get().isOfflinePage(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /**
     * Returns whether |nameSpace| is a temporary namespace.
     * @param nameSpace Namespace of the page in question.
     * @return true if the page is in a temporary namespace.
     */
    public boolean isTemporaryNamespace(String nameSpace) {
        return OfflinePageBridgeJni.get().isTemporaryNamespace(
                mNativeOfflinePageBridge, OfflinePageBridge.this, nameSpace);
    }

    /**
     * Checks if the supplied file path is in a private dir internal to chrome.
     * @param filePath Path of the file to check.
     * @return True if the file is in a private directory.
     */
    public boolean isInPrivateDirectory(String filePath) {
        return OfflinePageBridgeJni.get().isInPrivateDirectory(
                mNativeOfflinePageBridge, OfflinePageBridge.this, filePath);
    }

    /**
     * Retrieves the offline page that is shown for the tab.
     * @param webContents Web contents used to find the offline page.
     * @return The offline page if tab currently displays it, null otherwise.
     */
    @Nullable
    public OfflinePageItem getOfflinePage(WebContents webContents) {
        return OfflinePageBridgeJni.get().getOfflinePage(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /**
     * Queries the model for offline content that's been added since the given timestamp.
     * @param freshnessTimeMillis Returned content must be newer than |timestamp|, a date
     *         represented as the number of millis since the Java epoch.
     * @param callback Fired when the model check has been finished, with a String parameter that
     *         represents the source of the offline content.  The parameter will be the empty string
     *         if no fresh enough content is found.
     */
    public void checkForNewOfflineContent(long freshnessTimeMillis, Callback<String> callback) {
        OfflinePageBridgeJni.get().checkForNewOfflineContent(
                mNativeOfflinePageBridge, OfflinePageBridge.this, freshnessTimeMillis, callback);
    }

    /**
     * Get the the url params to open the offline page associated with the provided offline ID.
     * Depending on whether it is trusted or not, either http/https or file URL will be returned in
     * the callback.
     *
     * @param offlineId ID of the offline page.
     * @param location Where the offline page is launched.
     * @param callback callback to pass back the url string if found. Will pass back
     *         <code>null</code> if not.
     */
    public void getLoadUrlParamsByOfflineId(
            long offlineId, @LaunchLocation int location, Callback<LoadUrlParams> callback) {
        OfflinePageBridgeJni.get().getLoadUrlParamsByOfflineId(
                mNativeOfflinePageBridge, OfflinePageBridge.this, offlineId, location, callback);
    }

    /**
     * Get the url params to open the intent carrying MHTML file or content.
     *
     * @param url The file:// or content:// URL.
     * @param callback Callback to pass back the url params.
     */
    public void getLoadUrlParamsForOpeningMhtmlFileOrContent(
            String url, Callback<LoadUrlParams> callback) {
        OfflinePageBridgeJni.get().getLoadUrlParamsForOpeningMhtmlFileOrContent(
                mNativeOfflinePageBridge, OfflinePageBridge.this, url, callback);
    }

    /**
     * Checks if the web contents is showing a trusted offline page.
     * @param webContents Web contents shown.
     * @return True if a trusted offline page is shown.
     */
    public boolean isShowingTrustedOfflinePage(WebContents webContents) {
        return OfflinePageBridgeJni.get().isShowingTrustedOfflinePage(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents);
    }

    /**
     * Tries to acquire the storage access permssion if not yet.
     *
     * @param webContents Contents of the page to check.
     * @param callback Callback to notify the result.
     */
    public void acquireFileAccessPermission(WebContents webContents, Callback<Boolean> callback) {
        OfflinePageBridgeJni.get().acquireFileAccessPermission(
                mNativeOfflinePageBridge, OfflinePageBridge.this, webContents, callback);
    }

    @CalledByNative
    protected void offlinePageModelLoaded() {
        mIsNativeOfflinePageModelLoaded = true;
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageModelLoaded();
        }
    }

    @CalledByNative
    protected void offlinePageAdded(OfflinePageItem addedPage) {
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageAdded(addedPage);
        }
    }

    /**
     * Removes references to the native OfflinePageBridge when it is being destroyed.
     */
    @CalledByNative
    protected void offlinePageBridgeDestroyed() {
        ThreadUtils.assertOnUiThread();
        assert mNativeOfflinePageBridge != 0;

        mIsNativeOfflinePageModelLoaded = false;
        mNativeOfflinePageBridge = 0;

        // TODO(dewittj): Add a model destroyed method to the observer interface.
        mObservers.clear();
    }

    @CalledByNative
    void offlinePageDeleted(DeletedPageInfo deletedPage) {
        for (OfflinePageModelObserver observer : mObservers) {
            observer.offlinePageDeleted(deletedPage);
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(List<OfflinePageItem> offlinePagesList,
            String url, long offlineId, String clientNamespace, String clientId, String title,
            String filePath, long fileSize, long creationTime, int accessCount,
            long lastAccessTimeMs, String requestOrigin) {
        offlinePagesList.add(createOfflinePageItem(url, offlineId, clientNamespace, clientId, title,
                filePath, fileSize, creationTime, accessCount, lastAccessTimeMs, requestOrigin));
    }

    @CalledByNative
    private static OfflinePageItem createOfflinePageItem(String url, long offlineId,
            String clientNamespace, String clientId, String title, String filePath, long fileSize,
            long creationTime, int accessCount, long lastAccessTimeMs, String requestOrigin) {
        return new OfflinePageItem(url, offlineId, clientNamespace, clientId, title, filePath,
                fileSize, creationTime, accessCount, lastAccessTimeMs, requestOrigin);
    }

    @CalledByNative
    private static ClientId createClientId(String clientNamespace, String id) {
        return new ClientId(clientNamespace, id);
    }

    @CalledByNative
    private static DeletedPageInfo createDeletedPageInfo(
            long offlineId, String clientNamespace, String clientId, String requestOrigin) {
        return new DeletedPageInfo(offlineId, clientNamespace, clientId, requestOrigin);
    }

    @CalledByNative
    private static LoadUrlParams createLoadUrlParams(
            String url, String extraHeaderKey, String extraHeaderValue) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        if (!TextUtils.isEmpty(extraHeaderKey) && !TextUtils.isEmpty(extraHeaderValue)) {
            // Set both map-based and collapsed headers to support all use scenarios.
            Map<String, String> headers = new HashMap<String, String>();
            headers.put(extraHeaderKey, extraHeaderValue);
            loadUrlParams.setExtraHeaders(headers);
            loadUrlParams.setVerbatimHeaders(extraHeaderKey + ": " + extraHeaderValue);
        }
        return loadUrlParams;
    }

    @NativeMethods
    interface Natives {
        boolean canSavePage(String url);
        OfflinePageBridge getOfflinePageBridgeForProfileKey(ProfileKey profileKey);
        void getAllPages(long nativeOfflinePageBridge, OfflinePageBridge caller,
                List<OfflinePageItem> offlinePages, final Callback<List<OfflinePageItem>> callback);
        void willCloseTab(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        void getPageByOfflineId(long nativeOfflinePageBridge, OfflinePageBridge caller,
                long offlineId, Callback<OfflinePageItem> callback);
        void getPagesByClientId(long nativeOfflinePageBridge, OfflinePageBridge caller,
                List<OfflinePageItem> result, String[] namespaces, String[] ids,
                Callback<List<OfflinePageItem>> callback);
        void getPagesByRequestOrigin(long nativeOfflinePageBridge, OfflinePageBridge caller,
                List<OfflinePageItem> result, String requestOrigin,
                Callback<List<OfflinePageItem>> callback);
        void getPagesByNamespace(long nativeOfflinePageBridge, OfflinePageBridge caller,
                List<OfflinePageItem> result, String nameSpace,
                Callback<List<OfflinePageItem>> callback);
        void deletePagesByClientId(long nativeOfflinePageBridge, OfflinePageBridge caller,
                String[] namespaces, String[] ids, Callback<Integer> callback);
        void deletePagesByClientIdAndOrigin(long nativeOfflinePageBridge, OfflinePageBridge caller,
                String[] namespaces, String[] ids, String origin, Callback<Integer> callback);
        void deletePagesByOfflineId(long nativeOfflinePageBridge, OfflinePageBridge caller,
                long[] offlineIds, Callback<Integer> callback);
        void publishInternalPageByOfflineId(long nativeOfflinePageBridge, OfflinePageBridge caller,
                long offlineId, Callback<String> publishedCallback);
        void publishInternalPageByGuid(long nativeOfflinePageBridge, OfflinePageBridge caller,
                String guid, Callback<String> publishedCallback);
        void selectPageForOnlineUrl(long nativeOfflinePageBridge, OfflinePageBridge caller,
                String onlineUrl, int tabId, Callback<OfflinePageItem> callback);
        void savePage(long nativeOfflinePageBridge, OfflinePageBridge caller,
                SavePageCallback callback, WebContents webContents, String clientNamespace,
                String clientId, String origin);
        String getOfflinePageHeaderForReload(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        boolean isShowingOfflinePreview(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        boolean isShowingDownloadButtonInErrorPage(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        void scheduleDownload(long nativeOfflinePageBridge, OfflinePageBridge caller,
                WebContents webContents, String nameSpace, String url, int uiAction, String origin);
        boolean isOfflinePage(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        boolean isInPrivateDirectory(
                long nativeOfflinePageBridge, OfflinePageBridge caller, String filePath);
        boolean isTemporaryNamespace(
                long nativeOfflinePageBridge, OfflinePageBridge caller, String nameSpace);
        OfflinePageItem getOfflinePage(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        void checkForNewOfflineContent(long nativeOfflinePageBridge, OfflinePageBridge caller,
                long freshnessTimeMillis, Callback<String> callback);
        void getLoadUrlParamsByOfflineId(long nativeOfflinePageBridge, OfflinePageBridge caller,
                long offlineId, int location, Callback<LoadUrlParams> callback);
        boolean isShowingTrustedOfflinePage(
                long nativeOfflinePageBridge, OfflinePageBridge caller, WebContents webContents);
        void getLoadUrlParamsForOpeningMhtmlFileOrContent(long nativeOfflinePageBridge,
                OfflinePageBridge caller, String url, Callback<LoadUrlParams> callback);
        void acquireFileAccessPermission(long nativeOfflinePageBridge, OfflinePageBridge caller,
                WebContents webContents, Callback<Boolean> callback);
    }
}
