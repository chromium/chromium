// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.ActivityManager.AppTask;
import android.content.Intent;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.LastSessionExitType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;

import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Delegate to manage startup window policies and relaunch session restoration for {@link
 * ChromeTabbedActivity} windows.
 */
@JNINamespace("chrome::android")
@NullMarked
public class TabbedStartupWindowPolicyDelegate {
    /* package */ static final int PREF_UNSET = -1;

    private static @Nullable TabbedStartupWindowPolicyDelegate sInstance;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable PrefService mPrefService;

    /**
     * Tracks whether the startup window policy has been claimed for the current browser process.
     */
    private boolean mStartupPolicyClaimed;

    private TabbedStartupWindowPolicyDelegate() {}

    /** Returns the singleton instance of {@link TabbedStartupWindowPolicyDelegate}. */
    public static TabbedStartupWindowPolicyDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedStartupWindowPolicyDelegate();
        }
        return sInstance;
    }

    /**
     * Initializes the delegate with native preferences once native is ready. This method is
     * idempotent and can be safely called multiple times across activity lifecycles.
     *
     * @param prefService The {@link PrefService} to observe.
     */
    public void initializeWithNative(PrefService prefService) {
        if (!MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled()) return;
        // Early return if already initialized to ensure idempotency across multiple activities.
        if (mPrefChangeRegistrar != null) return;
        mPrefService = prefService;
        mPrefChangeRegistrar = new PrefChangeRegistrar(prefService);
        mPrefChangeRegistrar.addObserver(
                Pref.RESTORE_ON_STARTUP, this::onRestoreOnStartupPrefChanged);
        mPrefChangeRegistrar.addObserver(
                Pref.URLS_TO_RESTORE_ON_STARTUP, this::onRestoreOnStartupUrlsPrefChanged);
        onRestoreOnStartupPrefChanged();
        onRestoreOnStartupUrlsPrefChanged();
    }

    /**
     * Persists tabbed window state on session termination.
     *
     * @param exitType The {@link LastSessionExitType} to write.
     */
    public void maybeSaveWindowStateOnSessionTermination(@LastSessionExitType int exitType) {
        if (!MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            return;
        }

        // Only persist the QUIT session type to restore previous session windows if the startup
        // preference is unset or set to LAST.
        int startupPref = ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue();
        if (exitType == LastSessionExitType.QUIT
                && startupPref != PREF_UNSET
                && startupPref != SessionStartupPref.LAST) {
            return;
        }

        // If we are terminating the Chrome session with fewer than 2 active ChromeTabbedActivity
        // windows, there is no need to persist the QUIT session type that is used to restore
        // all active windows upon next launch.
        if (exitType == LastSessionExitType.QUIT
                && MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE) <= 1) {
            return;
        }

        ChromeMultiInstancePersistentStore.writeLastSessionExitType(exitType);
    }

    /**
     * Claims and evaluates whether default instance ID allocation should force allocating a
     * brand-new instance ID instead of adopting an existing persisted instance.
     *
     * <p>This occurs when:
     *
     * <ul>
     *   <li>The previous session was closed by the application (clean shutdown with single window)
     *       and the on-startup user preference is unset or configured to restore the last session
     *       (LAST).
     *   <li>The on-startup user preference is configured to open the New Tab page (NEW_TAB).
     * </ul>
     *
     * @param isIncognito Whether the launch intent is incognito.
     * @return {@code true} if a fresh window instance ID should be forced on startup; {@code false}
     *     otherwise.
     */
    /* package */ boolean claimForceNewInstancePolicy(boolean isIncognito) {
        assert MultiWindowUtils.isMultiInstanceApi31Enabled();

        boolean isStartupPolicyEnabled = MultiWindowUtils.isNewStartupWindowPolicyEnabled();
        boolean isPrefSyncEnabled = MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled();
        if (!isStartupPolicyEnabled && !isPrefSyncEnabled) {
            return false;
        }

        if (mStartupPolicyClaimed) {
            return false;
        }
        mStartupPolicyClaimed = true;

        // Incognito windows do not apply startup policies or evaluate user preferences, but
        // a cold-started incognito window marks the browser session as active and claims the
        // startup policy for the current process.
        if (isIncognito) {
            return false;
        }

        int startupPref = ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue();
        boolean isLastSessionCleanExit =
                isStartupPolicyEnabled
                        && ChromeMultiInstancePersistentStore.readLastSessionExitType()
                                == LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP;
        if (isLastSessionCleanExit
                && (startupPref == PREF_UNSET || startupPref == SessionStartupPref.LAST)) {
            return true;
        }

        return isPrefSyncEnabled && startupPref == SessionStartupPref.NEW_TAB;
    }

    /* package */ void applyPolicy(ChromeTabbedActivity activity) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()
                || !MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            return;
        }

        int exitType = ChromeMultiInstancePersistentStore.readLastSessionExitType();
        ChromeMultiInstancePersistentStore.clearLastSessionExitType();

        // Guard: Do not attempt to restore previous session windows onto an Incognito host
        // activity.
        if (activity.isIncognitoWindow()) {
            return;
        }

        if (exitType == LastSessionExitType.QUIT) {
            maybeRestoreWindowsAfterLaunch(activity);
        }
    }

    /* package */ void resetPolicy() {
        mStartupPolicyClaimed = false;
    }

    private void maybeRestoreWindowsAfterLaunch(ChromeTabbedActivity activity) {
        int currentInstanceId = activity.getWindowId();
        Set<Integer> allIds = ChromeMultiInstancePersistentStore.readAllInstanceIds();
        Map<Integer, AppTask> appTasksById = MultiWindowUtils.getAppTasksById(activity);
        boolean isMultiWindowMode = activity.isInMultiWindowMode();
        boolean windowsRestored = false;
        for (int windowId : allIds) {
            if (windowId == currentInstanceId) {
                continue;
            }

            // If Chrome starts in a fullscreen window and a restorable window's task is still
            // alive after previous app termination, skip processing such a window because we want
            // the host window to be in the foreground during such launch anyway.
            if (!isMultiWindowMode && MultiWindowUtils.isTaskAlive(windowId, appTasksById)) {
                continue;
            }

            if (ChromeMultiInstancePersistentStore.readIsRecoverable(windowId)) {
                Intent intent =
                        MultiWindowUtils.createNewWindowIntent(
                                activity,
                                windowId,
                                /* preferNew= */ false,
                                /* openAdjacently= */ isMultiWindowMode,
                                NewWindowAppSource.RELAUNCH);
                if (intent != null) {
                    // Finish any existing live task for this instance before starting a new
                    // activity in multi-window mode, to avoid creating duplicate tasks and leaving
                    // the old task orphaned in non-multiwindow mode.
                    if (MultiWindowUtils.isTaskAlive(windowId, appTasksById)) {
                        int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
                        AppTask task = appTasksById.get(taskId);
                        if (task != null) {
                            task.finishAndRemoveTask();
                        }
                    }

                    // Reset recoverability of an instance before creating a new activity for it to
                    // avoid propagating stale state for a window to a future session if the
                    // activity creation fails during restoration in the current session.
                    ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, false);
                    activity.startActivity(intent);
                    windowsRestored = true;
                }
            }
        }
        if (windowsRestored) {
            ApiCompatibilityUtils.moveTaskToFront(activity, activity.getTaskId(), /* flags= */ 0);
        }
    }

    private void onRestoreOnStartupPrefChanged() {
        int type = assertNonNull(mPrefService).getInteger(Pref.RESTORE_ON_STARTUP);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(type);
    }

    private void onRestoreOnStartupUrlsPrefChanged() {
        assertNonNull(mPrefService);
        List<String> urls =
                TabbedStartupWindowPolicyDelegateJni.get().getSessionStartupUrls(mPrefService);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(urls);
    }

    /* package */ void resetForTesting() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        mPrefService = null;
        mStartupPolicyClaimed = false;
    }

    /* package */ static void setInstanceForTesting(
            @Nullable TabbedStartupWindowPolicyDelegate delegate) {
        sInstance = delegate;
        ResettersForTesting.register(() -> sInstance = null);
    }

    @NativeMethods
    public interface Natives {
        @JniType("std::vector<std::string>")
        List<String> getSessionStartupUrls(@JniType("PrefService*") PrefService prefService);
    }
}
