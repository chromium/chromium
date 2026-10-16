// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.content.IntentSender;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.appupdate.AppUpdateManagerFactory;
import com.google.android.play.core.appupdate.AppUpdateOptions;
import com.google.android.play.core.install.InstallStateUpdatedListener;
import com.google.android.play.core.install.model.ActivityResult;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.install.model.InstallStatus;
import com.google.android.play.core.install.model.UpdateAvailability;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/**
 * Controller orchestrating Google Play In-App Updates (Play Core) for Chrome on Android.
 *
 * <p>Chrome integrates the Google Play Core In-App Updates SDK ({@code
 * com.google.android.play:app-update}) using strictly the non-blocking Flexible update flow ({@link
 * AppUpdateType#FLEXIBLE}) to mitigate the mobile patch gap caused by background auto-update gating
 * (AC charging, device idle, unmetered Wi-Fi). Update downloads occur in the background while the
 * user continues browsing. The blocking Immediate flow ({@link AppUpdateType#IMMEDIATE}) is
 * deliberately not implemented.
 *
 * <p><b>Architecture &amp; Decoupling:</b> This controller encapsulates Play Core session
 * management, UI presentation, Activity lifecycle bindings, and listener hygiene. It works in
 * tandem with {@link InAppUpdatePolicy}, a static, dependency-free utility that owns eligibility
 * guards (branded builds, GMS availability, feature flag) and backoff arithmetic. This separation
 * enables hermetic host-side Robolectric testing of policy logic without Play Core dependencies on
 * the classpath.
 *
 * <p><b>State Machines:</b> The system coordinates two machines with distinct lifecycles:
 *
 * <ul>
 *   <li><b>Update Lifecycle (Play Core-owned, durable):</b> Tracked via {@link AppUpdateInfo} and
 *       {@link InstallStateUpdatedListener} ({@link InstallStatus}). Persists across app sessions
 *       and process restarts until an update is completed or canceled.
 *   <li><b>Prompt / UI Machine (Activity-scoped, ephemeral):</b> Managed by this controller via
 *       {@link InAppUpdateSnackbarController}. Reset on configuration changes (e.g. rotation).
 *       Teardown dismissals via {@link #destroy()} suppress telemetry and backoff writes.
 * </ul>
 *
 * <p><b>Entry Points:</b> Driven across three primary entry points:
 *
 * <ol>
 *   <li>Post-startup deferred initialization ({@link #checkAndStartUpdateFlow()}): Queries Play
 *       Core after startup without blocking early launch paths.
 *   <li>Foreground resumption ({@link #onResume()}): Re-queries Play Core on Activity resume to
 *       catch downloads completed while Chrome was backgrounded.
 *   <li>App menu "Update Chrome" tap (handled by {@code UpdateMenuItemHelper} in CL 3b): Allows
 *       explicit user-initiated updates, bypassing discovery throttles.
 * </ol>
 *
 * <p><b>Network &amp; Metered Data Policy:</b> Chrome performs no network inspection of its own.
 * When the user confirms an update, Google Play begins downloading immediately over the active
 * connection (including metered cellular data), delegating network data policies entirely to Play
 * Core.
 *
 * <p>See {@code crbug.com/553918188} for full design specifications and state machine details.
 */
@NullMarked
public final class InAppUpdateController {
    /** Interface for Activities that host and manage an {@link InAppUpdateController}. */
    public interface InAppUpdateManageable {
        /**
         * Returns the {@link InAppUpdateController} for this Activity, or null if the feature is
         * disabled.
         */
        @Nullable InAppUpdateController getInAppUpdateController();
    }

    // Unified logcat tag matching InAppUpdatePolicy for end-to-end tracing during prototype
    // rollout.
    // TODO(crbug.com/553918188): Remove logging once in-app update flow stabilizes.
    public static final String TAG = "InAppUpdateFlow";

    /**
     * Request code passed to Play Core's {@link AppUpdateManager#startUpdateFlowForResult}. The
     * host Activity intercepts this code in its activity result pipeline and forwards the result
     * code to {@link #handleActivityResult(int, int)}.
     */
    public static final int REQUEST_CODE_IN_APP_UPDATE = 9876;

    // UMA Histograms
    @VisibleForTesting
    static final String HISTOGRAM_DOWNLOAD_RESULT = "Android.InAppUpdate.Download.Result";

    @VisibleForTesting
    public static final String HISTOGRAM_APP_MENU_ACTION = "Android.InAppUpdate.AppMenu.Action";

    @VisibleForTesting
    static final String PARAM_RESUME_COOLDOWN_MINUTES = "resume_cooldown_minutes";

    @VisibleForTesting static final int DEFAULT_RESUME_COOLDOWN_MINUTES = 5;

    /** Actions triggered via the App Menu "Update Chrome" item. */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(AppMenuAction)
    @IntDef({
        AppMenuAction.TRIGGER_DOWNLOAD,
        AppMenuAction.TRIGGER_RESTART,
        AppMenuAction.INTENT_FALLBACK,
        AppMenuAction.ALREADY_DOWNLOADING,
        AppMenuAction.RESTART_FAILED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AppMenuAction {
        int TRIGGER_DOWNLOAD = 0;
        int TRIGGER_RESTART = 1;
        int INTENT_FALLBACK = 2;
        int ALREADY_DOWNLOADING = 3;
        int RESTART_FAILED = 4;
        int COUNT = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:InAppUpdateAppMenuAction)

    /**
     * Outcomes of Play Core update download operations recorded to {@link
     * #HISTOGRAM_DOWNLOAD_RESULT}.
     *
     * <p>Values represent scenario-specific milestones and failure causes across the download
     * lifecycle rather than a generic boolean, allowing fine-grained failure attribution without a
     * secondary histogram. Note that {@link #PROMPT_CANCELED} (user declined the Play dialog) and
     * {@link #DOWNLOAD_CANCELED} (canceled after consent was granted) are recorded separately to
     * distinguish stated user intent from system cancellations.
     */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(InAppUpdateDownloadResult)
    @IntDef({
        DownloadResult.FLOW_STARTED,
        DownloadResult.SUCCEEDED,
        DownloadResult.START_FAILED,
        DownloadResult.PLAY_CORE_FAILED,
        DownloadResult.DOWNLOAD_FAILED,
        DownloadResult.PROMPT_CANCELED,
        DownloadResult.DOWNLOAD_CANCELED,
        DownloadResult.SEND_INTENT_FAILED,
        DownloadResult.START_EXCEPTION
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DownloadResult {
        /** In-app update consent dialog successfully displayed by Play Core. */
        int FLOW_STARTED = 0;

        /** Download completed successfully (reached {@link InstallStatus#DOWNLOADED}). */
        int SUCCEEDED = 1;

        /** {@link AppUpdateManager#startUpdateFlowForResult} returned false. */
        int START_FAILED = 2;

        /** Play Core update flow failed with {@link ActivityResult#RESULT_IN_APP_UPDATE_FAILED}. */
        int PLAY_CORE_FAILED = 3;

        /** Download failed after starting (reached {@link InstallStatus#FAILED}). */
        int DOWNLOAD_FAILED = 4;

        /** User dismissed or canceled the Google Play consent dialog (RESULT_CANCELED). */
        int PROMPT_CANCELED = 5;

        /**
         * Download was canceled after consent was given (reached {@link InstallStatus#CANCELED}).
         */
        int DOWNLOAD_CANCELED = 6;

        /**
         * IntentSender threw a {@link IntentSender.SendIntentException} when launching the dialog.
         */
        int SEND_INTENT_FAILED = 7;

        /** Runtime exception occurred when attempting to start the update flow. */
        int START_EXCEPTION = 8;

        int COUNT = 9;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:InAppUpdateDownloadResult)

    /** Injected test fake for {@link AppUpdateManager}. */
    private static @Nullable AppUpdateManager sAppUpdateManagerForTesting;

    /** Host Activity context for Play Core IPC, dialogs, and lifecycle checks. */
    private final Activity mActivity;

    /** Supplier for {@link SnackbarManager}, used when showing in-app update snackbars. */
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;

    /** Lazily created Play Core manager instance. */
    private @Nullable AppUpdateManager mAppUpdateManager;

    /**
     * Listener monitoring background download progress. Registered only while a download is in
     * flight or pending, and unregistered upon completion, cancellation, failure, or teardown to
     * prevent leaking the host Activity across configuration changes.
     */
    private @Nullable InstallStateUpdatedListener mInstallListener;

    /** Active discovery snackbar controller. Non-null while the discovery snackbar is displayed. */
    private @Nullable InAppUpdateSnackbarController mDiscoverySnackbarController;

    /** Active restart snackbar controller. Non-null while the restart snackbar is displayed. */
    private @Nullable InAppUpdateSnackbarController mRestartSnackbarController;

    /**
     * Teardown latch set to true during {@link #destroy()}. Suppresses asynchronous callbacks, UI
     * presentations, and telemetry/backoff writes triggered during Activity recreation (e.g.
     * rotation).
     */
    private boolean mIsDestroyed;

    /**
     * In-flight guard set to true while an update flow or metadata query is starting via the app
     * menu. Debounces rapid taps to prevent duplicate concurrent flow invocations.
     */
    private boolean mIsFlowStarting;

    /** Timestamp of the last resume update check in milliseconds. */
    private static long sLastResumeCheckTimestampMs;

    /** Overrides the {@link AppUpdateManager} instance for testing. */
    public static void setAppUpdateManagerForTesting(@Nullable AppUpdateManager manager) {
        sAppUpdateManagerForTesting = manager;
        ResettersForTesting.register(() -> sAppUpdateManagerForTesting = null);
    }

    /**
     * Creates a new controller instance.
     *
     * @param activity The host {@link Activity}.
     * @param snackbarManagerSupplier Supplier for the {@link SnackbarManager} to display UI
     *     prompts.
     */
    public InAppUpdateController(
            Activity activity, Supplier<@Nullable SnackbarManager> snackbarManagerSupplier) {
        mActivity = activity;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    /**
     * Cleans up listeners, dismisses any active snackbars, and suppresses late-arriving
     * asynchronous callbacks when the host Activity is destroyed (e.g. during screen rotation or
     * finish).
     *
     * <p>Setting {@link #mIsDestroyed} first ensures that teardown dismissals do not record false
     * user-declined backoffs or emit dismissal telemetry.
     */
    public void destroy() {
        mIsDestroyed = true;
        mIsFlowStarting = false;
        unregisterInstallListener();
        if (mDiscoverySnackbarController != null) {
            mDiscoverySnackbarController.dismiss();
            mDiscoverySnackbarController = null;
        }
        if (mRestartSnackbarController != null) {
            mRestartSnackbarController.dismiss();
            mRestartSnackbarController = null;
        }
        mAppUpdateManager = null;
    }

    /**
     * Checks if this controller is destroyed, or if the host Activity is finishing, destroyed, or
     * undergoing a configuration change (e.g. rotation).
     *
     * @return {@code true} if destroyed, finishing, or changing configurations, indicating no
     *     further UI or flow progression should occur on this Activity instance.
     */
    private boolean isDestroyedFinishingOrChangingConfigurations() {
        return mIsDestroyed
                || mActivity.isFinishing()
                || mActivity.isDestroyed()
                || mActivity.isChangingConfigurations();
    }

    /** Resolves the {@link SnackbarManager} from the supplier. */
    private @Nullable SnackbarManager getSnackbarManager() {
        return mSnackbarManagerSupplier.get();
    }

    /**
     * Returns the existing {@link AppUpdateManager} or lazily initializes one via {@link
     * AppUpdateManagerFactory}. Returns {@link #sAppUpdateManagerForTesting} if set.
     */
    private AppUpdateManager getOrCreateAppUpdateManager() {
        if (sAppUpdateManagerForTesting != null) {
            mAppUpdateManager = sAppUpdateManagerForTesting;
            return mAppUpdateManager;
        }
        if (mAppUpdateManager == null) {
            mAppUpdateManager = AppUpdateManagerFactory.create(mActivity);
        }
        return mAppUpdateManager;
    }

    /**
     * Primary entry point invoked during deferred post-startup initialization.
     *
     * <p>Checks eligibility via {@link InAppUpdatePolicy#isEligible}, queries Google Play Core
     * asynchronously for update availability, and reacts based on the returned install status:
     *
     * <ul>
     *   <li>If downloading or pending: Registers an {@link InstallStateUpdatedListener} to resume
     *       monitoring progress.
     *   <li>If already downloaded: Evaluates {@link InAppUpdatePolicy#isRestartPromptAllowed} and
     *       displays the restart snackbar if allowed. Notice that discovery throttle is NOT checked
     *       here; declining a download does not suppress restarting an already staged update.
     *   <li>If an update is available: Verifies that {@link AppUpdateType#FLEXIBLE} is allowed by
     *       Play Core and that discovery is not throttled via {@link
     *       InAppUpdatePolicy#isDiscoveryPromptAllowed}, then displays the discovery snackbar.
     * </ul>
     */
    public void checkAndStartUpdateFlow() {
        if (isDestroyedFinishingOrChangingConfigurations()) {
            return;
        }
        // Pre-flight check: short-circuits immediately if build or device is ineligible,
        // avoiding unnecessary IPC to Play Services.
        if (!InAppUpdatePolicy.isEligible(mActivity)) {
            return;
        }

        AppUpdateManager manager = getOrCreateAppUpdateManager();
        // Play Core's asynchronous Task API signals retrieval errors via addOnFailureListener
        // rather than throwing synchronous exceptions, so a synchronous try-catch is not required.
        manager.getAppUpdateInfo()
                .addOnSuccessListener(
                        mActivity,
                        appUpdateInfo -> {
                            // Guard against asynchronous callbacks delivering results after
                            // the host Activity has begun teardown.
                            if (isDestroyedFinishingOrChangingConfigurations()) {
                                return;
                            }
                            int rawInstallStatus = appUpdateInfo.installStatus();
                            int rawAvailability = appUpdateInfo.updateAvailability();

                            // Case 1: Download already in progress. Attach listener to observe
                            // completion.
                            if (rawInstallStatus == InstallStatus.DOWNLOADING
                                    || rawInstallStatus == InstallStatus.PENDING) {
                                Log.i(
                                        TAG,
                                        "checkAndStartUpdateFlow: Update downloading/pending ->"
                                                + " registering listener.");
                                registerInstallListener();
                                return;
                            }

                            // Case 2: Update already downloaded and staged on disk. Check restart
                            // throttle (asymmetric: discovery backoff does not suppress restart).
                            if (rawInstallStatus == InstallStatus.DOWNLOADED) {
                                if (!InAppUpdatePolicy.isRestartPromptAllowed(mActivity)) {
                                    return;
                                }
                                Log.i(
                                        TAG,
                                        "checkAndStartUpdateFlow: Update already downloaded ->"
                                                + " displaying restart snackbar.");
                                showRestartSnackbar();
                                return;
                            }

                            // Case 3: Update available. Allow command-line force switch override
                            // for local testing.
                            int updateAvailability = rawAvailability;
                            if (InAppUpdatePolicy.isForceUpdate()
                                    && updateAvailability != UpdateAvailability.UPDATE_AVAILABLE) {
                                updateAvailability = UpdateAvailability.UPDATE_AVAILABLE;
                            }

                            if (updateAvailability == UpdateAvailability.UPDATE_AVAILABLE) {
                                // Strictly verify Flexible flow support: Chrome only supports
                                // non-blocking background downloads.
                                boolean flexAllowedByPlayCore =
                                        appUpdateInfo.isUpdateTypeAllowed(
                                                AppUpdateOptions.defaultOptions(
                                                        AppUpdateType.FLEXIBLE));
                                if (!flexAllowedByPlayCore && InAppUpdatePolicy.isForceUpdate()) {
                                    Log.w(
                                            TAG,
                                            "Force update active: overriding Play Core flexible"
                                                    + " check.");
                                }
                                boolean isFlexAllowed =
                                        flexAllowedByPlayCore || InAppUpdatePolicy.isForceUpdate();
                                if (isFlexAllowed) {
                                    // Check discovery throttle: suppressed if user declined a
                                    // recent prompt or if a recent download failed.
                                    if (InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity)) {
                                        Log.i(
                                                TAG,
                                                "checkAndStartUpdateFlow: Flexible update available"
                                                        + " -> showing discovery snackbar.");
                                        showDiscoverySnackbar(appUpdateInfo);
                                    }
                                } else {
                                    Log.w(
                                            TAG,
                                            "checkAndStartUpdateFlow: Flexible update type not"
                                                    + " allowed by Play Core.");
                                }
                            }
                        })
                .addOnFailureListener(
                        mActivity,
                        e -> {
                            Log.w(
                                    TAG,
                                    "Failed to retrieve AppUpdateInfo from Google Play Core: %s",
                                    e.getMessage(),
                                    e);
                        });
    }

    /** Records the outcome of an update download operation to UMA. */
    private static void logDownloadResult(@DownloadResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_DOWNLOAD_RESULT, result, DownloadResult.COUNT);
    }

    /**
     * Displays the discovery snackbar notifying the user that an update is available.
     *
     * <p>If the user accepts the snackbar action ("Update"), {@link #startFlow(AppUpdateInfo, int)}
     * is invoked to launch the Play Core consent dialog. If dismissed without action, {@link
     * InAppUpdateSnackbarController} internally records a discovery decline via {@link
     * InAppUpdatePolicy#recordUpdateDeclined()}.
     *
     * @param appUpdateInfo Current update metadata from Play Core.
     */
    private void showDiscoverySnackbar(AppUpdateInfo appUpdateInfo) {
        // Guard against duplicate prompts, active restart prompts, and destroyed activity state.
        if (mDiscoverySnackbarController != null
                || mRestartSnackbarController != null
                || isDestroyedFinishingOrChangingConfigurations()) {
            return;
        }

        SnackbarManager snackbarManager = getSnackbarManager();
        if (snackbarManager == null) {
            return;
        }

        mDiscoverySnackbarController =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity,
                        snackbarManager,
                        () -> {
                            // User clicked "Update": clear reference and trigger flexible flow.
                            mDiscoverySnackbarController = null;
                            startFlow(appUpdateInfo, AppUpdateType.FLEXIBLE);
                        },
                        () -> {
                            // Snackbar dismissed (timeout or swipe): clear reference.
                            mDiscoverySnackbarController = null;
                        });
    }

    /**
     * Displays the restart snackbar notifying the user that an update has finished downloading and
     * offering to relaunch Chrome.
     *
     * <p>If the user taps "Restart", {@link AppUpdateManager#completeUpdate()} is called to perform
     * the binary swap and relaunch the app. If dismissed without action, {@link
     * InAppUpdateSnackbarController} records a restart decline via {@link
     * InAppUpdatePolicy#recordRestartDeclined()}.
     */
    private void showRestartSnackbar() {
        // Guard against duplicate prompts and destroyed activity state.
        if (mRestartSnackbarController != null || isDestroyedFinishingOrChangingConfigurations()) {
            return;
        }

        SnackbarManager snackbarManager = getSnackbarManager();
        if (snackbarManager == null) {
            return;
        }

        if (mDiscoverySnackbarController != null) {
            mDiscoverySnackbarController.dismiss();
            mDiscoverySnackbarController = null;
        }

        mRestartSnackbarController =
                InAppUpdateSnackbarController.showRestart(
                        mActivity,
                        snackbarManager,
                        () -> {
                            // User clicked "Restart": invoke completeUpdate() to restart Chrome.
                            mRestartSnackbarController = null;
                            if (isDestroyedFinishingOrChangingConfigurations()) {
                                return;
                            }
                            AppUpdateManager manager = getOrCreateAppUpdateManager();
                            manager.completeUpdate()
                                    .addOnFailureListener(
                                            mActivity,
                                            e -> {
                                                Log.e(
                                                        TAG,
                                                        "completeUpdate() failed on restart"
                                                                + " action: %s",
                                                        e.getMessage(),
                                                        e);
                                                // Record backoff on failure to prevent
                                                // infinite prompt loops.
                                                InAppUpdatePolicy.recordDownloadFailed();
                                            });
                        },
                        () -> {
                            // Snackbar dismissed without action: clear reference.
                            mRestartSnackbarController = null;
                        });
    }

    /**
     * Requests Google Play Core to launch the in-app update consent dialog via {@link
     * AppUpdateManager#startUpdateFlowForResult}.
     *
     * <p>The install state listener is registered prior to starting the flow to avoid missing early
     * state transitions. Play Core presents a system-managed bottom sheet/dialog to the user.
     *
     * @param appUpdateInfo Current update metadata.
     * @param updateType Flow type to start (strictly {@link AppUpdateType#FLEXIBLE}).
     * @return True if the update flow was successfully initiated; false otherwise.
     */
    private boolean startFlow(AppUpdateInfo appUpdateInfo, int updateType) {
        if (isDestroyedFinishingOrChangingConfigurations()) {
            return false;
        }

        // Attach install listener before initiating update flow to catch early download states.
        registerInstallListener();

        Log.i(TAG, "startFlow: Starting Play Core in-app update flow (type=%d).", updateType);
        try {
            AppUpdateOptions options = AppUpdateOptions.defaultOptions(updateType);
            AppUpdateManager manager = getOrCreateAppUpdateManager();

            boolean started =
                    manager.startUpdateFlowForResult(
                            appUpdateInfo, mActivity, options, REQUEST_CODE_IN_APP_UPDATE);
            if (started) {
                logDownloadResult(DownloadResult.FLOW_STARTED);
                return true;
            } else {
                Log.w(TAG, "startUpdateFlowForResult returned false.");
                logDownloadResult(DownloadResult.START_FAILED);
                InAppUpdatePolicy.recordDownloadFailed();
                unregisterInstallListener();
                return false;
            }
        } catch (IntentSender.SendIntentException e) {
            Log.e(TAG, "SendIntentException in startUpdateFlowForResult: %s", e.getMessage(), e);
            logDownloadResult(DownloadResult.SEND_INTENT_FAILED);
            InAppUpdatePolicy.recordDownloadFailed();
            unregisterInstallListener();
            return false;
        } catch (RuntimeException e) {
            Log.e(TAG, "RuntimeException in startUpdateFlowForResult: %s", e.getMessage(), e);
            logDownloadResult(DownloadResult.START_EXCEPTION);
            InAppUpdatePolicy.recordDownloadFailed();
            unregisterInstallListener();
            return false;
        }
    }

    /**
     * Registers an {@link InstallStateUpdatedListener} with Play Core to monitor flexible download
     * state changes.
     *
     * <p>Only one listener is registered at a time. The listener handles:
     *
     * <ul>
     *   <li>{@link InstallStatus#DOWNLOADED}: Clears restart backoff, displays restart snackbar,
     *       and unregisters listener.
     *   <li>{@link InstallStatus#FAILED}: Records transient failure backoff and unregisters
     *       listener.
     *   <li>{@link InstallStatus#CANCELED}: Records discovery decline backoff and unregisters
     *       listener.
     * </ul>
     */
    private void registerInstallListener() {
        if (isDestroyedFinishingOrChangingConfigurations()) {
            return;
        }
        if (mInstallListener == null) {
            mInstallListener =
                    state -> {
                        if (isDestroyedFinishingOrChangingConfigurations()) {
                            return;
                        }
                        int status = state.installStatus();
                        int errorCode = state.installErrorCode();

                        if (status == InstallStatus.DOWNLOADED) {
                            Log.i(
                                    TAG,
                                    "InstallState: Update downloaded -> showing restart snackbar.");
                            logDownloadResult(DownloadResult.SUCCEEDED);
                            // Clear restart backoff so that newly downloaded updates always get an
                            // unthrottled prompt to relaunch.
                            InAppUpdatePolicy.clearRestartBackoff();
                            showRestartSnackbar();
                            unregisterInstallListener();
                        } else if (status == InstallStatus.FAILED) {
                            Log.w(TAG, "InstallState: Download failed (errorCode=%d).", errorCode);
                            logDownloadResult(DownloadResult.DOWNLOAD_FAILED);
                            // Record failure backoff for transient issues (network drop, disk
                            // space).
                            InAppUpdatePolicy.recordDownloadFailed();
                            unregisterInstallListener();
                        } else if (status == InstallStatus.CANCELED) {
                            Log.i(TAG, "InstallState: Download canceled.");
                            logDownloadResult(DownloadResult.DOWNLOAD_CANCELED);
                            // User canceled in-flight download: apply discovery backoff.
                            InAppUpdatePolicy.recordUpdateDeclined();
                            unregisterInstallListener();
                        } else if (status == InstallStatus.INSTALLING
                                || status == InstallStatus.INSTALLED) {
                            // InstallStatus.INSTALLING and INSTALLED occur during/after
                            // completeUpdate() and are intentionally ignored here.
                        }
                    };
            getOrCreateAppUpdateManager().registerListener(mInstallListener);
        }
    }

    /**
     * Unregisters the {@link InstallStateUpdatedListener} from Play Core and clears its reference.
     * Ensures no dead Activity references or callbacks leak across rotations.
     */
    private void unregisterInstallListener() {
        if (mAppUpdateManager != null && mInstallListener != null) {
            mAppUpdateManager.unregisterListener(mInstallListener);
            mInstallListener = null;
        }
    }

    @Nullable InstallStateUpdatedListener getInstallListenerForTesting() {
        return mInstallListener;
    }

    @Nullable InAppUpdateSnackbarController getDiscoverySnackbarControllerForTesting() {
        return mDiscoverySnackbarController;
    }

    @Nullable InAppUpdateSnackbarController getRestartSnackbarControllerForTesting() {
        return mRestartSnackbarController;
    }

    static void setLastResumeCheckTimestampMsForTesting(long timestampMs) {
        sLastResumeCheckTimestampMs = timestampMs;
        ResettersForTesting.register(() -> sLastResumeCheckTimestampMs = 0);
    }

    static void resetResumeCooldownForTesting() {
        sLastResumeCheckTimestampMs = 0;
        ResettersForTesting.register(() -> sLastResumeCheckTimestampMs = 0);
    }

    /**
     * Handles the outcome of the Google Play in-app update consent dialog.
     *
     * <p>Called by the host Activity when {@code requestCode == REQUEST_CODE_IN_APP_UPDATE}.
     *
     * <ul>
     *   <li>{@link Activity#RESULT_CANCELED}: User dismissed the Play consent dialog without
     *       accepting. Stated user preference -> records discovery backoff via {@link
     *       InAppUpdatePolicy#recordUpdateDeclined()} and unregisters listener.
     *   <li>{@link ActivityResult#RESULT_IN_APP_UPDATE_FAILED}: Play Core failed to start the flow
     *       (internal error) -> records failure backoff via {@link
     *       InAppUpdatePolicy#recordDownloadFailed()} and unregisters listener.
     *   <li>{@link Activity#RESULT_OK}: User consented to the update. Play Core has started the
     *       background download; progress is tracked by the registered install listener.
     * </ul>
     *
     * @param requestCode The request code supplied to {@link Activity#startIntentSenderForResult}.
     * @param resultCode The result code returned by the child activity.
     */
    public void handleActivityResult(int requestCode, int resultCode) {
        if (requestCode == REQUEST_CODE_IN_APP_UPDATE) {
            if (resultCode == Activity.RESULT_CANCELED) {
                Log.i(
                        TAG,
                        "handleActivityResult: User canceled update prompt -> recording"
                                + " backoff.");
                logDownloadResult(DownloadResult.PROMPT_CANCELED);
                InAppUpdatePolicy.recordUpdateDeclined();
                unregisterInstallListener();
            } else if (resultCode == ActivityResult.RESULT_IN_APP_UPDATE_FAILED) {
                Log.w(
                        TAG,
                        "handleActivityResult: Play Core update flow failed"
                                + " (RESULT_IN_APP_UPDATE_FAILED).");
                logDownloadResult(DownloadResult.PLAY_CORE_FAILED);
                InAppUpdatePolicy.recordDownloadFailed();
                unregisterInstallListener();
            } else if (resultCode == Activity.RESULT_OK) {
                Log.i(TAG, "handleActivityResult: User accepted update prompt (RESULT_OK).");
                // Ensure listener is registered in case host Activity was recreated while Play
                // consent sheet was active.
                registerInstallListener();
            }
        }
    }

    private boolean isResumeThrottled() {
        ThreadUtils.assertOnUiThread();
        if (sLastResumeCheckTimestampMs <= 0) {
            return false;
        }
        int cooldownMinutes =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.IN_APP_UPDATE_FLOW,
                        PARAM_RESUME_COOLDOWN_MINUTES,
                        DEFAULT_RESUME_COOLDOWN_MINUTES);
        if (cooldownMinutes <= 0) {
            return false;
        }
        long elapsedMs = TimeUtils.elapsedRealtimeMillis() - sLastResumeCheckTimestampMs;
        return elapsedMs >= 0 && elapsedMs < TimeUnit.MINUTES.toMillis(cooldownMinutes);
    }

    /**
     * Activity lifecycle hook called during foreground resumption ({@code onResumeWithNative}).
     *
     * <p>Re-queries Google Play Core to check if an update completed downloading while Chrome was
     * backgrounded.
     *
     * <ul>
     *   <li>If {@link InstallStatus#DOWNLOADED}: Prompts the user with the restart snackbar if
     *       permitted by {@link InAppUpdatePolicy#isRestartPromptAllowed}.
     *   <li>If {@link InstallStatus#DOWNLOADING} or {@link InstallStatus#PENDING}: Ensures the
     *       {@link InstallStateUpdatedListener} is registered to catch download completion.
     * </ul>
     *
     * <p>Note: Discovery prompts are deliberately not initiated here; they are restricted to
     * deferred startup and manual menu invocation to prevent interrupting resuming users.
     */
    public void onResume() {
        ThreadUtils.assertOnUiThread();
        if (isDestroyedFinishingOrChangingConfigurations()) {
            return;
        }
        if (mRestartSnackbarController != null) {
            return;
        }
        if (!InAppUpdatePolicy.isForceUpdate() && isResumeThrottled()) {
            Log.i(TAG, "onResume: Throttled by resume cooldown.");
            return;
        }
        if (!InAppUpdatePolicy.isEligible(mActivity)) {
            return;
        }
        sLastResumeCheckTimestampMs = TimeUtils.elapsedRealtimeMillis();

        AppUpdateManager manager = getOrCreateAppUpdateManager();
        // Play Core's asynchronous Task API signals retrieval errors via addOnFailureListener
        // rather than throwing synchronous exceptions, so a synchronous try-catch is not required.
        manager.getAppUpdateInfo()
                .addOnSuccessListener(
                        mActivity,
                        appUpdateInfo -> {
                            if (isDestroyedFinishingOrChangingConfigurations()) {
                                return;
                            }
                            int status = appUpdateInfo.installStatus();
                            if (status == InstallStatus.DOWNLOADING
                                    || status == InstallStatus.PENDING) {
                                registerInstallListener();
                            } else if (status == InstallStatus.DOWNLOADED) {
                                if (!InAppUpdatePolicy.isRestartPromptAllowed(mActivity)) {
                                    return;
                                }
                                Log.i(
                                        TAG,
                                        "onResume: Downloaded update waiting -> displaying restart"
                                                + " snackbar.");
                                showRestartSnackbar();
                            }
                        })
                .addOnFailureListener(
                        mActivity,
                        e -> {
                            Log.w(
                                    TAG,
                                    "onResume: Failed to query AppUpdateInfo: %s",
                                    e.getMessage(),
                                    e);
                        });
    }

    /**
     * Handles user clicking the update menu item in the app menu with an optional fallback to start
     * an intent-based update if Play Core update is not available or fails.
     */
    public void onMenuItemClicked(@Nullable Runnable fallback) {
        if (isDestroyedFinishingOrChangingConfigurations()) {
            return;
        }
        if (mDiscoverySnackbarController != null) {
            mDiscoverySnackbarController.dismiss();
            mDiscoverySnackbarController = null;
        }
        if (mIsFlowStarting) {
            Log.i(
                    TAG,
                    "onMenuItemClicked: Update flow already starting -> ignoring duplicate tap.");
            return;
        }
        if (!InAppUpdatePolicy.isEligible(mActivity)) {
            Log.i(TAG, "onMenuItemClicked: Not eligible for in-app update -> triggering fallback.");
            if (fallback != null) {
                fallback.run();
            }
            return;
        }

        mIsFlowStarting = true;
        AppUpdateManager manager = getOrCreateAppUpdateManager();
        // This completion listener is deliberately unscoped (not tied to the Activity lifecycle)
        // so that mIsFlowStarting is guaranteed to reset even if the host Activity is stopped
        // while the query is in flight.
        manager.getAppUpdateInfo()
                .addOnCompleteListener(task -> mIsFlowStarting = false)
                .addOnSuccessListener(
                        mActivity,
                        appUpdateInfo -> {
                            if (isDestroyedFinishingOrChangingConfigurations()) {
                                return;
                            }
                            int status = appUpdateInfo.installStatus();
                            int availability = appUpdateInfo.updateAvailability();

                            if (status == InstallStatus.DOWNLOADED) {
                                if (mRestartSnackbarController != null) {
                                    mRestartSnackbarController.dismiss();
                                    mRestartSnackbarController = null;
                                }
                                Log.i(
                                        TAG,
                                        "onMenuItemClicked: Update already downloaded ->"
                                                + " restarting Chrome via completeUpdate().");
                                logAppMenuAction(AppMenuAction.TRIGGER_RESTART);
                                manager.completeUpdate()
                                        .addOnFailureListener(
                                                mActivity,
                                                e -> {
                                                    Log.e(
                                                            TAG,
                                                            "completeUpdate() failed on menu"
                                                                    + " click: %s",
                                                            e.getMessage(),
                                                            e);
                                                    InAppUpdatePolicy.recordDownloadFailed();
                                                    logAppMenuAction(AppMenuAction.RESTART_FAILED);
                                                    if (fallback != null) {
                                                        fallback.run();
                                                    }
                                                });
                                return;
                            }

                            // If an update download is already in progress, ensure the install
                            // listener is registered to display the restart snackbar upon
                            // completion.
                            if (status == InstallStatus.DOWNLOADING
                                    || status == InstallStatus.PENDING) {
                                Log.i(
                                        TAG,
                                        "onMenuItemClicked: Update already downloading/pending ->"
                                                + " registering listener.");
                                logAppMenuAction(AppMenuAction.ALREADY_DOWNLOADING);
                                registerInstallListener();
                                return;
                            }

                            if (availability == UpdateAvailability.UPDATE_AVAILABLE
                                    || InAppUpdatePolicy.isForceUpdate()) {
                                boolean isFlexibleAllowed =
                                        InAppUpdatePolicy.isForceUpdate()
                                                || appUpdateInfo.isUpdateTypeAllowed(
                                                        AppUpdateOptions.defaultOptions(
                                                                AppUpdateType.FLEXIBLE));
                                if (isFlexibleAllowed) {
                                    Log.i(TAG, "onMenuItemClicked: Starting flexible update flow.");
                                    if (startFlow(appUpdateInfo, AppUpdateType.FLEXIBLE)) {
                                        logAppMenuAction(AppMenuAction.TRIGGER_DOWNLOAD);
                                        return;
                                    }
                                } else {
                                    Log.w(
                                            TAG,
                                            "onMenuItemClicked: Flexible update not allowed by Play"
                                                    + " Core.");
                                }
                            }
                            Log.i(
                                    TAG,
                                    "onMenuItemClicked: Update unavailable or ineligible ->"
                                            + " triggering fallback.");
                            triggerFallback(fallback);
                        })
                .addOnFailureListener(
                        mActivity,
                        e -> {
                            if (isDestroyedFinishingOrChangingConfigurations()) {
                                return;
                            }
                            Log.w(
                                    TAG,
                                    "Failed to query AppUpdateInfo on menu item click: %s",
                                    e.getMessage(),
                                    e);
                            triggerFallback(fallback);
                        });
    }

    private static void logAppMenuAction(@AppMenuAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_APP_MENU_ACTION, action, AppMenuAction.COUNT);
    }

    private void triggerFallback(@Nullable Runnable fallback) {
        if (fallback != null) {
            logAppMenuAction(AppMenuAction.INTENT_FALLBACK);
            fallback.run();
        }
    }
}
