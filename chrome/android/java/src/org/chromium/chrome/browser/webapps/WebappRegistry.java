// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.browsing_data.UrlFilter;
import org.chromium.chrome.browser.browsing_data.UrlFilterBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.sync.protocol.WebApkSpecifics;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.WebContents;
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
 * <p>Aside from web app registration, which is asynchronous as a new SharedPreferences file must be
 * opened, all methods in this class are synchronous. All web app SharedPreferences known to
 * WebappRegistry are pre-warmed on browser startup when creating the singleton WebappRegistry
 * instance, whilst registering a new web app will automatically cache the new SharedPreferences
 * after it is created.
 *
 * <p>This class is not a comprehensive list of installed web apps because it is impossible to know
 * when the user removes a web app from the home screen. The WebappDataStorage.wasUsedRecently()
 * heuristic attempts to compensate for this.
 */
@NullMarked
public class WebappRegistry {
    /** Observer for changes in the list of installed or pending web apps. */
    public interface Observer {
        void onOriginsWithInstalledAppChanged();

        default void onPendingAppInstallStatusChanged() {}
    }

    private @Nullable ObserverList<Observer> mObservers;

    private ObserverList<Observer> getObservers() {
        if (mObservers == null) {
            mObservers = new ObserverList<>();
        }
        return mObservers;
    }

    public void registerObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        getObservers().addObserver(observer);
    }

    public void unregisterObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        getObservers().removeObserver(observer);
    }

    public void notifyOriginsWithInstalledAppChanged() {
        ThreadUtils.assertOnUiThread();
        for (Observer observer : getObservers()) {
            observer.onOriginsWithInstalledAppChanged();
        }
    }

    public void notifyPendingAppInstallStatusChanged() {
        ThreadUtils.assertOnUiThread();
        for (Observer observer : getObservers()) {
            observer.onPendingAppInstallStatusChanged();
        }
    }

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
    private final Map<String, WebappDataStorage> mStorages;

    /**
     * Maps a WebAPK's manifest ID to its package name for installations that are in progress. This
     * in-memory map helps detect concurrent installation requests for the same manifest and allows
     * internal services to block duplicate installation attempts before the package is fully
     * registered in the system.
     */
    public static final String PENDING_PACKAGE_NAME_PLACEHOLDER = "pending_placeholder";

    private final Map<String, String> mPendingManifestIdToPackageName = new HashMap<>();

    private final SharedPreferences mPreferences;
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
        mPermissionStore.setListener(this::notifyOriginsWithInstalledAppChanged);
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

    public static void setInstanceForTests(WebappRegistry registry) {
        var oldValue = Holder.sInstance;
        Holder.sInstance = registry;
        ResettersForTesting.register(() -> Holder.sInstance = oldValue);
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
            protected WebappDataStorage doInBackground() {
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
            protected void onPostExecute(WebappDataStorage storage) {
                // Update the last used time in order to prevent
                // {@link WebappRegistry@unregisterOldWebapps()} from deleting the
                // WebappDataStorage. Must be run on the main thread as
                // SharedPreferences.Editor.apply() is called.
                mStorages.put(webappId, storage);
                mPreferences.edit().putStringSet(KEY_WEBAPP_SET, mStorages.keySet()).apply();
                storage.updateLastUsedTime();
                if (storage.getId() != null
                        && storage.getId().startsWith(WebApkConstants.WEBAPK_ID_PREFIX)) {
                    storage.resetWebApkUninstallTimestamp();
                }
                if (callback != null) callback.onWebappDataStorageRetrieved(storage);

                String manifestId = storage.getWebApkManifestId();
                if (manifestId != null) {
                    mPendingManifestIdToPackageName.remove(manifestId);
                    notifyPendingAppInstallStatusChanged();
                }
                notifyOriginsWithInstalledAppChanged();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Returns the WebappDataStorage object for webappId, or null if one cannot be found.
     *
     * @param webappId The id of the web app.
     * @return The storage object for the web app, or null if webappId is not registered.
     */
    public @Nullable WebappDataStorage getWebappDataStorage(@Nullable String webappId) {
        return mStorages.get(webappId);
    }

    /**
     * Returns the WebappDataStorage object whose scope most closely matches the provided URL, or
     * null if a matching web app cannot be found. The most closely matching scope is the longest
     * scope which has the same prefix as the URL to open. Note: this function skips any storage
     * object associated with WebAPKs.
     *
     * @param url The URL to search for.
     * @return The storage object for the web app, or null if one cannot be found.
     */
    public @Nullable WebappDataStorage getWebappDataStorageForUrl(final String url) {
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

            String webApkPackageName = storage.getWebApkPackageName();
            assumeNonNull(webApkPackageName);
            if (scope.startsWith(origin) && PackageUtils.isPackageInstalled(webApkPackageName)) {
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
            if (storage.getWebApkUninstallTimestamp() > 0) continue;

            Origin origin = Origin.create(scope);
            assumeNonNull(origin);
            origins.add(origin.toString());
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
            @Nullable GetWebApkSpecificsImplSetWebappInfoForTesting setWebappInfoForTesting) {
        List<WebApkSpecifics> webApkSpecificsList = new ArrayList<>();
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
        for (Map.Entry<String, WebappDataStorage> entry : mStorages.entrySet()) {
            WebappDataStorage storage = entry.getValue();
            String webApkPackageName = storage.getWebApkPackageName();
            assumeNonNull(webApkPackageName);
            if (!TextUtils.isEmpty(storage.getPendingUpdateRequestPath())
                    && PackageUtils.isPackageInstalled(webApkPackageName)) {
                webApkIdsWithPendingUpdate.add(entry.getKey());
            }
        }
        return webApkIdsWithPendingUpdate;
    }

    public void registerPendingWebApk(String manifestId, String packageName) {
        ThreadUtils.assertOnUiThread();
        mPendingManifestIdToPackageName.put(manifestId, packageName);
        notifyPendingAppInstallStatusChanged();
    }

    public void removePendingWebApk(String manifestId) {
        ThreadUtils.assertOnUiThread();
        if (mPendingManifestIdToPackageName.remove(manifestId) != null) {
            notifyPendingAppInstallStatusChanged();
        }
    }

    /** Returns whether there is a pending WebAPK installation for the given manifest ID. */
    public boolean isWebApkPending(@Nullable String manifestId) {
        if (manifestId == null) return false;
        return mPendingManifestIdToPackageName.containsKey(manifestId);
    }

    /** Returns whether a WebAPK with the given manifest ID was recently installed. */
    public boolean wasWebApkRecentlyInstalled(@Nullable String manifestId, long maxAgeMs) {
        if (manifestId == null) return false;

        String packageName = findWebApkWithManifestId(manifestId);
        if (packageName == null) return false;

        String webappId = WebappIntentUtils.getIdForWebApkPackage(packageName);
        WebappDataStorage storage = getWebappDataStorage(webappId);
        if (storage == null) return false;

        long registrationTime = storage.getLocalRegistrationTimestamp();
        long age = TimeUtils.currentTimeMillis() - registrationTime;
        return age < maxAgeMs;
    }

    /**
     * Returns the newest WebAPK PackageName whose manifestId matches the provided one. If multiple
     * WebAPKs match, the newest one is returned. It checks both pending installations and fully
     * registered apps. Returns null if no matches.
     *
     * @param manifestId The manifestId to search for.
     * @return The package name for the newest WebAPK, or null if one cannot be found.
     */
    public @Nullable String findWebApkWithManifestId(@Nullable String manifestId) {
        if (manifestId == null) return null;

        String pendingInstallPackageName = mPendingManifestIdToPackageName.get(manifestId);
        if (pendingInstallPackageName != null
                && !PENDING_PACKAGE_NAME_PLACEHOLDER.equals(pendingInstallPackageName)) {
            return pendingInstallPackageName;
        }

        String newestPackageName = null;
        long newestRegistrationTime = -1;
        for (WebappDataStorage storage : mStorages.values()) {
            if (!storage.getId().startsWith(WebApkConstants.WEBAPK_ID_PREFIX)) continue;
            String registeredManifestId = storage.getWebApkManifestId();
            if (TextUtils.equals(manifestId, registeredManifestId)) {
                long registrationTime = storage.getLocalRegistrationTimestamp();
                if (registrationTime > newestRegistrationTime) {
                    newestRegistrationTime = registrationTime;
                    newestPackageName = storage.getWebApkPackageName();
                }
            }
        }
        return newestPackageName;
    }

    /**
     * Returns the WebappDataStorage object whose manifestId matches the provided manifestId. Note:
     * this function skips any storage object associated with WebAPKs.
     *
     * @param manifestId The manifestId to search for.
     * @return The storage object for the WebAPK, or null if one cannot be found.
     */
    @Nullable WebappDataStorage getWebappDataStorageForManifestId(
            final @Nullable String manifestId) {
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
        // Wrap with unmodifiableSet to ensure it's never modified. See crbug.com/40448581.
        return Collections.unmodifiableSet(
                openSharedPreferences().getStringSet(KEY_WEBAPP_SET, Collections.emptySet()));
    }

    void clearForTesting() {
        mPendingManifestIdToPackageName.clear();
        Iterator<Map.Entry<String, WebappDataStorage>> it = mStorages.entrySet().iterator();
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

        boolean deleted = false;
        Iterator<Map.Entry<String, WebappDataStorage>> it = mStorages.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, WebappDataStorage> entry = it.next();
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
            deleted = true;
        }

        WebApkSyncService.removeOldWebAPKsFromSync(currentTime);

        mPreferences
                .edit()
                .putLong(KEY_LAST_CLEANUP, currentTime)
                .putStringSet(KEY_WEBAPP_SET, mStorages.keySet())
                .apply();

        if (deleted) {
            notifyOriginsWithInstalledAppChanged();
        }
    }

    /**
     * Returns whether the {@link WebappDataStorage} should be deleted for the passed-in WebAPK
     * package.
     */
    private static boolean shouldDeleteStorageForWebApk(String id, String webApkPackageName) {
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

    public void setPermissionStoreForTesting(InstalledWebappPermissionStore store) {
        var oldValue = mPermissionStore;
        mPermissionStore = store;
        ResettersForTesting.register(() -> mPermissionStore = oldValue);
    }

    /**
     * Deletes the data of all web apps whose url matches |urlFilter|.
     *
     * @param urlFilter The filter object to check URLs.
     */
    @VisibleForTesting
    void unregisterWebappsForUrlsImpl(UrlFilter urlFilter) {
        boolean deleted = false;
        Iterator<Map.Entry<String, WebappDataStorage>> it = mStorages.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, WebappDataStorage> entry = it.next();
            WebappDataStorage storage = entry.getValue();
            if (urlFilter.matchesUrl(storage.getUrl())) {
                storage.delete();
                it.remove();
                deleted = true;
            }
        }

        if (mStorages.isEmpty()) {
            mPreferences.edit().clear().apply();
        } else {
            mPreferences.edit().putStringSet(KEY_WEBAPP_SET, mStorages.keySet()).apply();
        }

        if (deleted) {
            notifyOriginsWithInstalledAppChanged();
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

    private void initStorages(@Nullable String idToInitialize) {
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
                // See crbug.com/40676347 for details on bug which caused this scenario to occur.
                if (id == null) {
                    id = "";
                }
                if (!mStorages.containsKey(id)) {
                    initedStorages.add(Pair.create(id, WebappDataStorage.open(id)));
                }
            }
        } else {
            assumeNonNull(idToInitialize);
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
        if (!initedStorages.isEmpty()) {
            notifyOriginsWithInstalledAppChanged();
        }
    }

    /** Resolves the manifest ID for the given tab, falling back to the tab's URL if empty. */
    public static String getManifestIdOrUrl(Tab tab) {
        @Nullable WebContents webContents = tab.getWebContents();
        String manifestId =
                webContents != null ? AppBannerManager.maybeGetManifestId(webContents) : null;
        if (TextUtils.isEmpty(manifestId)) {
            manifestId = tab.getUrl().getSpec();
        }
        return manifestId;
    }
}
