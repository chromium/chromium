// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.graphics.Bitmap;
import android.text.format.DateUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapk.lib.common.WebApkConstants;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;

import java.io.File;

/**
 * Stores data about an installed web app. Uses SharedPreferences to persist the data to disk.
 * This class must only be accessed via {@link WebappRegistry}, which is used to register and keep
 * track of web app data known to Chrome.
 */
public class WebappDataStorage {
    private static final String TAG = "WebappDataStorage";

    static final String SHARED_PREFS_FILE_PREFIX = "webapp_";
    static final String KEY_SPLASH_ICON = "splash_icon";
    static final String KEY_LAST_USED = "last_used";
    static final String KEY_URL = "url";
    static final String KEY_SCOPE = "scope";
    static final String KEY_ICON = "icon";
    static final String KEY_NAME = "name";
    static final String KEY_SHORT_NAME = "short_name";
    static final String KEY_DISPLAY_MODE = "display_mode";
    static final String KEY_ORIENTATION = "orientation";
    static final String KEY_THEME_COLOR = "theme_color";
    static final String KEY_BACKGROUND_COLOR = "background_color";
    static final String KEY_SOURCE = "source";
    static final String KEY_IS_ICON_GENERATED = "is_icon_generated";
    static final String KEY_IS_ICON_ADAPTIVE = "is_icon_adaptive";
    static final String KEY_LAUNCH_COUNT = "launch_count";
    static final String KEY_VERSION = "version";
    static final String KEY_WEBAPK_PACKAGE_NAME = "webapk_package_name";
    static final String KEY_WEBAPK_INSTALL_TIMESTAMP = "webapk_install_timestamp";
    static final String KEY_WEBAPK_UNINSTALL_TIMESTAMP = "webapk_uninstall_timestamp";
    static final String KEY_WEBAPK_MANIFEST_URL = "webapk_manifest_url";
    static final String KEY_WEBAPK_MANIFEST_ID = "webapk_manifest_id";
    static final String KEY_WEBAPK_VERSION_CODE = "webapk_version_code";

    // The completion time of the last check for whether the WebAPK's Web Manifest was updated.
    static final String KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME =
            "last_check_web_manifest_update_time";

    // The last time that the WebAPK update request completed (successfully or unsuccessfully).
    static final String KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME = "last_update_request_complete_time";

    // Whether the last WebAPK update request succeeded.
    static final String KEY_DID_LAST_UPDATE_REQUEST_SUCCEED = "did_last_update_request_succeed";

    // The update pipeline might hold off on updating while the WebAPK is in use. If the usage drags
    // on, it could result in a new update check being issued (restarting the update pipeline)
    // before the update takes place, which can result in the App Identity Update dialog being shown
    // again to the user (showing the same update they already approved). This setting helps prevent
    // that, by storing a hash of what the last accepted update contained.
    static final String KEY_LAST_UPDATE_HASH_ACCEPTED = "last_update_hash_accepted";

    // Whether to check updates less frequently.
    static final String KEY_RELAX_UPDATES = "relax_updates";

    // The shell Apk version requested in the last update.
    static final String KEY_LAST_REQUESTED_SHELL_APK_VERSION = "last_requested_shell_apk_version";

    // Whether to show the user the Snackbar disclosure UI.
    static final String KEY_SHOW_DISCLOSURE = "show_disclosure";

    // The path where serialized update data is written before uploading to the WebAPK server.
    static final String KEY_PENDING_UPDATE_FILE_PATH = "pending_update_file_path";

    // Whether to force an update.
    static final String KEY_SHOULD_FORCE_UPDATE = "should_force_update";

    // Whether an update has been scheduled.
    static final String KEY_UPDATE_SCHEDULED = "update_scheduled";

    // Status indicating a WebAPK is not updatable through chrome://webapks.
    public static final String NOT_UPDATABLE = "Not updatable";

    // Number of milliseconds between checks for whether the WebAPK's Web Manifest has changed.
    public static final long UPDATE_INTERVAL = DateUtils.DAY_IN_MILLIS;

    // Number of milliseconds between checks of updates for a WebAPK that is expected to check
    // updates less frequently. crbug.com/680128.
    public static final long RELAXED_UPDATE_INTERVAL = DateUtils.DAY_IN_MILLIS * 30;

    // The default shell Apk version of WebAPKs.
    static final int DEFAULT_SHELL_APK_VERSION = 1;

    // Invalid constants for timestamps and URLs. '0' is used as the invalid timestamp as
    // WebappRegistry and WebApkUpdateManager assume that timestamps are always valid.
    static final long TIMESTAMP_INVALID = 0;
    static final String URL_INVALID = "";
    static final int VERSION_INVALID = 0;

    // We use a heuristic to determine whether a web app is still installed on the home screen, as
    // there is no way to do so directly. Any web app which has been opened in the last ten days
    // is considered to be still on the home screen.
    static final long WEBAPP_LAST_OPEN_MAX_TIME = DateUtils.DAY_IN_MILLIS * 10;

    private static Factory sFactory = new Factory();

    private final String mId;
    private final SharedPreferences mPreferences;

    /**
     * Called after data has been retrieved from storage.
     * @param <T> The type of the data being retrieved.
     */
    public interface FetchCallback<T> {
        public void onDataRetrieved(T readObject);
    }

    /**
     * Factory used to generate WebappDataStorage objects.
     * Overridden in tests to inject mocked objects.
     */
    public static class Factory {
        /** Generates a WebappDataStorage instance for a specified web app. */
        public WebappDataStorage create(final String webappId) {
            return new WebappDataStorage(webappId);
        }
    }

    /**
     * Opens an instance of WebappDataStorage for the web app specified.
     *
     * @param webappId The ID of the web app.
     */
    static WebappDataStorage open(String webappId) {
        return sFactory.create(webappId);
    }

    /** Sets the factory used to generate WebappDataStorage objects. */
    public static void setFactoryForTests(Factory factory) {
        var oldValue = sFactory;
        sFactory = factory;
        ResettersForTesting.register(() -> sFactory = oldValue);
    }

    /**
     * Asynchronously retrieves the splash screen image associated with the web app. The work is
     * performed on a background thread as it requires a potentially expensive image decode.
     * @param callback Called when the splash screen image has been retrieved.
     *                 The bitmap result will be null if no image was found.
     */
    public void getSplashScreenImage(final FetchCallback<Bitmap> callback) {
        new AsyncTask<Bitmap>() {
            @Override
            protected final Bitmap doInBackground() {
                return BitmapHelper.decodeBitmapFromString(
                        mPreferences.getString(KEY_SPLASH_ICON, null));
            }

            @Override
            protected final void onPostExecute(Bitmap result) {
                assert callback != null;
                callback.onDataRetrieved(result);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Update the splash screen image associated with the web app with the specified data. The image
     * must have been encoded using {@link BitmapHelper#encodeBitmapAsString}.
     * @param splashScreenImage The image which should be shown on the splash screen of the web app.
     */
    public void updateSplashScreenImage(String splashScreenImage) {
        mPreferences.edit().putString(KEY_SPLASH_ICON, splashScreenImage).apply();
    }

    /**
     * Creates and returns a web app launch intent from the data stored in this object.
     * @return The web app launch intent.
     */
    public Intent createWebappLaunchIntent() {
        // Assume that all of the data is invalid if the version isn't set, so return a null intent.
        int version = mPreferences.getInt(KEY_VERSION, VERSION_INVALID);
        if (version == VERSION_INVALID) return null;

        // Use "standalone" as the default display mode as this was the original assumed default for
        // all web apps.
        return ShortcutHelper.createWebappShortcutIntent(
                mId,
                mPreferences.getString(KEY_URL, null),
                mPreferences.getString(KEY_SCOPE, null),
                mPreferences.getString(KEY_NAME, null),
                mPreferences.getString(KEY_SHORT_NAME, null),
                mPreferences.getString(KEY_ICON, null),
                version,
                mPreferences.getInt(KEY_DISPLAY_MODE, DisplayMode.STANDALONE),
                mPreferences.getInt(KEY_ORIENTATION, ScreenOrientationLockType.DEFAULT),
                mPreferences.getLong(KEY_THEME_COLOR, ColorUtils.INVALID_COLOR),
                mPreferences.getLong(KEY_BACKGROUND_COLOR, ColorUtils.INVALID_COLOR),
                mPreferences.getBoolean(KEY_IS_ICON_GENERATED, false),
                mPreferences.getBoolean(KEY_IS_ICON_ADAPTIVE, false));
    }

    /**
     * Updates the data stored in this object to match that in the supplied {@link
     * BrowserServicesIntentDataProvider}.
     */
    public void updateFromWebappIntentDataProvider(
            BrowserServicesIntentDataProvider intentDataProvider) {
        if (intentDataProvider == null) return;
        WebappInfo info = WebappInfo.create(intentDataProvider);

        SharedPreferences.Editor editor = mPreferences.edit();
        boolean updated = false;

        // The URL and scope may have been deleted by the user clearing their history. Check whether
        // they are present, and update if necessary.
        String url = mPreferences.getString(KEY_URL, URL_INVALID);
        if (url.equals(URL_INVALID)) {
            url = info.url();
            editor.putString(KEY_URL, url);
            updated = true;
        }

        if (mPreferences.getString(KEY_SCOPE, URL_INVALID).equals(URL_INVALID)) {
            editor.putString(KEY_SCOPE, info.scopeUrl());
            updated = true;
        }

        // For all other fields, assume that if the version key is present and equal to
        // WebappConstants.WEBAPP_SHORTCUT_VERSION, then all fields are present and do not need to
        // be updated. All fields except for the last used time, scope, and URL are either set or
        // cleared together.
        if (mPreferences.getInt(KEY_VERSION, VERSION_INVALID)
                != WebappConstants.WEBAPP_SHORTCUT_VERSION) {
            editor.putInt(KEY_VERSION, WebappConstants.WEBAPP_SHORTCUT_VERSION);

            if (info.isForWebApk()) {
                editor.putString(KEY_WEBAPK_PACKAGE_NAME, info.webApkPackageName());
                editor.putString(KEY_WEBAPK_MANIFEST_URL, info.manifestUrl());
                editor.putString(KEY_WEBAPK_MANIFEST_ID, info.manifestIdWithFallback());
                editor.putInt(KEY_WEBAPK_VERSION_CODE, info.webApkVersionCode());
                editor.putLong(
                        KEY_WEBAPK_INSTALL_TIMESTAMP,
                        fetchWebApkInstallTimestamp(info.webApkPackageName()));
            } else {
                editor.putString(KEY_NAME, info.name());
                editor.putString(KEY_SHORT_NAME, info.shortName());
                editor.putString(KEY_ICON, info.icon().encoded());
                editor.putInt(KEY_DISPLAY_MODE, info.displayMode());
                editor.putInt(KEY_ORIENTATION, info.orientation());
                editor.putLong(KEY_THEME_COLOR, info.toolbarColor());
                editor.putLong(KEY_BACKGROUND_COLOR, info.backgroundColor());
                editor.putBoolean(KEY_IS_ICON_GENERATED, info.isIconGenerated());
                editor.putBoolean(KEY_IS_ICON_ADAPTIVE, info.isIconAdaptive());
                editor.putInt(KEY_SOURCE, info.source());
            }
            updated = true;
        }
        if (updated) editor.apply();
    }

    /**
     * Returns true if this web app recently (within WEBAPP_LAST_OPEN_MAX_TIME milliseconds) was
     * either:
     * - registered with WebappRegistry
     * - launched from the homescreen
     */
    public boolean wasUsedRecently() {
        // WebappRegistry.register sets the last used time, so that counts as a 'launch'.
        return (TimeUtils.currentTimeMillis() - getLastUsedTimeMs() < WEBAPP_LAST_OPEN_MAX_TIME);
    }

    /**
     * Deletes the data for a web app by clearing all the information inside the SharedPreferences
     * file. This does NOT delete the file itself but the file is left empty.
     */
    public void delete() {
        deletePendingUpdateRequestFile();
        mPreferences.edit().clear().apply();
    }

    /**
     * Deletes the URL and scope, and sets all timestamps to 0 in SharedPreferences.
     * This does not remove the stored splash screen image (if any) for the app.
     */
    void clearHistory() {
        deletePendingUpdateRequestFile();

        SharedPreferences.Editor editor = mPreferences.edit();

        editor.remove(KEY_LAST_USED);
        editor.remove(KEY_URL);
        editor.remove(KEY_SCOPE);
        editor.remove(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME);
        editor.remove(KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME);
        editor.remove(KEY_DID_LAST_UPDATE_REQUEST_SUCCEED);
        editor.remove(KEY_LAST_UPDATE_HASH_ACCEPTED);
        editor.remove(KEY_RELAX_UPDATES);
        editor.remove(KEY_SHOW_DISCLOSURE);
        editor.remove(KEY_LAUNCH_COUNT);
        editor.remove(KEY_WEBAPK_UNINSTALL_TIMESTAMP);
        editor.apply();

        // Don't clear fields which can be fetched from WebAPK manifest.
    }

    /** Returns the scope stored in this object, or URL_INVALID if it is not stored. */
    public String getScope() {
        return mPreferences.getString(KEY_SCOPE, URL_INVALID);
    }

    /** Returns the URL stored in this object, or URL_INVALID if it is not stored. */
    public String getUrl() {
        return mPreferences.getString(KEY_URL, URL_INVALID);
    }

    /** Returns the source stored in this object, or ShortcutSource.UNKNOWN if it is not stored. */
    public int getSource() {
        return mPreferences.getInt(KEY_SOURCE, ShortcutSource.UNKNOWN);
    }

    /** Updates the source. */
    public void updateSource(int source) {
        mPreferences.edit().putInt(KEY_SOURCE, source).apply();
    }

    /** Returns the id stored in this object. */
    public String getId() {
        return mId;
    }

    /** Returns the last used time, in milliseconds, of this object, or -1 if it is not stored. */
    public long getLastUsedTimeMs() {
        return mPreferences.getLong(KEY_LAST_USED, TIMESTAMP_INVALID);
    }

    /**
     * Update the information associated with the web app with the specified data. Used for testing.
     * @param splashScreenImage The image encoded as a string which should be shown on the splash
     *                          screen of the web app.
     */
    void updateSplashScreenImageForTests(String splashScreenImage) {
        mPreferences.edit().putString(KEY_SPLASH_ICON, splashScreenImage).apply();
    }

    /**
     * Update the package name of the WebAPK. Used for testing.
     * @param webApkPackageName The package name of the WebAPK.
     */
    void updateWebApkPackageNameForTests(String webApkPackageName) {
        mPreferences.edit().putString(KEY_WEBAPK_PACKAGE_NAME, webApkPackageName).apply();
    }

    /** Updates the last used time of this object. */
    void updateLastUsedTime() {
        mPreferences.edit().putLong(KEY_LAST_USED, TimeUtils.currentTimeMillis()).apply();
    }

    /** Returns the package name if the data is for a WebAPK, null otherwise. */
    public String getWebApkPackageName() {
        return mPreferences.getString(KEY_WEBAPK_PACKAGE_NAME, null);
    }

    /**
     * Updates the time of the completion of the last check for whether the WebAPK's Web Manifest
     * was updated.
     */
    void updateTimeOfLastCheckForUpdatedWebManifest() {
        mPreferences
                .edit()
                .putLong(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME, TimeUtils.currentTimeMillis())
                .apply();
    }

    /**
     * Returns the completion time, in milliseconds, of the last check for whether the WebAPK's Web
     * Manifest was updated. This time needs to be set when the WebAPK is registered.
     */
    public long getLastCheckForWebManifestUpdateTimeMs() {
        return mPreferences.getLong(KEY_LAST_CHECK_WEB_MANIFEST_UPDATE_TIME, TIMESTAMP_INVALID);
    }

    /** Updates when the last WebAPK update request finished (successfully or unsuccessfully). */
    void updateTimeOfLastWebApkUpdateRequestCompletion() {
        mPreferences
                .edit()
                .putLong(KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME, TimeUtils.currentTimeMillis())
                .apply();
    }

    /**
     * Returns the time, in milliseconds, that the last WebAPK update request completed
     * (successfully or unsuccessfully). This time needs to be set when the WebAPK is registered.
     */
    public long getLastWebApkUpdateRequestCompletionTimeMs() {
        return mPreferences.getLong(KEY_LAST_UPDATE_REQUEST_COMPLETE_TIME, TIMESTAMP_INVALID);
    }

    /** Updates whether the last update request to WebAPK Server succeeded. */
    void updateDidLastWebApkUpdateRequestSucceed(boolean success) {
        mPreferences.edit().putBoolean(KEY_DID_LAST_UPDATE_REQUEST_SUCCEED, success).apply();
    }

    /** Returns whether the last update request to WebAPK Server succeeded. */
    boolean getDidLastWebApkUpdateRequestSucceed() {
        return mPreferences.getBoolean(KEY_DID_LAST_UPDATE_REQUEST_SUCCEED, false);
    }

    /** Updates the `hash` of the last accepted identity update that was approved. */
    public void updateLastWebApkUpdateHashAccepted(String hash) {
        mPreferences.edit().putString(KEY_LAST_UPDATE_HASH_ACCEPTED, hash).apply();
    }

    /** Returns the `hash` of the last accepted identity update that was approved. */
    String getLastWebApkUpdateHashAccepted() {
        return mPreferences.getString(KEY_LAST_UPDATE_HASH_ACCEPTED, "");
    }

    /**
     * Returns whether to show the user a privacy disclosure (used for TWAs and unbound WebAPKs).
     * This is not cleared until the user explicitly acknowledges it.
     */
    public boolean shouldShowDisclosure() {
        return mPreferences.getBoolean(KEY_SHOW_DISCLOSURE, false);
    }

    /**
     * Clears the show disclosure bit, this stops TWAs and unbound WebAPKs from showing a privacy
     * disclosure on every resume of the Webapp. This should be called when the user has
     * acknowledged the disclosure.
     */
    public void clearShowDisclosure() {
        mPreferences.edit().putBoolean(KEY_SHOW_DISCLOSURE, false).apply();
    }

    /**
     * Sets the disclosure bit which causes TWAs and unbound WebAPKs to show a privacy disclosure.
     * This is set the first time an app is opened without storage (either right after install or
     * after Chrome's storage is cleared).
     */
    public void setShowDisclosure() {
        mPreferences.edit().putBoolean(KEY_SHOW_DISCLOSURE, true).apply();
    }

    /** Updates the shell Apk version requested in the last update. */
    void updateLastRequestedShellApkVersion(int shellApkVersion) {
        mPreferences.edit().putInt(KEY_LAST_REQUESTED_SHELL_APK_VERSION, shellApkVersion).apply();
    }

    /** Returns the shell Apk version requested in last update. */
    int getLastRequestedShellApkVersion() {
        return mPreferences.getInt(KEY_LAST_REQUESTED_SHELL_APK_VERSION, DEFAULT_SHELL_APK_VERSION);
    }

    /**
     * Returns whether the previous WebAPK update attempt succeeded. Returns true if there has not
     * been any update attempts.
     */
    boolean didPreviousUpdateSucceed() {
        long lastUpdateCompletionTime = getLastWebApkUpdateRequestCompletionTimeMs();
        if (lastUpdateCompletionTime == TIMESTAMP_INVALID) {
            return true;
        }
        return getDidLastWebApkUpdateRequestSucceed();
    }

    /** Sets whether we should check for updates less frequently. */
    void setRelaxedUpdates(boolean relaxUpdates) {
        mPreferences.edit().putBoolean(KEY_RELAX_UPDATES, relaxUpdates).apply();
    }

    /** Returns whether we should check for updates less frequently. */
    public boolean shouldRelaxUpdates() {
        return mPreferences.getBoolean(KEY_RELAX_UPDATES, false);
    }

    /** Sets whether an update has been scheduled. */
    public void setUpdateScheduled(boolean isUpdateScheduled) {
        mPreferences.edit().putBoolean(KEY_UPDATE_SCHEDULED, isUpdateScheduled).apply();
    }

    /** Gets whether an update has been scheduled. */
    public boolean isUpdateScheduled() {
        return mPreferences.getBoolean(KEY_UPDATE_SCHEDULED, false);
    }

    /** Whether a WebAPK is unbound. */
    private boolean isUnboundWebApk() {
        String webApkPackageName = getWebApkPackageName();
        return (webApkPackageName != null
                && !webApkPackageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX));
    }

    /** Sets whether an update should be forced. */
    public void setShouldForceUpdate(boolean forceUpdate) {
        if (!isUnboundWebApk()) {
            mPreferences.edit().putBoolean(KEY_SHOULD_FORCE_UPDATE, forceUpdate).apply();
        }
    }

    /** Whether to force an update. */
    public boolean shouldForceUpdate() {
        return mPreferences.getBoolean(KEY_SHOULD_FORCE_UPDATE, false);
    }

    /** Returns the update status. */
    public String getUpdateStatus() {
        if (isUnboundWebApk()) return NOT_UPDATABLE;
        if (isUpdateScheduled()) return "Scheduled";
        if (shouldForceUpdate()) return "Pending";
        return didPreviousUpdateSucceed() ? "Succeeded" : "Failed";
    }

    /**
     * Returns file where WebAPK update data should be stored and stores the file name in
     * SharedPreferences.
     */
    String createAndSetUpdateRequestFilePath(WebappInfo info) {
        String filePath = WebappDirectoryManager.getWebApkUpdateFilePathForStorage(this).getPath();
        mPreferences.edit().putString(KEY_PENDING_UPDATE_FILE_PATH, filePath).apply();
        return filePath;
    }

    /** Returns the path of the file which contains data to update the WebAPK. */
    @Nullable
    String getPendingUpdateRequestPath() {
        return mPreferences.getString(KEY_PENDING_UPDATE_FILE_PATH, null);
    }

    /**
     * Deletes the file which contains data to update the WebAPK. The file is large (> 1Kb) and
     * should be deleted when the update completes.
     */
    void deletePendingUpdateRequestFile() {
        final String pendingUpdateFilePath = getPendingUpdateRequestPath();
        if (pendingUpdateFilePath == null) return;

        mPreferences.edit().remove(KEY_PENDING_UPDATE_FILE_PATH).apply();
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    if (!new File(pendingUpdateFilePath).delete()) {
                        Log.d(TAG, "Failed to delete file " + pendingUpdateFilePath);
                    }
                });
    }

    /**
     * Returns whether a check for whether the Web Manifest needs to be updated has occurred in the
     * last {@link numMillis} milliseconds.
     */
    boolean wasCheckForUpdatesDoneInLastMs(long numMillis) {
        return (TimeUtils.currentTimeMillis() - getLastCheckForWebManifestUpdateTimeMs())
                < numMillis;
    }

    /** Returns whether we should check for update. */
    boolean shouldCheckForUpdate() {
        if (shouldForceUpdate()) return true;
        long checkUpdatesInterval =
                shouldRelaxUpdates() ? RELAXED_UPDATE_INTERVAL : UPDATE_INTERVAL;
        long now = TimeUtils.currentTimeMillis();
        long sinceLastCheckDurationMs = now - getLastCheckForWebManifestUpdateTimeMs();
        return sinceLastCheckDurationMs >= checkUpdatesInterval;
    }

    protected WebappDataStorage(String webappId) {
        mId = webappId;
        mPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                SHARED_PREFS_FILE_PREFIX + webappId, Context.MODE_PRIVATE);
    }

    /** Fetches the timestamp that the WebAPK was installed from the PackageManager. */
    private long fetchWebApkInstallTimestamp(String webApkPackageName) {
        PackageInfo packageInfo = PackageUtils.getPackageInfo(webApkPackageName, 0);
        return packageInfo == null ? 0 : packageInfo.firstInstallTime;
    }

    /** Returns the timestamp when the WebAPK was installed. */
    public long getWebApkInstallTimestamp() {
        return mPreferences.getLong(KEY_WEBAPK_INSTALL_TIMESTAMP, 0);
    }

    /** Sets the timestamp when the WebAPK was uninstalled to the current time. */
    public void setWebApkUninstallTimestamp() {
        mPreferences
                .edit()
                .putLong(KEY_WEBAPK_UNINSTALL_TIMESTAMP, TimeUtils.currentTimeMillis())
                .apply();
    }

    /** Returns the timestamp when the WebAPK was uninstalled. */
    public long getWebApkUninstallTimestamp() {
        return mPreferences.getLong(KEY_WEBAPK_UNINSTALL_TIMESTAMP, 0);
    }

    /** Increments the number of times that the webapp was launched. */
    public void incrementLaunchCount() {
        int launchCount = getLaunchCount();
        mPreferences.edit().putInt(KEY_LAUNCH_COUNT, launchCount + 1).apply();
    }

    /** Returns the number of times that the webapp was launched. */
    public int getLaunchCount() {
        return mPreferences.getInt(KEY_LAUNCH_COUNT, 0);
    }

    /** Returns cached Web Manifest URL. */
    public String getWebApkManifestUrl() {
        return mPreferences.getString(KEY_WEBAPK_MANIFEST_URL, null);
    }

    /** Returns cached Web Manifest ID. */
    public String getWebApkManifestId() {
        return mPreferences.getString(KEY_WEBAPK_MANIFEST_ID, null);
    }

    /** Returns cached WebAPK version code. */
    public int getWebApkVersionCode() {
        return mPreferences.getInt(KEY_WEBAPK_VERSION_CODE, 0);
    }
}
