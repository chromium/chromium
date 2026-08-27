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
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SessionStartupPolicy;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Delegate to manage startup window policies and relaunch session restoration for {@link
 * ChromeTabbedActivity} windows.
 */
@JNINamespace("chrome::android")
@NullMarked
public class TabbedStartupWindowPolicyDelegate implements SyncStateChangedListener {
    /* package */ static final int PREF_UNSET = -1;

    private static @Nullable TabbedStartupWindowPolicyDelegate sInstance;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable PrefService mPrefService;
    private @Nullable SyncService mSyncService;

    /**
     * Tracks whether the startup window policy has been claimed for the current browser process.
     */
    private boolean mStartupPolicyClaimed;

    /**
     * Tracks whether the startup preference URLs have been evaluated in the current browser
     * process.
     */
    // TODO (crbug.com/548199511): Potentially remove this state and leverage single state to claim
    // and apply startup policies.
    private boolean mHasEvaluatedStartupUrls;

    private TabbedStartupWindowPolicyDelegate() {}

    /** Returns the singleton instance of {@link TabbedStartupWindowPolicyDelegate}. */
    public static TabbedStartupWindowPolicyDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedStartupWindowPolicyDelegate();
        }
        return sInstance;
    }

    // SyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        updateCachedRestoreOnStartupPref();
        updateCachedRestoreOnStartupUrlsPref();
    }

    /**
     * Initializes the delegate with native preferences once native is ready. This method is
     * idempotent and can be safely called multiple times across activity lifecycles.
     *
     * @param profile The {@link Profile} associated with the browser session.
     */
    public void initializeWithNative(Profile profile) {
        if (!MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled()) return;
        // Early return if already initialized to ensure idempotency across multiple activities.
        if (mPrefChangeRegistrar != null) return;
        PrefService prefService = UserPrefs.get(profile);
        mPrefService = prefService;
        mPrefChangeRegistrar = new PrefChangeRegistrar(prefService);
        mPrefChangeRegistrar.addObserver(
                Pref.RESTORE_ON_STARTUP, this::updateCachedRestoreOnStartupPref);
        mPrefChangeRegistrar.addObserver(
                Pref.URLS_TO_RESTORE_ON_STARTUP, this::updateCachedRestoreOnStartupUrlsPref);
        mSyncService = SyncServiceFactory.getForProfile(profile);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }
        updateCachedRestoreOnStartupPref();
        updateCachedRestoreOnStartupUrlsPref();
    }

    /**
     * Records session state on termination that determines next session startup behavior.
     *
     * @param startupPolicy The {@link SessionStartupPolicy} to write.
     */
    public void maybeSaveSessionStateOnTermination(@SessionStartupPolicy int startupPolicy) {
        if (!MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            return;
        }

        // Only persist the session policy to determine next session startup behavior if the
        // startup preference is unset or set to LAST.
        int startupPref = ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue();
        if (startupPref != PREF_UNSET && startupPref != SessionStartupPref.LAST) {
            return;
        }

        // If we are terminating the Chrome session with fewer than 2 active ChromeTabbedActivity
        // windows, there is no need to persist the RESTORE_ALL session policy that is used to
        // restore all active windows upon next launch.
        if (startupPolicy == SessionStartupPolicy.RESTORE_ALL
                && MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE) <= 1) {
            return;
        }

        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(startupPolicy);
    }

    /**
     * Resolves the list of startup URLs to launch based on the session startup preference, if
     * applicable for this browser process. This evaluation occurs at most once per browser process.
     *
     * @param incognito Whether the startup is in incognito mode.
     * @return The list of valid startup URLs to open, or an empty list if none apply.
     */
    public List<String> resolveStartupUrls(boolean incognito) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()
                || !MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled()
                || mHasEvaluatedStartupUrls) {
            return Collections.emptyList();
        }

        mHasEvaluatedStartupUrls = true;

        if (incognito) {
            return Collections.emptyList();
        }

        int startupPref = ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue();
        if (startupPref != SessionStartupPref.URLS) {
            return Collections.emptyList();
        }

        return ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls();
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
     *   <li>The on-startup user preference is configured to open the New Tab page (NEW_TAB) or
     *       specific startup URLs (URLS).
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
                        && ChromeMultiInstancePersistentStore.readSessionStartupPolicy()
                                == SessionStartupPolicy.CREATE_NEW;
        if (isLastSessionCleanExit
                && (startupPref == PREF_UNSET || startupPref == SessionStartupPref.LAST)) {
            return true;
        }

        return isPrefSyncEnabled
                && (startupPref == SessionStartupPref.NEW_TAB
                        || startupPref == SessionStartupPref.URLS);
    }

    /* package */ void applyPolicy(ChromeTabbedActivity activity) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()
                || !MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            return;
        }

        int startupPolicy = ChromeMultiInstancePersistentStore.readSessionStartupPolicy();
        ChromeMultiInstancePersistentStore.clearSessionStartupPolicy();

        // Do not attempt to restore previous session windows from an incognito host.
        if (activity.isIncognitoWindow()) {
            return;
        }

        if (startupPolicy == SessionStartupPolicy.RESTORE_ALL) {
            maybeRestoreWindowsAfterLaunch(activity);
        }
    }

    /* package */ void resetPolicy() {
        mStartupPolicyClaimed = false;
        mHasEvaluatedStartupUrls = false;
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

    private void updateCachedRestoreOnStartupPref() {
        if (!isHistorySyncActive()) {
            ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(PREF_UNSET);
            return;
        }
        int type = assertNonNull(mPrefService).getInteger(Pref.RESTORE_ON_STARTUP);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(type);
    }

    private void updateCachedRestoreOnStartupUrlsPref() {
        if (!isHistorySyncActive()) {
            ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(Collections.emptyList());
            return;
        }
        assertNonNull(mPrefService);
        List<String> urls =
                TabbedStartupWindowPolicyDelegateJni.get().getSessionStartupUrls(mPrefService);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(urls);
    }

    private boolean isHistorySyncActive() {
        if (mSyncService == null) return false;
        // If the user is signed out (account info is null), History sync is inactive and
        // account-level synced preferences must not be used.
        if (mSyncService.getAccountInfo() == null) return false;
        return mSyncService.getSelectedTypes().contains(UserSelectableType.HISTORY);
    }

    /* package */ void resetForTesting() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
            mSyncService = null;
        }
        mPrefService = null;
        mStartupPolicyClaimed = false;
        mHasEvaluatedStartupUrls = false;
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
