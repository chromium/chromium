// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.banners.InstallerDelegate;
import org.chromium.chrome.browser.browsing_data.UrlFilter;
import org.chromium.chrome.browser.browsing_data.UrlFilterBridge;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

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
    static final long FULL_CLEANUP_DURATION = TimeUnit.DAYS.toMillis(4L * 7L);

    /** Represents a period of 13 weeks in milliseconds */
    static final long WEBAPP_UNOPENED_CLEANUP_DURATION = TimeUnit.DAYS.toMillis(13L * 7L);

    /** Initialization-on-demand holder. This exists for thread-safe lazy initialization. */
    private static class Holder {
        // Not final for testing.
        private static WebappRegistry sInstance = new WebappRegistry();
    }

    private HashMap<String, WebappDataStorage> mStorages;
    private SharedPreferences mPreferences;

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
    }

    /**
     * Returns the singleton WebappRegistry instance. Creates the instance on first call.
     */
    public static WebappRegistry getInstance() {
        return Holder.sInstance;
    }

    /**
     * Warm up the WebappRegistry and a specific WebappDataStorage SharedPreferences.
     * @param id The web app id to warm up in addition to the WebappRegistry.
     */
    public static void warmUpSharedPrefsForId(String id) {
        getInstance().initStorages(id, false);
    }

    /**
     * Warm up the WebappRegistry and all WebappDataStorage SharedPreferences.
     */
    public static void warmUpSharedPrefs() {
        getInstance().initStorages(null, false);
    }

    @VisibleForTesting
    public static void refreshSharedPrefsForTesting() {
        Holder.sInstance = new WebappRegistry();
        getInstance().initStorages(null, true);
    }

    /**
     * Registers the existence of a web app, creates a SharedPreference entry for it, and runs the
     * supplied callback (if not null) on the UI thread with the resulting WebappDataStorage object.
     * @param webappId The id of the web app to register.
     * @param callback The callback to run with the WebappDataStorage argument.
     * @return The storage object for the web app.
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
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
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
        for (HashMap.Entry<String, WebappDataStorage> entry : mStorages.entrySet()) {
            WebappDataStorage storage = entry.getValue();
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
     * Returns the list of WebAPK IDs with pending updates. Filters out WebAPKs which have been
     * uninstalled.
     * */
    public List<String> findWebApksWithPendingUpdate() {
        ArrayList<String> webApkIdsWithPendingUpdate = new ArrayList<String>();
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        for (HashMap.Entry<String, WebappDataStorage> entry : mStorages.entrySet()) {
            WebappDataStorage storage = entry.getValue();
            if (!TextUtils.isEmpty(storage.getPendingUpdateRequestPath())
                    && InstallerDelegate.isInstalled(
                               packageManager, storage.getWebApkPackageName())) {
                webApkIdsWithPendingUpdate.add(entry.getKey());
            }
        }
        return webApkIdsWithPendingUpdate;
    }

    /**
     * Returns the list of web app IDs which are written to SharedPreferences.
     */
    @VisibleForTesting
    public static Set<String> getRegisteredWebappIdsForTesting() {
        // Wrap with unmodifiableSet to ensure it's never modified. See crbug.com/568369.
        return Collections.unmodifiableSet(openSharedPreferences().getStringSet(
                KEY_WEBAPP_SET, Collections.<String>emptySet()));
    }

    @VisibleForTesting
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
     * the last month. "Old" web apps have not been opened by the user in the last 3 months, or have
     * had their last used time set to 0 by the user clearing their history. Cleanup is run, at
     * most, once a month.
     * @param currentTime The current time which will be checked to decide if the task should be run
     *                    and if a web app should be cleaned up.
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
                // Prefix check that the key matches the current scheme instead of an old
                // deprecated naming scheme and that the WebApk is still installed. The former is
                // necessary as we migrate away from the old naming scheme and garbage collect.
                if (entry.getKey().startsWith(WebApkConstants.WEBAPK_ID_PREFIX)
                        && isWebApkInstalled(webApkPackage)) {
                    continue;
                }
            } else if ((currentTime - storage.getLastUsedTimeMs())
                    < WEBAPP_UNOPENED_CLEANUP_DURATION) {
                continue;
            }
            storage.delete();
            it.remove();
        }

        mPreferences.edit()
                .putLong(KEY_LAST_CLEANUP, currentTime)
                .putStringSet(KEY_WEBAPP_SET, mStorages.keySet())
                .apply();
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
        for (HashMap.Entry<String, WebappDataStorage> entry : mStorages.entrySet()) {
            WebappDataStorage storage = entry.getValue();
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

    /**
     * Returns true if the given WebAPK is installed.
     */
    private boolean isWebApkInstalled(String webApkPackage) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        return InstallerDelegate.isInstalled(packageManager, webApkPackage);
    }

    private static SharedPreferences openSharedPreferences() {
        return ContextUtils.getApplicationContext().getSharedPreferences(
                REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
    }

    private void initStorages(String idToInitialize, boolean replaceExisting) {
        Set<String> webapps =
                mPreferences.getStringSet(KEY_WEBAPP_SET, Collections.<String>emptySet());
        boolean initAll = (idToInitialize == null || idToInitialize.isEmpty());

        // Don't overwrite any entry in mStorages unless replaceExisting is set to true.
        if (initAll) {
            for (String id : webapps) {
                if (replaceExisting || !mStorages.containsKey(id)) {
                    mStorages.put(id, WebappDataStorage.open(id));
                }
            }
        } else {
            if (webapps.contains(idToInitialize)
                    && (replaceExisting || !mStorages.containsKey(idToInitialize))) {
                mStorages.put(idToInitialize, WebappDataStorage.open(idToInitialize));
            }
        }
    }
}
