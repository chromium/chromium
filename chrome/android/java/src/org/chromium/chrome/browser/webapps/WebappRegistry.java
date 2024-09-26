// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.browsing_data.UrlFilter;
import org.chromium.chrome.browser.browsing_data.UrlFilterBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.sync.protocol.WebApkSpecifics;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Singleton class which tracks web apps backed by a SharedPreferences file (abstracted by the
 * WebappDataStorage class). This class must be used on the main thread, except when warming
 * SharedPreferences.
 *
 * Aside from web app registration, which is asynchronous as a new SharedPreferences file must be
 * opened, all methods in this class are synchronous. All web app SharedPreferences known to
 * WebappRegistry are pre-warmed on browser startup when creating the singleton WebappRegistry
 * instance, whilst registering a new web app will automatically cache the new SharedPreferences
 * after it is created.
 *
 * This class is not a comprehensive list of installed web apps because it is impossible to know
 * when the user removes a web app from the home screen. The WebappDataStorage.wasUsedRecently()
 * heuristic attempts to compensate for this.
 */
public class WebappRegistry {

    static final String REGISTRY_FILE_NAME = "webapp_registry";
    static final String KEY_WEBAPP_SET = "webapp_set";
    static final String KEY_LAST_CLEANUP = "last_cleanup";

    /** Represents a period of 4 weeks in milliseconds */
    static final long FULL_CLEANUP_DURATION = DateUtils.WEEK_IN_MILLIS * 4;

    /** Represents a period of 13 weeks in milliseconds */
    static final long WEBAPP_UNOPENED_CLEANUP_DURATION = DateUtils.WEEK_IN_MILLIS * 13;

    /** Initialization-on-demand holder. This exists for thread-safe lazy initialization. */
    private static class Holder {
        // Not final for testing.
        private static WebappRegistry sInstance = new WebappRegistry();
    }

    private boolean mIsInitialized;

    /** Maps webapp ids to storages. */
    private Map<String, WebappDataStorage> mStorages;

    private SharedPreferences mPreferences;
    private InstalledWebappPermissionStore mPermissionStore;

    /**
     * Callback run when a WebappDataStorage object is registered for the first time. The storage
     * parameter will never be null.
     */
    public interface FetchWebappDataStorageCallback {
        void onWebappDataStorageRetrieved(WebappDataStorage storage);
    }

    private WebappRegistry() {
        mPreferences = openSharedPreferences();
        mStorages = new HashMap<>();
        mPermissionStore = new InstalledWebappPermissionStore();
    }

    /** Returns the singleton WebappRegistry instance. Creates the instance on first call. */
    public static WebappRegistry getInstance() {
        return Holder.sInstance;
    }

    /**
     * Warm up the WebappRegistry and a specific WebappDataStorage SharedPreferences. Can be called
     * from any thread.
     * @param id The web app id to warm up in addition to the WebappRegistry.
     */
    public static void warmUpSharedPrefsForId(String id) {
        getInstance().initStorages(id);
    }

    /**
     * Warm up the WebappRegistry and all WebappDataStorage SharedPreferences. Can be called from
     * any thread.
     */
    public static void warmUpSharedPrefs() {
        getInstance().initStorages(null);
    }

    public static void refreshSharedPrefsForTesting() {
        Holder.sInstance = new WebappRegistry();
        getInstance().clearStoragesForTesting();
        getInstance().initStorages(null);
    }

    /**
     * Registers the existence of a web app, creates a SharedPreference entry for it, and runs the
     * supplied callback (if not null) on the UI thread with the resulting WebappDataStorage object.
     *
     * @param webappId The id of the web app to register.
     * @param callback The callback to run with the WebappDataStorage argument.
     */
    public void register(final String webappId, final FetchWebappDataStorageCallback callback) {
        new AsyncTask<WebappDataStorage>() {
            @Override
            protected final WebappDataStorage doInBackground() {
                // Create the WebappDataStorage on the background thread, as this must create and
                // open a new SharedPreferences.
                WebappDataStorage storage = WebappDataStorage.open(webappId);
                // Access the WebappDataStorage to force it to finish loading. A strict mode
                // exception is thrown if the WebappDataStorage is accessed on the UI thread prior
                // to the storage being fully loaded.
                storage.getLastUsedTimeMs();
                return storage;
            }

            @Override
            protected final void onPostExecute(WebappDataStorage storage) {
                // Update the last used time in order to prevent
                // {@link WebappRegistry@unregisterOldWebapps()} from deleting the
                // WebappDataStorage. Must be run on the main thread as
                // SharedPreferences.Editor.apply() is called.
                mStorages.put(webappId, storage);
                mPreferences.edit().putStringSet(KEY_WEBAPP_SET, mStorages.keySet()).apply();
                storage.updateLastUsedTime();
                if (callback != null) callback.onWebappDataStorageRetrieved(storage);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Returns the WebappDataStorage object for webappId, or null if one cannot be found.
     * @param webappId The id of the web app.
     * @return The storage object for the web app, or null if webappId is not registered.
     */
    public WebappDataStorage getWebappDataStorage(String webappId) {
        return mStorages.get(webappId);
    }

    /**
     * Returns the WebappDataStorage object whose scope most closely matches the provided URL, or
     * null if a matching web app cannot be found. The most closely matching scope is the longest
     * scope which has the same prefix as the URL to open.
     * Note: this function skips any storage object associated with WebAPKs.
     * @param url The URL to search for.
     * @return The storage object for the web app, or null if one cannot be found.
     */
    public WebappDataStorage getWebappDataStorageForUrl(final String url) {
        WebappDataStorage bestMatch = null;
        int largestOverlap = 0;
        for (WebappDataStorage storage : mStorages.values()) {
            if (storage.getId().startsWith(WebApkConstants.WEBAPK_ID_PREFIX)) continue;

            String scope = storage.getScope();
            if (url.startsWith(scope) && scope.length() > largestOverlap) {
                bestMatch = storage;
                largestOverlap = scope.length();
            }
        }
        return bestMatch;
    }

    /**
     * Returns a string representation of the WebAPK scope URL, or the empty string if the storage
     * is not for a WebAPK.
     * @param storage The storage to extract the scope URL from.
     */
    private String getWebApkScopeFromStorage(WebappDataStorage storage) {
        if (!storage.getId().startsWith(WebApkConstants.WEBAPK_ID_PREFIX)) {
            return "";
        }

        String scope = storage.getScope();

        return scope;
    }

    /**
     * Returns true if a WebAPK is found whose scope matches |origin|.
     * @param origin The origin to search a WebAPK for.
     */
    public boolean hasAtLeastOneWebApkForOrigin(String origin) {
        for (WebappDataStorage storage : mStorages.values()) {
            String scope = getWebApkScopeFromStorage(storage);
            if (scope.isEmpty()) continue;

            if (scope.startsWith(origin)
                    && PackageUtils.isPackageInstalled(storage.getWebApkPackageName())) {
                return true;
            }
        }
        return false;
    }

    /** Returns a Set of all origins that have an installed WebAPK. */
    private Set<String> getOriginsWithWebApk() {
        Set<String> origins = new HashSet<>();
        for (WebappDataStorage storage : mStorages.values()) {
            String scope = getWebApkScopeFromStorage(storage);
            if (scope.isEmpty()) continue;

            origins.add(Origin.create(scope).toString());
        }
        return origins;
    }

    /** Returns an array of all origins that have an installed WebAPK. */
    @CalledByNative
    private static String[] getOriginsWithWebApkAsArray() {
        Set<String> origins = WebappRegistry.getInstance().getOriginsWithWebApk();
        String[] originsArray = new String[origins.size()];
        return origins.toArray(originsArray);
    }

    /*
     * Returns an array of serialized |WebApkSpecifics| protos in byte[] format.
     */
    @CalledByNative
    public static byte[][] getWebApkSpecifics() {
        List<WebApkSpecifics> webApkSpecifics =
                WebappRegistry.getInstance()
                        .getWebApkSpecificsImpl(/* setWebappInfoForTesting= */ null);
        List<byte[]> specificsBytes = new ArrayList<byte[]>();
        for (WebApkSpecifics specifics : webApkSpecifics) {
            specificsBytes.add(specifics.toByteArray());
        }

        byte[][] specificsBytesArray = new byte[specificsBytes.size()][];
        return specificsBytes.toArray(specificsBytesArray);
    }

    /*
     * Callback interface used for testing getWebApkSpecificsImpl().
     */
    public interface GetWebApkSpecificsImplSetWebappInfoForTesting {
        void run(String scope);
    }

    /*
     * Returns a List of |WebApkSpecifics| protos.
     */
    public List<WebApkSpecifics> getWebApkSpecificsImpl(
            GetWebApkSpecificsImplSetWebappInfoForTesting setWebappInfoForTesting) {
        List<WebApkSpecifics> webApkSpecificsList = new ArrayList<WebApkSpecifics>();
        for (WebappDataStorage storage : mStorages.values()) {
            String scope = getWebApkScopeFromStorage(storage);
            if (scope.isEmpty()) {
                continue;
            }

            if (setWebappInfoForTesting != null) {
                setWebappInfoForTesting.run(scope);
            }

            WebappInfo webApkInfo = WebApkDataProvider.getPartialWebappInfo(scope);
            WebApkSpecifics webApkSpecifics =
                    WebApkSyncService.getWebApkSpecifics(webApkInfo, storage);
            if (webApkSpecifics == null) {
                continue;
            }
            webApkSpecificsList.add(webApkSpecifics);
        }
        return webApkSpecificsList;
    }

    /** Checks whether a TWA is installed for the origin, and no WebAPK. */
    public boolean isTwaInstalled(String origin) {
        Set<String> webApkOrigins = getOriginsWithWebApk();
        Set<String> installedWebappOrigins = mPermissionStore.getStoredOrigins();
        return installedWebappOrigins.contains(origin) && !webApkOrigins.contains(origin);
    }

    /** Returns all origins that have a WebAPK or TWA installed. */
    public Set<String> getOriginsWithInstalledApp() {
        Set<String> origins = new HashSet<>();
        origins.addAll(getOriginsWithWebApk());
        origins.addAll(mPermissionStore.getStoredOrigins());
        return origins;
    }

    /** Returns an array of all origins that have a WebAPK or TWA installed. */
    @CalledByNative
    public static String[] getOriginsWithInstalledAppAsArray() {
        Set<String> origins = WebappRegistry.getInstance().getOriginsWithInstalledApp();
        String[] originsArray = new String[origins.size()];
        return origins.toArray(originsArray);
    }

    /**
     * Sets an Android Shared Preference bit to indicate that there are WebAPKs that need to be
     * restored from Sync on Chrome's 2nd run.
     */
    @CalledByNative
    public static void setNeedsPwaRestore(boolean needs) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.PWA_RESTORE_APPS_AVAILABLE, needs);
    }

    /**
     * Gets the value of an Android Shared Preference bit which indicates whether or not there are
     * WebAPKs that need to be restored from Sync on Chrome's 2nd run.
     */
    @CalledByNative
    public static boolean getNeedsPwaRestore() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.PWA_RESTORE_APPS_AVAILABLE, false);
    }

    /**
     * Returns the list of WebAPK IDs with pending updates. Filters out WebAPKs which have been
     * uninstalled.
     */
    public List<String> findWebApksWithPendingUpdate() {
        List<String> webApkIdsWithPendingUpdate = new ArrayList<>();
        for (HashMap.Entry<String, WebappDataStorage> entry : mStorages.entrySet()) {
            WebappDataStorage storage = entry.getValue();
            if (!TextUtils.isEmpty(storage.getPendingUpdateRequestPath())
                    && PackageUtils.isPackageInstalled(storage.getWebApkPackageName())) {
                webApkIdsWithPendingUpdate.add(entry.getKey());
            }
        }
        return webApkIdsWithPendingUpdate;
    }

    /**
     * Returns the WebAPK PackageName whose manifestId matches the provided one. Returns null
     * if no matches.
     * @param manifestId The manifestId to search for.
     * @return The package name for the WebAPK, or null if one cannot be found.
     **/
    public @Nullable String findWebApkWithManifestId(String manifestId) {
        WebappDataStorage storage = getWebappDataStorageForManifestId(manifestId);
        if (storage != null) {
            return storage.getWebApkPackageName();
        }
        return null;
    }

    /**
     * Returns the WebappDataStorage object whose manifestId matches the provided manifestId.
     * Note: this function skips any storage object associated with WebAPKs.
     * @param manifestId The manifestId to search for.
     * @return The storage object for the WebAPK, or null if one cannot be found.
     */
    WebappDataStorage getWebappDataStorageForManifestId(final String manifestId) {
        if (TextUtils.isEmpty(manifestId)) return null;

        for (WebappDataStorage storage : mStorages.values()) {
            if (!storage.getId().startsWith(WebApkConstants.WEBAPK_ID_PREFIX)) continue;

            if (TextUtils.equals(storage.getWebApkManifestId(), manifestId)) {
                return storage;
            }
        }
        return null;
    }

    /** Returns the list of web app IDs which are written to SharedPreferences. */
    public static Set<String> getRegisteredWebappIdsForTesting() {
        // Wrap with unmodifiableSet to ensure it's never modified. See crbug.com/568369.
        return Collections.unmodifiableSet(
                openSharedPreferences().getStringSet(KEY_WEBAPP_SET, Collections.emptySet()));
    }

    void clearForTesting() {
        Iterator<HashMap.Entry<String, WebappDataStorage>> it = mStorages.entrySet().iterator();
        while (it.hasNext()) {
            it.next().getValue().delete();
            it.remove();
        }
        mPreferences.edit().putStringSet(KEY_WEBAPP_SET, mStorages.keySet()).apply();
    }

    /**
     * Deletes the data for all "old" web apps, as well as all WebAPKs that have been uninstalled in
     * the last month, and removes all WebAPKs from Sync which haven't been used in the last month.
     * "Old" web apps have not been opened by the user in the last 3 months, or have had their last
     * used time set to 0 by the user clearing their history. Cleanup is run, at most, once a month.
     *
     * @param currentTime The current time which will be checked to decide if the task should be run
     *     and if a web app should be cleaned up.
     */
    public void unregisterOldWebapps(long currentTime) {
        if ((currentTime - mPreferences.getLong(KEY_LAST_CLEANUP, 0)) < FULL_CLEANUP_DURATION) {
            return;
        }

        Iterator<HashMap.Entry<String, WebappDataStorage>> it = mStorages.entrySet().iterator();
        while (it.hasNext()) {
            HashMap.Entry<String, WebappDataStorage> entry = it.next();
            WebappDataStorage storage = entry.getValue();
            String webApkPackage = storage.getWebApkPackageName();
            if (webApkPackage != null) {
                if (!shouldDeleteStorageForWebApk(entry.getKey(), webApkPackage)) {
                    continue;
                }
            } else if ((currentTime - storage.getLastUsedTimeMs())
                    < WEBAPP_UNOPENED_CLEANUP_DURATION) {
                continue;
            }
            storage.delete();
            it.remove();
        }

        WebApkSyncService.removeOldWebAPKsFromSync(currentTime);

        mPreferences
                .edit()
                .putLong(KEY_LAST_CLEANUP, currentTime)
                .putStringSet(KEY_WEBAPP_SET, mStorages.keySet())
                .apply();
    }

    /**
     * Returns whether the {@link WebappDataStorage} should be deleted for the passed-in WebAPK
     * package.
     */
    private static boolean shouldDeleteStorageForWebApk(
            @NonNull String id, @NonNull String webApkPackageName) {
        // Prefix check that the key matches the current scheme instead of an old deprecated naming
        // scheme. This is necessary as we migrate away from the old naming scheme and garbage
        // collect.
        if (!id.startsWith(WebApkConstants.WEBAPK_ID_PREFIX)) return true;

        // Do not delete WebappDataStorage if we still need it for UKM logging.
        Set<String> webApkPackagesWithPendingUkm =
                ChromeSharedPreferences.getInstance()
                        .readStringSet(ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES);
        if (webApkPackagesWithPendingUkm.contains(webApkPackageName)) return false;

        return !PackageUtils.isPackageInstalled(webApkPackageName);
    }

    public InstalledWebappPermissionStore getPermissionStore() {
        return mPermissionStore;
    }

    /**
     * Deletes the data of all web apps whose url matches |urlFilter|.
     * @param urlFilter The filter object to check URLs.
     */
    @VisibleForTesting
    void unregisterWebappsForUrlsImpl(UrlFilter urlFilter) {
        Iterator<HashMap.Entry<String, WebappDataStorage>> it = mStorages.entrySet().iterator();
        while (it.hasNext()) {
            HashMap.Entry<String, WebappDataStorage> entry = it.next();
            WebappDataStorage storage = entry.getValue();
            if (urlFilter.matchesUrl(storage.getUrl())) {
                storage.delete();
                it.remove();
            }
        }

        if (mStorages.isEmpty()) {
            mPreferences.edit().clear().apply();
        } else {
            mPreferences.edit().putStringSet(KEY_WEBAPP_SET, mStorages.keySet()).apply();
        }
    }

    @CalledByNative
    static void unregisterWebappsForUrls(UrlFilterBridge urlFilter) {
        WebappRegistry.getInstance().unregisterWebappsForUrlsImpl(urlFilter);
        urlFilter.destroy();
    }

    /**
     * Deletes the URL and scope, and sets the last used time to 0 for all web apps whose url
     * matches |urlFilter|.
     * @param urlFilter The filter object to check URLs.
     */
    @VisibleForTesting
    void clearWebappHistoryForUrlsImpl(UrlFilter urlFilter) {
        for (WebappDataStorage storage : mStorages.values()) {
            if (urlFilter.matchesUrl(storage.getUrl())) {
                storage.clearHistory();
            }
        }
    }

    @CalledByNative
    static void clearWebappHistoryForUrls(UrlFilterBridge urlFilter) {
        WebappRegistry.getInstance().clearWebappHistoryForUrlsImpl(urlFilter);
        urlFilter.destroy();
    }

    private static SharedPreferences openSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
    }

    private void clearStoragesForTesting() {
        ThreadUtils.assertOnUiThread();
        mStorages.clear();
    }

    private void initStorages(String idToInitialize) {
        Set<String> webapps = mPreferences.getStringSet(KEY_WEBAPP_SET, Collections.emptySet());
        boolean initAll = (idToInitialize == null || idToInitialize.isEmpty());
        boolean initializing = initAll && !mIsInitialized;

        if (initAll && !mIsInitialized) {
            mPermissionStore.initStorage();
            mIsInitialized = true;
        }

        List<Pair<String, WebappDataStorage>> initedStorages = new ArrayList<>();
        if (initAll) {
            for (String id : webapps) {
                // See crbug.com/1055566 for details on bug which caused this scenario to occur.
                if (id == null) {
                    id = "";
                }
                if (!mStorages.containsKey(id)) {
                    initedStorages.add(Pair.create(id, WebappDataStorage.open(id)));
                }
            }
        } else {
            if (webapps.contains(idToInitialize) && !mStorages.containsKey(idToInitialize)) {
                initedStorages.add(
                        Pair.create(idToInitialize, WebappDataStorage.open(idToInitialize)));
            }
        }

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    initStoragesOnUiThread(initedStorages, initializing);
                });
    }

    private void initStoragesOnUiThread(
            List<Pair<String, WebappDataStorage>> initedStorages, boolean isInitalizing) {
        ThreadUtils.assertOnUiThread();

        for (Pair<String, WebappDataStorage> initedStorage : initedStorages) {
            if (!mStorages.containsKey(initedStorage.first)) {
                mStorages.put(initedStorage.first, initedStorage.second);
            }
        }
        if (isInitalizing) {
            WebApkUmaRecorder.recordWebApksCount(getOriginsWithWebApk().size());
        }
    }
}
