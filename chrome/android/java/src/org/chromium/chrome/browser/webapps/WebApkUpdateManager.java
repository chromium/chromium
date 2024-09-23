// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;
import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Handler;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.MergedWebappInfo;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.webapps.WebApkInstallResult;
import org.chromium.components.webapps.WebApkUpdateReason;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * WebApkUpdateManager manages when to check for updates to the WebAPK's Web Manifest, and sends an
 * update request to the WebAPK Server when an update is needed.
 */
@ActivityScope
public class WebApkUpdateManager implements WebApkUpdateDataFetcher.Observer, DestroyObserver {
    private static final String TAG = "WebApkUpdateManager";

    // Maximum wait time for WebAPK update to be scheduled.
    private static final long UPDATE_TIMEOUT_MILLISECONDS = DateUtils.SECOND_IN_MILLIS * 30;

    // Number of milliseconds that a WebAPK shell is outdated from last
    // install/update and should be updated again.
    @VisibleForTesting
    static final long OLD_SHELL_NEEDS_UPDATE_INTERVAL = DateUtils.DAY_IN_MILLIS * 360;

    // Flags for AppIdentity Update dialog histograms.
    private static final int CHANGING_NOTHING = 0;
    private static final int CHANGING_ICON = 1 << 0;
    private static final int CHANGING_ICON_MASK = 1 << 1;
    private static final int CHANGING_APP_NAME = 1 << 2;
    private static final int CHANGING_SHORTNAME = 1 << 3;
    private static final int CHANGING_ICON_BELOW_THRESHOLD = 1 << 4;
    private static final int CHANGING_ICON_SHELL_UPDATE = 1 << 5;
    private static final int HISTOGRAM_SCOPE = 1 << 6;

    private static final int WEB_APK_ICON_UPDATE_BLOCKED_AT_PERCENTAGE = 11;

    private static final String PARAM_SHELL_VERSION = "shell_version";

    private final ActivityTabProvider mTabProvider;

    /** Whether updates are enabled. Some tests disable updates. */
    private static boolean sUpdatesDisabledForTesting;

    /** The icon change threshold while testing updates. */
    private static Integer sIconThresholdForTesting;

    /** The activity context to use. */
    private Context mContext;

    /** The minimum shell version the WebAPK needs to be using. */
    private static int sWebApkTargetShellVersion;

    /** Data extracted from the WebAPK's launch intent and from the WebAPK's Android Manifest. */
    private WebappInfo mInfo;

    /** The updated manifest information. */
    private MergedWebappInfo mFetchedInfo;

    /** The URL of the primary icon. */
    private String mFetchedPrimaryIconUrl;

    /** The URL of the splash icon. */
    private String mFetchedSplashIconUrl;

    /** The list of reasons why an update was triggered. */
    private @WebApkUpdateReason List<Integer> mUpdateReasons;

    /** The WebappDataStorage with cached data about prior update requests. */
    private WebappDataStorage mStorage;

    private WebApkUpdateDataFetcher mFetcher;

    /** Runs failure callback if WebAPK update is not scheduled within deadline. */
    private Handler mUpdateFailureHandler;

    /** Called with update result. */
    public static interface WebApkUpdateCallback {
        @CalledByNative("WebApkUpdateCallback")
        public void onResultFromNative(@WebApkInstallResult int result, boolean relaxUpdates);
    }

    @Inject
    public WebApkUpdateManager(
            @Named(ACTIVITY_CONTEXT) Context context,
            ActivityTabProvider tabProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mContext = context;
        mTabProvider = tabProvider;
        lifecycleDispatcher.register(this);
    }

    /**
     * Checks whether the WebAPK's Web Manifest has changed. Requests an updated WebAPK if the Web
     * Manifest has changed. Skips the check if the check was done recently.
     */
    public void updateIfNeeded(
            WebappDataStorage storage, BrowserServicesIntentDataProvider intentDataProvider) {
        mStorage = storage;
        mInfo = WebappInfo.create(intentDataProvider);

        Tab tab = mTabProvider.get();

        if (tab == null || !shouldCheckIfWebManifestUpdated(mInfo)) return;

        mFetcher = buildFetcher();
        mFetcher.start(tab, mInfo, this);
        mUpdateFailureHandler = new Handler();
        mUpdateFailureHandler.postDelayed(
                new Runnable() {
                    @Override
                    public void run() {
                        onGotManifestData(null, null, null);
                    }
                },
                updateTimeoutMilliseconds());
    }

    @Override
    public void onDestroy() {
        destroyFetcher();
        if (mUpdateFailureHandler != null) {
            mUpdateFailureHandler.removeCallbacksAndMessages(null);
        }
    }

    public static void setUpdatesDisabledForTesting(boolean value) {
        sUpdatesDisabledForTesting = value;
        ResettersForTesting.register(() -> sUpdatesDisabledForTesting = false);
    }

    public static void setIconThresholdForTesting(int percentage) {
        sIconThresholdForTesting = percentage;
        ResettersForTesting.register(
                () -> sIconThresholdForTesting = WEB_APK_ICON_UPDATE_BLOCKED_AT_PERCENTAGE);
    }

    /**
     * Calculates how different two colors are, by comparing how far apart each of the RGBA channels
     * are (in terms of percentages) and averaging that across all channels.
     */
    static float colorDiff(int color1, int color2) {
        return (Math.abs(Color.red(color1) - Color.red(color2)) / 255f
                        + Math.abs(Color.green(color1) - Color.green(color2)) / 255f
                        + Math.abs(Color.blue(color1) - Color.blue(color2)) / 255f
                        + Math.abs(Color.alpha(color1) - Color.alpha(color2)) / 255f)
                / 4f;
    }

    /**
     * Calculates a single percentage number for how different the two given bitmaps are, as
     * calculated by comparing the color of each pixel. Returns 100 (percent) if either a) the
     * pictures are not the same dimensions or color configuration or b) one bitmap is null and the
     * other is a valid image.
     */
    static int imageDiffValue(@Nullable Bitmap before, @Nullable Bitmap after) {
        if (before == null || after == null) {
            return before == after ? 0 : 100;
        }

        assert before.getWidth() == after.getWidth() && before.getHeight() == after.getHeight();

        if (before.getConfig() != after.getConfig()) {
            return 100;
        }

        float difference = 0;
        for (int y = 0; y < before.getHeight(); ++y) {
            for (int x = 0; x < before.getWidth(); ++x) {
                difference += colorDiff(before.getPixel(x, y), after.getPixel(x, y));
            }
        }

        return (int) Math.floor(100f * difference / (before.getHeight() * before.getWidth()));
    }

    /**
     * Logs how different (in percentages) two bitmaps are, scaling the images down to be the same
     * size (if the two are of different dimensions). Does nothing if either bitmap passed in is
     * null (but it only happens during testing).
     *
     * @param before The current Bitmap of the web app.
     * @param after The new (proposed) Bitmap for the web app.
     * @return the percentage difference of the two Bitmaps. Note that a floor function is used when
     *     rounding, so a return value of 1 means there was a change between 1% and 2%, inclusive
     *     and exclusive (respectively).
     */
    static int logIconDiffs(Bitmap before, Bitmap after) {
        // The icons may be null during unit testing.
        if (before == null || after == null) return Integer.MAX_VALUE;

        // Unfortunately, the install size can differ from the update size (for example, a 96x96
        // installed icon is downscaled to size 72x72, during update -- even if the website provides
        // a 96x96 icon replacement, as per InstallableManager::GetIdealPrimaryIconSizeInPx()).
        boolean scaled = false;
        if (before.getWidth() < after.getWidth() || before.getHeight() < after.getHeight()) {
            after = Bitmap.createScaledBitmap(after, before.getWidth(), before.getHeight(), false);
            scaled = true;
        } else if (before.getWidth() > after.getWidth() || before.getHeight() > after.getHeight()) {
            before = Bitmap.createScaledBitmap(before, after.getWidth(), after.getHeight(), false);
            scaled = true;
        }

        int diffValue = imageDiffValue(before, after);

        if (scaled) {
            RecordHistogram.recordCount100Histogram(
                    "WebApk.AppIdentityDialog.PendingImageUpdateDiffValueScaled", diffValue);
        } else {
            RecordHistogram.recordCount100Histogram(
                    "WebApk.AppIdentityDialog.PendingImageUpdateDiffValue", diffValue);
        }

        return diffValue;
    }

    @Override
    public void onGotManifestData(
            BrowserServicesIntentDataProvider fetchedIntentDataProvider,
            String primaryIconUrl,
            String splashIconUrl) {
        mFetchedPrimaryIconUrl = primaryIconUrl;
        mFetchedSplashIconUrl = splashIconUrl;
        mFetchedInfo =
                MergedWebappInfo.create(/* oldWebappInfo= */ mInfo, fetchedIntentDataProvider);

        mUpdateReasons =
                generateUpdateReasons(
                        mInfo,
                        mFetchedInfo,
                        mFetchedPrimaryIconUrl,
                        mFetchedSplashIconUrl,
                        iconUpdateDialogEnabled(),
                        nameUpdateDialogEnabled());

        if (mFetchedInfo != null) {
            // When only some/no app identity updates are permitted, the update
            // should still proceed with updating all the other values (not related
            // to the blocked app identity update).
            if (!mUpdateReasons.contains(WebApkUpdateReason.NAME_DIFFERS)
                    && !mUpdateReasons.contains(WebApkUpdateReason.SHORT_NAME_DIFFERS)) {
                mFetchedInfo.setUseOldName(true);
            }

            if (!mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS)) {
                mFetchedInfo.setUseOldIcon(true);
                // Forces recreation of the primary icon during proto construction.
                mFetchedPrimaryIconUrl = "";
            }
        }

        mStorage.updateTimeOfLastCheckForUpdatedWebManifest();
        if (mUpdateFailureHandler != null) {
            mUpdateFailureHandler.removeCallbacksAndMessages(null);
        }

        boolean gotManifest = (mFetchedInfo != null);
        boolean needsUpgrade = !mUpdateReasons.isEmpty();
        if (mStorage.shouldForceUpdate() && needsUpgrade) {
            // Add to the front of the list to designate it as the primary reason.
            mUpdateReasons.add(0, WebApkUpdateReason.MANUALLY_TRIGGERED);
        }
        Log.i(TAG, "Got Manifest: " + gotManifest);
        Log.i(TAG, "WebAPK upgrade needed: " + needsUpgrade);
        Log.i(TAG, "Upgrade reasons: " + Arrays.toString(mUpdateReasons.toArray()));

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

        if (TextUtils.isEmpty(mInfo.manifestId())) {
            RecordHistogram.recordBooleanHistogram(
                    "WebApk.Update.UpdateEmptyUniqueId.NeedsUpgrade", needsUpgrade);
        }

        if (!needsUpgrade) {
            if (!mStorage.didPreviousUpdateSucceed() || mStorage.shouldForceUpdate()) {
                onFinishedUpdate(mStorage, WebApkInstallResult.SUCCESS, /* relaxUpdates= */ false);
            }
            return;
        }

        boolean iconChanging =
                mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS)
                        || mUpdateReasons.contains(
                                WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS);
        boolean shellUpdateInProgress =
                mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_CHANGE_SHELL_UPDATE);
        boolean iconChangeBelowThreshold =
                mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_CHANGE_BELOW_THRESHOLD);
        boolean shortNameChanging = mUpdateReasons.contains(WebApkUpdateReason.SHORT_NAME_DIFFERS);
        boolean nameChanging = mUpdateReasons.contains(WebApkUpdateReason.NAME_DIFFERS);

        int histogramAction = CHANGING_NOTHING;
        if (mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS)) {
            histogramAction |= CHANGING_ICON;
        }
        if (mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_CHANGE_BELOW_THRESHOLD)) {
            histogramAction |= CHANGING_ICON_BELOW_THRESHOLD;
        }
        if (mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_CHANGE_SHELL_UPDATE)) {
            histogramAction |= CHANGING_ICON_SHELL_UPDATE;
        }
        if (mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS)) {
            histogramAction |= CHANGING_ICON_MASK;
        }
        if (nameChanging) histogramAction |= CHANGING_APP_NAME;
        if (shortNameChanging) histogramAction |= CHANGING_SHORTNAME;

        // Use the original `primaryIconUrl` (as opposed to `mFetchedPrimaryIconUrl`) to construct
        // the hash, which ensures that we don't regress on issue crbug.com/1287447.
        String hash = getAppIdentityHash(mFetchedInfo, primaryIconUrl);
        boolean alreadyUserApproved =
                !hash.isEmpty()
                        && TextUtils.equals(hash, mStorage.getLastWebApkUpdateHashAccepted());
        boolean showDialogForName =
                (nameChanging || shortNameChanging) && nameUpdateDialogEnabled();
        boolean showDialogForIcon =
                iconChanging && !iconChangeBelowThreshold && !shellUpdateInProgress;

        if ((showDialogForName || showDialogForIcon) && !alreadyUserApproved) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Webapp.AppIdentityDialog.Showing", histogramAction, HISTOGRAM_SCOPE);
            showIconOrNameUpdateDialog(iconChanging, shortNameChanging, nameChanging);
            return;
        }

        if (alreadyUserApproved) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Webapp.AppIdentityDialog.AlreadyApproved", histogramAction, HISTOGRAM_SCOPE);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Webapp.AppIdentityDialog.NotShowing", histogramAction, HISTOGRAM_SCOPE);
        }

        // It might seem non-obvious why the POSITIVE button is selected when we've determined
        // that App Identity updates are not enabled or when nothing meaningful is changing.
        // Keep in mind, though, that the update being processed might not be app identity
        // related and those updates must still go through. The server keeps track of whether an
        // identity update has been approved by the user, using the `appIdentityUpdateSupported`
        // flag, so the app won't be able to update its identity even if we use the POSITIVE
        // button here.
        onUserApprovedUpdate(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    protected boolean iconUpdateDialogEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_UPDATE_DIALOG_FOR_ICON);
    }

    protected boolean nameUpdateDialogEnabled() {
        // TODO(finnur): Remove this function when future of the icon flag is clear.
        return true;
    }

    private static boolean allowIconUpdateForShellVersion(int shellVersion) {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATE, PARAM_SHELL_VERSION, 0)
                >= shellVersion;
    }

    /**
     * @return whether a percentage change is below the threshold allowed for icon updates. Note
     *     that percentage values are rounded using a floor function, so a `percentage` change of 1
     *     means it is somewhere between 1% and 2%, inclusive and exclusive (respectively). This
     *     means that the threshold updates need to be set to -1 to disable them (0 would allow
     *     changes up to 1%).
     */
    private static boolean belowAppIdIconUpdateThreshold(int percentage) {
        // See also go/app-id-update-threshold/ for (internal) discussion.
        int threshold =
                sIconThresholdForTesting != null
                        ? sIconThresholdForTesting
                        : WEB_APK_ICON_UPDATE_BLOCKED_AT_PERCENTAGE;
        return percentage < threshold;
    }

    protected void showIconOrNameUpdateDialog(
            boolean iconChanging, boolean shortNameChanging, boolean nameChanging) {
        // Show the dialog to confirm name and/or icon update.
        ModalDialogManager dialogManager =
                mTabProvider.get().getWindowAndroid().getModalDialogManager();
        WebApkIconNameUpdateDialog dialog = new WebApkIconNameUpdateDialog();
        dialog.show(
                mContext,
                dialogManager,
                mInfo.webApkPackageName(),
                iconChanging,
                shortNameChanging,
                nameChanging,
                mInfo.shortName(),
                mFetchedInfo.shortName(),
                mInfo.name(),
                mFetchedInfo.name(),
                mInfo.icon().bitmap(),
                mFetchedInfo.icon().bitmap(),
                mInfo.isIconAdaptive(),
                mFetchedInfo.isIconAdaptive(),
                this::onUserApprovedUpdate);
    }

    private String getAppIdentityHash(WebappInfo info, String primaryIconUrl) {
        if (info == null) {
            return "";
        }
        return info.name()
                + "|"
                + info.shortName()
                + "|"
                + info.iconUrlToMurmur2HashMap().get(primaryIconUrl)
                + (info.isIconAdaptive() ? "|Adaptive" : "|NotAdaptive");
    }

    protected void onUserApprovedUpdate(int dismissalCause) {
        // Set WebAPK update as having failed in case that Chrome is killed prior to
        // {@link onBuiltWebApk} being called.
        recordUpdate(mStorage, WebApkInstallResult.FAILURE, /* relaxUpdates= */ false);

        // Continue if the user explicitly allows the update using the button, or isn't interested
        // in the update dialog warning (presses Back). Otherwise, they can be left in a state where
        // they always press Back and are stuck on an old version of the app forever.
        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                && dismissalCause != DialogDismissalCause.NAVIGATE_BACK
                && dismissalCause != DialogDismissalCause.TOUCH_OUTSIDE) {
            return;
        }

        String hash = getAppIdentityHash(mFetchedInfo, mFetchedPrimaryIconUrl);
        if (!hash.isEmpty()) mStorage.updateLastWebApkUpdateHashAccepted(hash);

        if (mFetchedInfo != null) {
            buildUpdateRequestAndSchedule(
                    mFetchedInfo,
                    mFetchedPrimaryIconUrl,
                    mFetchedSplashIconUrl,
                    /* isManifestStale= */ false,
                    /* appIdentityUpdateSupported= */ nameUpdateDialogEnabled()
                            || iconUpdateDialogEnabled(),
                    mUpdateReasons);
            return;
        }

        // Tell the server that the our version of the Web Manifest might be stale and to ignore
        // our Web Manifest data if the server's Web Manifest data is newer. This scenario can
        // occur if the Web Manifest is temporarily unreachable.
        buildUpdateRequestAndSchedule(
                mInfo,
                /* primaryIconUrl= */ "",
                /* splashIconUrl= */ "",
                /* isManifestStale= */ true,
                /* appIdentityUpdateSupported= */ nameUpdateDialogEnabled()
                        || iconUpdateDialogEnabled(),
                mUpdateReasons);
    }

    /** Builds {@link WebApkUpdateDataFetcher}. In a separate function for the sake of tests. */
    @VisibleForTesting
    protected WebApkUpdateDataFetcher buildFetcher() {
        return new WebApkUpdateDataFetcher();
    }

    @VisibleForTesting
    protected long updateTimeoutMilliseconds() {
        return UPDATE_TIMEOUT_MILLISECONDS;
    }

    /** Builds proto to send to the WebAPK server. */
    private void buildUpdateRequestAndSchedule(
            WebappInfo info,
            String primaryIconUrl,
            String splashIconUrl,
            boolean isManifestStale,
            boolean appIdentityUpdateSupported,
            List<Integer> updateReasons) {
        Callback<Boolean> callback =
                (success) -> {
                    if (!success) {
                        onFinishedUpdate(
                                mStorage, WebApkInstallResult.FAILURE, /* relaxUpdates= */ false);
                        return;
                    }
                    scheduleUpdate(info.shellApkVersion());
                };
        String updateRequestPath = mStorage.createAndSetUpdateRequestFilePath(info);
        encodeIconsInBackground(
                updateRequestPath,
                info,
                primaryIconUrl,
                splashIconUrl,
                isManifestStale,
                appIdentityUpdateSupported,
                updateReasons,
                callback);
    }

    /** Schedules update for when WebAPK is not running. */
    @VisibleForTesting
    protected void scheduleUpdate(int shellApkVersion) {
        WebApkUmaRecorder.recordQueuedUpdateShellVersion(shellApkVersion);
        TaskInfo updateTask;
        if (mStorage.shouldForceUpdate()) {
            // Start an update task ASAP for forced updates.
            updateTask =
                    TaskInfo.createOneOffTask(
                                    TaskIds.WEBAPK_UPDATE_JOB_ID, /* windowEndTimeMs= */ 0)
                            .setUpdateCurrent(true)
                            .setIsPersisted(true)
                            .build();
            mStorage.setUpdateScheduled(true);
            mStorage.setShouldForceUpdate(false);
        } else {
            // The task deadline should be before {@link WebappDataStorage#RETRY_UPDATE_DURATION}
            updateTask =
                    TaskInfo.createOneOffTask(
                                    TaskIds.WEBAPK_UPDATE_JOB_ID,
                                    DateUtils.HOUR_IN_MILLIS,
                                    DateUtils.HOUR_IN_MILLIS * 23)
                            .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                            .setUpdateCurrent(true)
                            .setIsPersisted(true)
                            .setRequiresCharging(true)
                            .build();
        }

        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), updateTask);
    }

    /** Sends update request to the WebAPK Server. Should be called when WebAPK is not running. */
    public static void updateWhileNotRunning(
            final WebappDataStorage storage, final Runnable callback) {
        Log.i(TAG, "Update now");
        WebApkUpdateCallback callbackRunner =
                (result, relaxUpdates) -> {
                    onFinishedUpdate(storage, result, relaxUpdates);
                    callback.run();
                };

        WebApkUmaRecorder.recordUpdateRequestSent(
                WebApkUmaRecorder.UpdateRequestSent.WHILE_WEBAPK_CLOSED);
        WebApkUpdateManagerJni.get()
                .updateWebApkFromFile(storage.getPendingUpdateRequestPath(), callbackRunner);
    }

    /** Destroys {@link mFetcher}. In a separate function for the sake of tests. */
    protected void destroyFetcher() {
        if (mFetcher != null) {
            mFetcher.destroy();
            mFetcher = null;
        }
    }

    private static int webApkTargetShellVersion() {
        if (sWebApkTargetShellVersion == 0) {
            sWebApkTargetShellVersion = WebApkUpdateManagerJni.get().getWebApkTargetShellVersion();
        }
        return sWebApkTargetShellVersion;
    }

    /** Whether the shell version is outdated. */
    private static boolean isShellApkVersionOutOfDate(WebappInfo info) {
        return info.shellApkVersion() < webApkTargetShellVersion()
                || (info.lastUpdateTime() > 0
                        && TimeUtils.currentTimeMillis() - info.lastUpdateTime()
                                > OLD_SHELL_NEEDS_UPDATE_INTERVAL);
    }

    /**
     * Returns whether the Web Manifest should be refetched to check whether it has been updated.
     * TODO: Make this method static once there is a static global clock class.
     *
     * @param info Meta data from WebAPK's Android Manifest. True if there has not been any update
     *     attempts.
     */
    private boolean shouldCheckIfWebManifestUpdated(WebappInfo info) {
        if (sUpdatesDisabledForTesting) return false;

        if (CommandLine.getInstance()
                .hasSwitch(ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP)) {
            return true;
        }

        if (!info.webApkPackageName().startsWith(WEBAPK_PACKAGE_PREFIX)) return false;

        if (isShellApkVersionOutOfDate(info)
                && webApkTargetShellVersion() > mStorage.getLastRequestedShellApkVersion()) {
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
        storage.updateLastRequestedShellApkVersion(webApkTargetShellVersion());
    }

    /**
     * Callback for when WebAPK update finishes or succeeds. Unlike {@link #recordUpdate()} cannot
     * be called while update is in progress.
     */
    private static void onFinishedUpdate(
            WebappDataStorage storage, @WebApkInstallResult int result, boolean relaxUpdates) {
        storage.setShouldForceUpdate(false);
        storage.setUpdateScheduled(false);
        recordUpdate(storage, result, relaxUpdates);
        storage.deletePendingUpdateRequestFile();
    }

    private static boolean shortcutsDiffer(
            List<WebApkExtras.ShortcutItem> oldShortcuts,
            List<WebApkExtras.ShortcutItem> fetchedShortcuts) {
        assert oldShortcuts != null;
        assert fetchedShortcuts != null;

        if (fetchedShortcuts.size() != oldShortcuts.size()) {
            return true;
        }

        for (int i = 0; i < oldShortcuts.size(); i++) {
            if (!TextUtils.equals(oldShortcuts.get(i).name, fetchedShortcuts.get(i).name)
                    || !TextUtils.equals(
                            oldShortcuts.get(i).shortName, fetchedShortcuts.get(i).shortName)
                    || !TextUtils.equals(
                            oldShortcuts.get(i).launchUrl, fetchedShortcuts.get(i).launchUrl)
                    || !TextUtils.equals(
                            oldShortcuts.get(i).iconHash, fetchedShortcuts.get(i).iconHash)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Returns a list of reasons why the WebAPK needs to be updated.
     *
     * @param oldInfo Data extracted from WebAPK manifest.
     * @param fetchedInfo Fetched data for Web Manifest.
     * @param primaryIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
     *     suited for use as the launcher icon on this device.
     * @param splashIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
     *     suited for use as the splash icon on this device.
     * @param iconUpdateDialogEnabled Whether the App Identity Dialog is enabled for icons.
     * @param nameUpdateDialogEnabled Whether the App Identity Dialog is enabled for names.
     * @return reasons that an update is needed or an empty list if an update is not needed.
     */
    private static List<Integer> generateUpdateReasons(
            WebappInfo oldInfo,
            WebappInfo fetchedInfo,
            String primaryIconUrl,
            String splashIconUrl,
            boolean iconUpdateDialogEnabled,
            boolean nameUpdateDialogEnabled) {
        List<Integer> updateReasons = new ArrayList<>();

        if (isShellApkVersionOutOfDate(oldInfo)) {
            updateReasons.add(WebApkUpdateReason.OLD_SHELL_APK);
        }
        if (fetchedInfo == null) {
            return updateReasons;
        }

        // We should have computed the Murmur2 hashes for the bitmaps at the primary icon URL and
        // the splash icon for {@link fetchedInfo} (but not the other icon URLs.)
        String fetchedPrimaryIconMurmur2Hash =
                fetchedInfo.iconUrlToMurmur2HashMap().get(primaryIconUrl);
        String primaryIconMurmur2Hash =
                findMurmur2HashForUrlIgnoringFragment(
                        oldInfo.iconUrlToMurmur2HashMap(), primaryIconUrl);
        String fetchedSplashIconMurmur2Hash =
                fetchedInfo.iconUrlToMurmur2HashMap().get(splashIconUrl);
        String splashIconMurmur2Hash =
                findMurmur2HashForUrlIgnoringFragment(
                        oldInfo.iconUrlToMurmur2HashMap(), splashIconUrl);

        if (!TextUtils.equals(primaryIconMurmur2Hash, fetchedPrimaryIconMurmur2Hash)) {
            boolean shouldUpdateIcon = false; // Determined below.

            int iconDiffValue = logIconDiffs(oldInfo.icon().bitmap(), fetchedInfo.icon().bitmap());
            if (belowAppIdIconUpdateThreshold(iconDiffValue)) {
                shouldUpdateIcon = true;
                updateReasons.add(WebApkUpdateReason.PRIMARY_ICON_CHANGE_BELOW_THRESHOLD);
            }

            if (allowIconUpdateForShellVersion(oldInfo.shellApkVersion())) {
                updateReasons.add(WebApkUpdateReason.PRIMARY_ICON_CHANGE_SHELL_UPDATE);
                shouldUpdateIcon = true;
            }

            if (iconUpdateDialogEnabled) {
                shouldUpdateIcon = true;
            }

            if (shouldUpdateIcon) {
                updateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);

                if (!TextUtils.equals(splashIconMurmur2Hash, fetchedSplashIconMurmur2Hash)) {
                    updateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
                }
            }
        }
        if (!UrlUtilities.urlsMatchIgnoringFragments(oldInfo.scopeUrl(), fetchedInfo.scopeUrl())) {
            updateReasons.add(WebApkUpdateReason.SCOPE_DIFFERS);
        }
        if (!UrlUtilities.urlsMatchIgnoringFragments(
                oldInfo.manifestStartUrl(), fetchedInfo.manifestStartUrl())) {
            updateReasons.add(WebApkUpdateReason.START_URL_DIFFERS);
        }
        if (nameUpdateDialogEnabled) {
            if (!TextUtils.equals(oldInfo.shortName(), fetchedInfo.shortName())) {
                updateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
            }
            if (!TextUtils.equals(oldInfo.name(), fetchedInfo.name())) {
                updateReasons.add(WebApkUpdateReason.NAME_DIFFERS);
            }
        }
        if (oldInfo.backgroundColor() != fetchedInfo.backgroundColor()) {
            updateReasons.add(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
        }
        if (oldInfo.toolbarColor() != fetchedInfo.toolbarColor()) {
            updateReasons.add(WebApkUpdateReason.THEME_COLOR_DIFFERS);
        }
        if (oldInfo.darkBackgroundColor() != fetchedInfo.darkBackgroundColor()) {
            updateReasons.add(WebApkUpdateReason.DARK_BACKGROUND_COLOR_DIFFERS);
        }
        if (oldInfo.darkToolbarColor() != fetchedInfo.darkToolbarColor()) {
            updateReasons.add(WebApkUpdateReason.DARK_THEME_COLOR_DIFFERS);
        }
        if (oldInfo.orientation() != fetchedInfo.orientation()) {
            updateReasons.add(WebApkUpdateReason.ORIENTATION_DIFFERS);
        }
        if (oldInfo.displayMode() != fetchedInfo.displayMode()) {
            updateReasons.add(WebApkUpdateReason.DISPLAY_MODE_DIFFERS);
        }
        if (!WebApkShareTarget.equals(oldInfo.shareTarget(), fetchedInfo.shareTarget())) {
            updateReasons.add(WebApkUpdateReason.WEB_SHARE_TARGET_DIFFERS);
        }
        if (oldInfo.isIconAdaptive() != fetchedInfo.isIconAdaptive()
                && (!fetchedInfo.isIconAdaptive()
                        || WebappsIconUtils.doesAndroidSupportMaskableIcons())) {
            updateReasons.add(WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS);
        }
        if (shortcutsDiffer(oldInfo.shortcutItems(), fetchedInfo.shortcutItems())) {
            updateReasons.add(WebApkUpdateReason.SHORTCUTS_DIFFER);
        }
        return updateReasons;
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

    // Encode the icons in a background process since encoding is expensive.
    protected void encodeIconsInBackground(
            String updateRequestPath,
            WebappInfo info,
            String primaryIconUrl,
            String splashIconUrl,
            boolean isManifestStale,
            boolean isAppIdentityUpdateSupported,
            List<Integer> updateReasons,
            Callback<Boolean> callback) {
        new AsyncTask<Pair<byte[], byte[]>>() {
            @Override
            protected Pair<byte[], byte[]> doInBackground() {
                byte[] primaryIconData = info.icon().data();
                byte[] splashIconData = info.splashIcon().data();
                return Pair.create(primaryIconData, splashIconData);
            }

            @Override
            protected void onPostExecute(Pair<byte[], byte[]> result) {
                storeWebApkUpdateRequestToFile(
                        updateRequestPath,
                        info,
                        primaryIconUrl,
                        result.first,
                        splashIconUrl,
                        result.second,
                        isManifestStale,
                        isAppIdentityUpdateSupported,
                        updateReasons,
                        callback);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    protected void storeWebApkUpdateRequestToFile(
            String updateRequestPath,
            WebappInfo info,
            String primaryIconUrl,
            byte[] primaryIconData,
            String splashIconUrl,
            byte[] splashIconData,
            boolean isManifestStale,
            boolean isAppIdentityUpdateSupported,
            List<Integer> updateReasons,
            Callback<Boolean> callback) {
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

        String[][] shortcuts = new String[info.shortcutItems().size()][];
        byte[][] shortcutIconData = new byte[info.shortcutItems().size()][];
        for (int j = 0; j < info.shortcutItems().size(); j++) {
            WebApkExtras.ShortcutItem shortcut = info.shortcutItems().get(j);
            shortcuts[j] =
                    new String[] {
                        shortcut.name,
                        shortcut.shortName,
                        shortcut.launchUrl,
                        shortcut.iconUrl,
                        shortcut.iconHash
                    };
            shortcutIconData[j] = shortcut.icon.data();
        }

        String shareTargetAction = "";
        String shareTargetParamTitle = "";
        String shareTargetParamText = "";
        boolean shareTargetIsMethodPost = false;
        boolean shareTargetIsEncTypeMultipart = false;
        String[] shareTargetParamFileNames = new String[0];
        String[][] shareTargetParamAccepts = new String[0][];
        WebApkShareTarget shareTarget = info.shareTarget();
        if (shareTarget != null) {
            shareTargetAction = shareTarget.getAction();
            shareTargetParamTitle = shareTarget.getParamTitle();
            shareTargetParamText = shareTarget.getParamText();
            shareTargetIsMethodPost = shareTarget.isShareMethodPost();
            shareTargetIsEncTypeMultipart = shareTarget.isShareEncTypeMultipart();
            shareTargetParamFileNames = shareTarget.getFileNames();
            shareTargetParamAccepts = shareTarget.getFileAccepts();
        }

        int[] updateReasonsArray = new int[updateReasons.size()];
        for (int j = 0; j < updateReasons.size(); j++) {
            updateReasonsArray[j] = updateReasons.get(j);
        }

        WebApkUpdateManagerJni.get()
                .storeWebApkUpdateRequestToFile(
                        updateRequestPath,
                        info.manifestStartUrl(),
                        info.scopeUrl(),
                        info.name(),
                        info.shortName(),
                        info.hasCustomName(),
                        info.manifestIdWithFallback(),
                        info.appKey(),
                        primaryIconUrl,
                        primaryIconData,
                        info.isIconAdaptive(),
                        splashIconUrl,
                        splashIconData,
                        info.isSplashIconMaskable(),
                        iconUrls,
                        iconHashes,
                        info.displayMode(),
                        info.orientation(),
                        info.toolbarColor(),
                        info.backgroundColor(),
                        info.darkToolbarColor(),
                        info.darkBackgroundColor(),
                        shareTargetAction,
                        shareTargetParamTitle,
                        shareTargetParamText,
                        shareTargetIsMethodPost,
                        shareTargetIsEncTypeMultipart,
                        shareTargetParamFileNames,
                        shareTargetParamAccepts,
                        shortcuts,
                        shortcutIconData,
                        info.manifestUrl(),
                        info.webApkPackageName(),
                        versionCode,
                        isManifestStale,
                        isAppIdentityUpdateSupported,
                        updateReasonsArray,
                        callback);
    }

    @NativeMethods
    interface Natives {
        public void storeWebApkUpdateRequestToFile(
                @JniType("std::string") String updateRequestPath,
                @JniType("std::string") String startUrl,
                @JniType("std::string") String scope,
                @JniType("std::u16string") String name,
                @JniType("std::u16string") String shortName,
                boolean hasCustomName,
                @JniType("std::string") String manifestId,
                @JniType("std::string") String appKey,
                @JniType("std::string") String primaryIconUrl,
                byte[] primaryIconData,
                boolean isPrimaryIconMaskable,
                @JniType("std::string") String splashIconUrl,
                byte[] splashIconData,
                boolean isSplashIconMaskable,
                @JniType("std::vector<std::string>") String[] iconUrls,
                @JniType("std::vector<std::string>") String[] iconHashes,
                @DisplayMode.EnumType int displayMode,
                int orientation,
                long themeColor,
                long backgroundColor,
                long darkThemeColor,
                long darkBackgroundColor,
                @JniType("std::string") String shareTargetAction,
                @JniType("std::u16string") String shareTargetParamTitle,
                @JniType("std::u16string") String shareTargetParamText,
                boolean shareTargetParamIsMethodPost,
                boolean shareTargetParamIsEncTypeMultipart,
                @JniType("std::vector<std::u16string>") String[] shareTargetParamFileNames,
                Object[] shareTargetParamAccepts,
                String[][] shortcuts,
                byte[][] shortcutIconData,
                @JniType("std::string") String manifestUrl,
                @JniType("std::string") String webApkPackage,
                int webApkVersion,
                boolean isManifestStale,
                boolean isAppIdentityUpdateSupported,
                int[] updateReasons,
                Callback<Boolean> callback);

        public void updateWebApkFromFile(String updateRequestPath, WebApkUpdateCallback callback);

        public int getWebApkTargetShellVersion();
    }
}
