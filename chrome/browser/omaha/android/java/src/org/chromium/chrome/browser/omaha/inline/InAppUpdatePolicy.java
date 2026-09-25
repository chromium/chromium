// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.gms.ChromiumPlayServicesAvailability;

import java.util.concurrent.TimeUnit;

/**
 * Decides whether Chrome may offer a Play Core in-app update, and records the throttling state
 * those decisions depend on.
 *
 * <p>This class holds no Play Core state and shows no UI. It is the single place that reads the
 * feature flag, the device configuration and the persisted backoff timestamps, so that eligibility
 * can be reasoned about, and tested, without depending on Play Core.
 *
 * <p>Three throttles are tracked independently, because they answer different questions:
 *
 * <ul>
 *   <li><b>Discovery</b>: the user declined to download an available update.
 *   <li><b>Restart</b>: the user declined to relaunch into an update already staged on the device.
 *   <li><b>Failure</b>: a download failed for reasons outside the user's control.
 * </ul>
 *
 * <p>Declining a download says nothing about whether the user wants to relaunch into an update that
 * is already downloaded, so the discovery throttle deliberately does not suppress the restart
 * prompt.
 */
@NullMarked
public final class InAppUpdatePolicy {
    private static final String TAG = "InAppUpdateFlow";

    /** Command line switch that bypasses every eligibility guard and every backoff. */
    public static final String FORCE_IN_APP_UPDATE = "force-in-app-update";

    @VisibleForTesting
    static final String PREF_KEY_DISCOVERY_BACKOFF =
            ChromePreferenceKeys.IN_APP_UPDATE_DISCOVERY_BACKOFF_TIMESTAMP;

    @VisibleForTesting
    static final String PREF_KEY_RESTART_BACKOFF =
            ChromePreferenceKeys.IN_APP_UPDATE_RESTART_BACKOFF_TIMESTAMP;

    @VisibleForTesting
    static final String PREF_KEY_FAILURE_BACKOFF =
            ChromePreferenceKeys.IN_APP_UPDATE_FAILURE_BACKOFF_TIMESTAMP;

    @VisibleForTesting static final String PARAM_DISCOVERY_BACKOFF_HOURS = "prompt_backoff_hours";

    @VisibleForTesting
    static final String PARAM_RESTART_BACKOFF_HOURS = "restart_prompt_backoff_hours";

    @VisibleForTesting static final String PARAM_FAILURE_BACKOFF_HOURS = "failure_backoff_hours";

    /** How long to wait before re-offering a download the user declined. */
    @VisibleForTesting static final int DEFAULT_DISCOVERY_BACKOFF_HOURS = 24;

    /** How long to wait before re-offering a relaunch the user declined. */
    @VisibleForTesting static final int DEFAULT_RESTART_BACKOFF_HOURS = 24;

    /**
     * How long to wait before retrying after a failed download. Shorter than the two user-declined
     * backoffs, because a failure is a transient system condition rather than a stated preference.
     */
    @VisibleForTesting static final int DEFAULT_FAILURE_BACKOFF_HOURS = 6;

    private static @Nullable Boolean sIsOfficialBuildForTesting;

    private InAppUpdatePolicy() {}

    /** Overrides the branded official build guard in unit tests. */
    public static void setIsOfficialBuildForTesting(boolean isOfficial) {
        sIsOfficialBuildForTesting = isOfficial;
        ResettersForTesting.register(() -> sIsOfficialBuildForTesting = null);
    }

    /**
     * Returns whether {@code --force-in-app-update} was passed. This bypasses every eligibility
     * guard and every backoff, and exists so the flow can be exercised on builds and devices that
     * would otherwise be ineligible.
     */
    public static boolean isForceUpdate() {
        return CommandLine.isInitialized()
                && CommandLine.getInstance().hasSwitch(FORCE_IN_APP_UPDATE);
    }

    /**
     * Returns whether the build, the device and the feature flag permit in-app updates at all.
     *
     * <p>This ignores throttling. Callers offering a prompt want {@link #isDiscoveryPromptAllowed}
     * or {@link #isRestartPromptAllowed} instead; this bare form is for user-initiated entry points
     * such as the app menu, which are never throttled.
     */
    public static boolean isEligible(Context context) {
        if (isForceUpdate()) {
            return true;
        }
        if (!isBrandedOfficialBuild()) {
            return false;
        }
        if (DeviceInfo.isAutomotive()) {
            return false;
        }
        // Check the flag first before calling {@link #isGooglePlayServicesAvailable}
        // to eliminate the unnecessary UI-thread IPC that may cause if the feature is disabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.IN_APP_UPDATE_FLOW)) {
            return false;
        }
        return ChromiumPlayServicesAvailability.isGooglePlayServicesAvailable(context);
    }

    /**
     * Returns whether Chrome may offer to download an available update. False if the user recently
     * declined an offer, or if a download recently failed.
     */
    public static boolean isDiscoveryPromptAllowed(Context context) {
        if (!isEligible(context)) {
            return false;
        }
        if (isBackoffActive(
                PREF_KEY_DISCOVERY_BACKOFF,
                PARAM_DISCOVERY_BACKOFF_HOURS,
                DEFAULT_DISCOVERY_BACKOFF_HOURS)) {
            Log.i(TAG, "Discovery prompt suppressed: user recently declined an update.");
            return false;
        }
        if (isBackoffActive(
                PREF_KEY_FAILURE_BACKOFF,
                PARAM_FAILURE_BACKOFF_HOURS,
                DEFAULT_FAILURE_BACKOFF_HOURS)) {
            Log.i(TAG, "Discovery prompt suppressed: a recent download failed.");
            return false;
        }
        return true;
    }

    /**
     * Returns whether Chrome may offer to relaunch into an update that is already downloaded and
     * staged. False only if the user recently declined such an offer; a declined or failed
     * <em>download</em> does not suppress this, because the payload is already on the device.
     */
    public static boolean isRestartPromptAllowed(Context context) {
        if (!isEligible(context)) {
            return false;
        }
        if (isBackoffActive(
                PREF_KEY_RESTART_BACKOFF,
                PARAM_RESTART_BACKOFF_HOURS,
                DEFAULT_RESTART_BACKOFF_HOURS)) {
            Log.i(TAG, "Restart prompt suppressed: user recently declined a relaunch.");
            return false;
        }
        return true;
    }

    /**
     * Records that the user declined to download an update, whether by dismissing the discovery
     * snackbar, cancelling Play's consent dialog, or cancelling an in-progress download.
     */
    public static void recordUpdateDeclined() {
        writeTimestamp(PREF_KEY_DISCOVERY_BACKOFF);
    }

    /** Records that a download failed for reasons outside the user's control. */
    public static void recordDownloadFailed() {
        writeTimestamp(PREF_KEY_FAILURE_BACKOFF);
    }

    /** Records that the user declined to relaunch into a staged update. */
    public static void recordRestartDeclined() {
        writeTimestamp(PREF_KEY_RESTART_BACKOFF);
    }

    /**
     * Clears the restart throttle, so that a freshly completed download always gets one unthrottled
     * relaunch prompt.
     */
    public static void clearRestartBackoff() {
        ChromeSharedPreferences.getInstance().removeKey(PREF_KEY_RESTART_BACKOFF);
    }

    private static boolean isBrandedOfficialBuild() {
        if (sIsOfficialBuildForTesting != null) {
            return sIsOfficialBuildForTesting;
        }
        return BuildConfig.IS_CHROME_BRANDED && VersionInfo.isOfficialBuild();
    }

    /**
     * Returns whether the throttle recorded at {@code prefKey} is still inside its window, that is,
     * whether the corresponding prompt is currently <em>blocked</em>.
     *
     * <p>A timestamp in the future, which can happen if the device clock moves backwards, fails
     * open (unthrottled) rather than indefinitely blocking the prompt. The stored timestamp is
     * intentionally retained rather than pruned on read to preserve pure query semantics and avoid
     * prematurely erasing a valid backoff during transient boot-time NTP desynchronization.
     */
    private static boolean isBackoffActive(String prefKey, String paramName, int defaultHours) {
        if (isForceUpdate()) {
            return false;
        }
        long lastTimestampMs = ChromeSharedPreferences.getInstance().readLong(prefKey);
        if (lastTimestampMs <= 0) {
            return false;
        }
        int backoffHours =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.IN_APP_UPDATE_FLOW, paramName, defaultHours);
        long elapsedMs = TimeUtils.currentTimeMillis() - lastTimestampMs;
        return elapsedMs >= 0 && elapsedMs < TimeUnit.HOURS.toMillis(backoffHours);
    }

    private static void writeTimestamp(String prefKey) {
        ChromeSharedPreferences.getInstance().writeLong(prefKey, TimeUtils.currentTimeMillis());
    }
}
