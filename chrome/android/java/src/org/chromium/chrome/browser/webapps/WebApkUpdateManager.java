// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;

import android.graphics.Bitmap;
import android.os.Handler;
import android.text.TextUtils;
import android.text.format.DateUtils;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.webapk.lib.client.WebApkVersion;

import java.util.Map;

/**
 * WebApkUpdateManager manages when to check for updates to the WebAPK's Web Manifest, and sends
 * an update request to the WebAPK Server when an update is needed.
 */
public class WebApkUpdateManager implements WebApkUpdateDataFetcher.Observer {
    private static final String TAG = "WebApkUpdateManager";

    // Maximum wait time for WebAPK update to be scheduled.
    private static final long UPDATE_TIMEOUT_MILLISECONDS = DateUtils.SECOND_IN_MILLIS * 30;

    /** Whether updates are enabled. Some tests disable updates. */
    private static boolean sUpdatesEnabled = true;

    /** Data extracted from the WebAPK's launch intent and from the WebAPK's Android Manifest. */
    private WebApkInfo mInfo;

    /** The WebappDataStorage with cached data about prior update requests. */
    private final WebappDataStorage mStorage;

    private WebApkUpdateDataFetcher mFetcher;

    /** Runs failure callback if WebAPK update is not scheduled within deadline. */
    private Handler mUpdateFailureHandler;

    /** Called with update result. */
    public static interface WebApkUpdateCallback {
        @CalledByNative("WebApkUpdateCallback")
        public void onResultFromNative(@WebApkInstallResult int result, boolean relaxUpdates);
    }

    public WebApkUpdateManager(WebappDataStorage storage) {
        mStorage = storage;
    }

    /**
     * Checks whether the WebAPK's Web Manifest has changed. Requests an updated WebAPK if the Web
     * Manifest has changed. Skips the check if the check was done recently.
     * @param tab  The tab of the WebAPK.
     * @param info The WebApkInfo of the WebAPK.
     */
    public void updateIfNeeded(Tab tab, WebApkInfo info) {
        mInfo = info;

        if (!shouldCheckIfWebManifestUpdated(mInfo)) return;

        mFetcher = buildFetcher();
        mFetcher.start(tab, mInfo, this);
        mUpdateFailureHandler = new Handler();
        mUpdateFailureHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                onGotManifestData(null, null, null);
            }
        }, UPDATE_TIMEOUT_MILLISECONDS);
    }

    public void destroy() {
        destroyFetcher();
        if (mUpdateFailureHandler != null) {
            mUpdateFailureHandler.removeCallbacksAndMessages(null);
        }
    }

    public static void setUpdatesEnabledForTesting(boolean enabled) {
        sUpdatesEnabled = enabled;
    }

    @Override
    public void onGotManifestData(WebApkInfo fetchedInfo, String primaryIconUrl,
            String badgeIconUrl) {
        mStorage.updateTimeOfLastCheckForUpdatedWebManifest();
        if (mUpdateFailureHandler != null) {
            mUpdateFailureHandler.removeCallbacksAndMessages(null);
        }

        boolean gotManifest = (fetchedInfo != null);
        @WebApkUpdateReason
        int updateReason = needsUpdate(mInfo, fetchedInfo, primaryIconUrl, badgeIconUrl);
        boolean needsUpgrade = (updateReason != WebApkUpdateReason.NONE);
        if (mStorage.shouldForceUpdate() && needsUpgrade) {
            updateReason = WebApkUpdateReason.MANUALLY_TRIGGERED;
        }
        Log.i(TAG, "Got Manifest: " + gotManifest);
        Log.i(TAG, "WebAPK upgrade needed: " + needsUpgrade);

        // If the Web Manifest was not found and an upgrade is requested, stop fetching Web
        // Manifests as the user navigates to avoid sending multiple WebAPK update requests. In
        // particular:
        // - A WebAPK update request on the initial load because the Shell APK version is out of
        //   date.
        // - A second WebAPK update request once the user navigates to a page which points to the
        //   correct Web Manifest URL because the Web Manifest has been updated by the Web
        //   developer.
        //
        // If the Web Manifest was not found and an upgrade is not requested, keep on fetching
        // Web Manifests as the user navigates. For instance, the WebAPK's start_url might not
        // point to a Web Manifest because start_url redirects to the WebAPK's main page.
        if (gotManifest || needsUpgrade) {
            destroyFetcher();
        }

        if (!needsUpgrade) {
            if (!mStorage.didPreviousUpdateSucceed() || mStorage.shouldForceUpdate()) {
                onFinishedUpdate(mStorage, WebApkInstallResult.SUCCESS, false /* relaxUpdates */);
            }
            return;
        }

        // Set WebAPK update as having failed in case that Chrome is killed prior to
        // {@link onBuiltWebApk} being called.
        recordUpdate(mStorage, WebApkInstallResult.FAILURE, false /* relaxUpdates*/);

        if (fetchedInfo != null) {
            buildUpdateRequestAndSchedule(fetchedInfo, primaryIconUrl, badgeIconUrl,
                    false /* isManifestStale */, updateReason);
            return;
        }

        // Tell the server that the our version of the Web Manifest might be stale and to ignore
        // our Web Manifest data if the server's Web Manifest data is newer. This scenario can
        // occur if the Web Manifest is temporarily unreachable.
        buildUpdateRequestAndSchedule(mInfo, "" /* primaryIconUrl */, "" /* badgeIconUrl */,
                true /* isManifestStale */, updateReason);
    }

    /**
     * Builds {@link WebApkUpdateDataFetcher}. In a separate function for the sake of tests.
     */
    protected WebApkUpdateDataFetcher buildFetcher() {
        return new WebApkUpdateDataFetcher();
    }

    /** Builds proto to send to the WebAPK server. */
    private void buildUpdateRequestAndSchedule(WebApkInfo info, String primaryIconUrl,
            String badgeIconUrl, boolean isManifestStale, @WebApkUpdateReason int updateReason) {
        Callback<Boolean> callback = (success) -> {
            if (!success) {
                onFinishedUpdate(mStorage, WebApkInstallResult.FAILURE, false /* relaxUpdates*/);
                return;
            }
            scheduleUpdate();
        };
        String updateRequestPath = mStorage.createAndSetUpdateRequestFilePath(info);
        storeWebApkUpdateRequestToFile(updateRequestPath, info, primaryIconUrl, badgeIconUrl,
                isManifestStale, updateReason, callback);
    }

    /** Schedules update for when WebAPK is not running. */
    private void scheduleUpdate() {
        WebApkUma.recordUpdateRequestQueued(1);
        TaskInfo updateTask;
        if (mStorage.shouldForceUpdate()) {
            // Start an update task ASAP for forced updates.
            updateTask = TaskInfo.createOneOffTask(TaskIds.WEBAPK_UPDATE_JOB_ID,
                                         WebApkUpdateTask.class, 0 /* windowEndTimeMs */)
                                 .setUpdateCurrent(true)
                                 .setIsPersisted(true)
                                 .build();
            mStorage.setUpdateScheduled(true);
            mStorage.setShouldForceUpdate(false);
        } else {
            // The task deadline should be before {@link WebappDataStorage#RETRY_UPDATE_DURATION}
            updateTask =
                    TaskInfo.createOneOffTask(TaskIds.WEBAPK_UPDATE_JOB_ID, WebApkUpdateTask.class,
                                    DateUtils.HOUR_IN_MILLIS, DateUtils.HOUR_IN_MILLIS * 23)
                            .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                            .setUpdateCurrent(true)
                            .setIsPersisted(true)
                            .setRequiresCharging(true)
                            .build();
        }

        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), updateTask);
    }

    /** Sends update request to the WebAPK Server. Should be called when WebAPK is not running. */
    public static void updateWhileNotRunning(
            final WebappDataStorage storage, final Runnable callback) {
        Log.i(TAG, "Update now");
        WebApkUpdateCallback callbackRunner = (result, relaxUpdates) -> {
            onFinishedUpdate(storage, result, relaxUpdates);
            callback.run();
        };

        WebApkUma.recordUpdateRequestSent(WebApkUma.UpdateRequestSent.WHILE_WEBAPK_CLOSED);
        WebApkUpdateManagerJni.get().updateWebApkFromFile(
                storage.getPendingUpdateRequestPath(), callbackRunner);
    }

    /**
     * Destroys {@link mFetcher}. In a separate function for the sake of tests.
     */
    protected void destroyFetcher() {
        if (mFetcher != null) {
            mFetcher.destroy();
            mFetcher = null;
        }
    }

    /**
     * Whether there is a new version of the //chrome/android/webapk/shell_apk code.
     */
    private static boolean isShellApkVersionOutOfDate(WebApkInfo info) {
        return info.shellApkVersion() < WebApkVersion.REQUEST_UPDATE_FOR_SHELL_APK_VERSION;
    }

    /**
     * Returns whether the Web Manifest should be refetched to check whether it has been updated.
     * TODO: Make this method static once there is a static global clock class.
     * @param info Meta data from WebAPK's Android Manifest.
     * True if there has not been any update attempts.
     */
    private boolean shouldCheckIfWebManifestUpdated(WebApkInfo info) {
        if (!sUpdatesEnabled) return false;

        if (CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP)) {
            return true;
        }

        if (!info.webApkPackageName().startsWith(WEBAPK_PACKAGE_PREFIX)) return false;

        if (isShellApkVersionOutOfDate(info)
                && WebApkVersion.REQUEST_UPDATE_FOR_SHELL_APK_VERSION
                        > mStorage.getLastRequestedShellApkVersion()) {
            return true;
        }

        return mStorage.shouldCheckForUpdate();
    }

    /**
     * Updates {@link WebappDataStorage} with the time of the latest WebAPK update and whether the
     * WebAPK update succeeded. Also updates the last requested "shell APK version".
     */
    private static void recordUpdate(
            WebappDataStorage storage, @WebApkInstallResult int result, boolean relaxUpdates) {
        // Update the request time and result together. It prevents getting a correct request time
        // but a result from the previous request.
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(result == WebApkInstallResult.SUCCESS);
        storage.setRelaxedUpdates(relaxUpdates);
        storage.updateLastRequestedShellApkVersion(
                WebApkVersion.REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
    }

    /**
     * Callback for when WebAPK update finishes or succeeds. Unlike {@link #recordUpdate()}
     * cannot be called while update is in progress.
     */
    private static void onFinishedUpdate(
            WebappDataStorage storage, @WebApkInstallResult int result, boolean relaxUpdates) {
        storage.setShouldForceUpdate(false);
        storage.setUpdateScheduled(false);
        recordUpdate(storage, result, relaxUpdates);
        storage.deletePendingUpdateRequestFile();
    }

    /**
     * Checks whether the WebAPK needs to be updated.
     * @param oldInfo        Data extracted from WebAPK manifest.
     * @param fetchedInfo    Fetched data for Web Manifest.
     * @param primaryIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
     *                       suited for use as the launcher icon on this device.
     * @param badgeIconUrl   The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
     *                       suited for use as the badge icon on this device.
     * @return reason that an update is needed or {@link WebApkUpdateReason#NONE} if an update is
     *         not needed.
     */
    private static @WebApkUpdateReason int needsUpdate(WebApkInfo oldInfo, WebApkInfo fetchedInfo,
            String primaryIconUrl, String badgeIconUrl) {
        if (isShellApkVersionOutOfDate(oldInfo)) return WebApkUpdateReason.OLD_SHELL_APK;
        if (fetchedInfo == null) return WebApkUpdateReason.NONE;

        // We should have computed the Murmur2 hashes for the bitmaps at the primary icon URL and
        // the badge icon for {@link fetchedInfo} (but not the other icon URLs.)
        String fetchedPrimaryIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap()
                .get(primaryIconUrl);
        String primaryIconMurmur2Hash = findMurmur2HashForUrlIgnoringFragment(
                oldInfo.iconUrlToMurmur2HashMap(), primaryIconUrl);
        String fetchedBadgeIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap()
                .get(badgeIconUrl);
        String badgeIconMurmur2Hash = findMurmur2HashForUrlIgnoringFragment(
                oldInfo.iconUrlToMurmur2HashMap(), badgeIconUrl);

        if (!TextUtils.equals(primaryIconMurmur2Hash, fetchedPrimaryIconMurmur2Hash)) {
            return WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS;
        } else if (!TextUtils.equals(badgeIconMurmur2Hash, fetchedBadgeIconMurmur2Hash)) {
            return WebApkUpdateReason.BADGE_ICON_HASH_DIFFERS;
        } else if (!UrlUtilities.urlsMatchIgnoringFragments(
                           oldInfo.scopeUrl(), fetchedInfo.scopeUrl())) {
            return WebApkUpdateReason.SCOPE_DIFFERS;
        } else if (!UrlUtilities.urlsMatchIgnoringFragments(
                           oldInfo.manifestStartUrl(), fetchedInfo.manifestStartUrl())) {
            return WebApkUpdateReason.START_URL_DIFFERS;
        } else if (!TextUtils.equals(oldInfo.shortName(), fetchedInfo.shortName())) {
            return WebApkUpdateReason.SHORT_NAME_DIFFERS;
        } else if (!TextUtils.equals(oldInfo.name(), fetchedInfo.name())) {
            return WebApkUpdateReason.NAME_DIFFERS;
        } else if (oldInfo.backgroundColor() != fetchedInfo.backgroundColor()) {
            return WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS;
        } else if (oldInfo.toolbarColor() != fetchedInfo.toolbarColor()) {
            return WebApkUpdateReason.THEME_COLOR_DIFFERS;
        } else if (oldInfo.orientation() != fetchedInfo.orientation()) {
            return WebApkUpdateReason.ORIENTATION_DIFFERS;
        } else if (oldInfo.displayMode() != fetchedInfo.displayMode()) {
            return WebApkUpdateReason.DISPLAY_MODE_DIFFERS;
        } else if (!oldInfo.shareTarget().equals(fetchedInfo.shareTarget())) {
            return WebApkUpdateReason.WEB_SHARE_TARGET_DIFFERS;
        } else if (ShortcutHelper.doesAndroidSupportMaskableIcons()
                && oldInfo.isIconAdaptive() != fetchedInfo.isIconAdaptive()) {
            return WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS;
        }
        return WebApkUpdateReason.NONE;
    }

    /**
     * Returns the Murmur2 hash for entry in {@link iconUrlToMurmur2HashMap} whose canonical
     * representation, ignoring fragments, matches {@link iconUrlToMatch}.
     */
    private static String findMurmur2HashForUrlIgnoringFragment(
            Map<String, String> iconUrlToMurmur2HashMap, String iconUrlToMatch) {
        for (Map.Entry<String, String> entry : iconUrlToMurmur2HashMap.entrySet()) {
            if (UrlUtilities.urlsMatchIgnoringFragments(entry.getKey(), iconUrlToMatch)) {
                return entry.getValue();
            }
        }
        return null;
    }

    protected void storeWebApkUpdateRequestToFile(String updateRequestPath, WebApkInfo info,
            String primaryIconUrl, String badgeIconUrl, boolean isManifestStale,
            @WebApkUpdateReason int updateReason, Callback<Boolean> callback) {
        int versionCode = info.webApkVersionCode();
        int size = info.iconUrlToMurmur2HashMap().size();
        String[] iconUrls = new String[size];
        String[] iconHashes = new String[size];
        int i = 0;
        for (Map.Entry<String, String> entry : info.iconUrlToMurmur2HashMap().entrySet()) {
            iconUrls[i] = entry.getKey();
            String iconHash = entry.getValue();
            iconHashes[i] = (iconHash != null) ? iconHash : "";
            i++;
        }

        WebApkUpdateManagerJni.get().storeWebApkUpdateRequestToFile(updateRequestPath,
                info.manifestStartUrl(), info.scopeUrl(), info.name(), info.shortName(),
                primaryIconUrl, info.icon().bitmap(), info.isIconAdaptive(), badgeIconUrl,
                info.badgeIcon().bitmap(), iconUrls, iconHashes, info.displayMode(),
                info.orientation(), info.toolbarColor(), info.backgroundColor(),
                info.shareTarget().getAction(), info.shareTarget().getParamTitle(),
                info.shareTarget().getParamText(), info.shareTarget().isShareMethodPost(),
                info.shareTarget().isShareEncTypeMultipart(), info.shareTarget().getFileNames(),
                info.shareTarget().getFileAccepts(), info.manifestUrl(), info.webApkPackageName(),
                versionCode, isManifestStale, updateReason, callback);
    }

    @NativeMethods
    interface Natives {
        public void storeWebApkUpdateRequestToFile(String updateRequestPath, String startUrl,
                String scope, String name, String shortName, String primaryIconUrl,
                Bitmap primaryIcon, boolean isPrimaryIconMaskable, String badgeIconUrl,
                Bitmap badgeIcon, String[] iconUrls, String[] iconHashes,
                @WebDisplayMode int displayMode, int orientation, long themeColor,
                long backgroundColor, String shareTargetAction, String shareTargetParamTitle,
                String shareTargetParamText, boolean shareTargetParamIsMethodPost,
                boolean shareTargetParamIsEncTypeMultipart, String[] shareTargetParamFileNames,
                Object[] shareTargetParamAccepts, String manifestUrl, String webApkPackage,
                int webApkVersion, boolean isManifestStale, @WebApkUpdateReason int updateReason,
                Callback<Boolean> callback);
        public void updateWebApkFromFile(String updateRequestPath, WebApkUpdateCallback callback);
    }
}
