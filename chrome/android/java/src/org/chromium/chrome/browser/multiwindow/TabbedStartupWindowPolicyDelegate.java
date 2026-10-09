// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.ActivityManager.AppTask;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SessionStartupPolicy;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
/* package */ class TabbedStartupWindowPolicyDelegate extends BaseTabbedStartupDelegate
        implements SyncStateChangedListener {
    /* package */ static final int PREF_UNSET = -1;

    /**
     * Launch allocation modes for the primary window during browser startup that determine how
     * session startup policies (window restoration, startup URLs) are applied.
     */
    @IntDef({
        StartupMode.EXPLICIT_INSTANCE,
        StartupMode.MAPPED_TASK,
        StartupMode.NEW_WINDOW,
        StartupMode.UNMAPPED_TASK
    })
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface StartupMode {
        /**
         * Tier 1: Launch targeting a specific, pre-selected window instance. Although explicit
         * instance selection typically occurs while the browser is already running, this mode is
         * supported defensively to disallow multi-window restoration if such a launch cold-starts
         * the process.
         */
        int EXPLICIT_INSTANCE = 0;

        /**
         * Tier 2: Launch reconnecting an existing task with a destroyed activity to its mapped
         * window instance.
         */
        int MAPPED_TASK = 1;

        /** Tier 3: Launch explicitly requesting a fresh, standalone new window. */
        int NEW_WINDOW = 2;

        /** Tier 4: Launch for an unmapped task allocating an available or unassigned instance. */
        int UNMAPPED_TASK = 3;
    }

    // LINT.IfChange(StartupPolicy)
    @IntDef({
        StartupPolicy.UNSET,
        StartupPolicy.LAST,
        StartupPolicy.NEW_TAB,
        StartupPolicy.URLS,
        StartupPolicy.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface StartupPolicy {
        /**
         * No startup preference is configured or synced; defaults to restoring the last session.
         */
        int UNSET = 0;

        /** Explicitly configured to restore the last session ("Continue where you left off"). */
        int LAST = 1;

        /** Explicitly configured to open the New Tab page. */
        int NEW_TAB = 2;

        /** Explicitly configured to open a specific page or set of pages. */
        int URLS = 3;

        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:StartupPolicy)

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

    /** Tracks whether window restoration is permitted for the current browser process session. */
    private boolean mCanRestoreWindows;

    private TabbedStartupWindowPolicyDelegate() {}

    // SyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        updateCachedRestoreOnStartupPref();
        updateCachedRestoreOnStartupUrlsPref();
    }

    // BaseTabbedStartupDelegate implementation.
    @Override
    protected void onRestorationInitiated() {
        RecordUserAction.record("Android.MultiWindow.StartupRestorationInitiated");
    }

    @Override
    protected void onAllWindowsRestored(long durationMillis) {
        RecordHistogram.recordTimesHistogram(
                "Android.MultiWindow.StartupRestorationDuration", durationMillis);
        RecordUserAction.record("Android.MultiWindow.StartupRestorationCompleted");
    }

    @Override
    protected void resetState() {
        super.resetState();
        mStartupPolicyClaimed = false;
        mHasEvaluatedStartupUrls = false;
        mCanRestoreWindows = false;
    }

    /* package */ static TabbedStartupWindowPolicyDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedStartupWindowPolicyDelegate();
        }
        return sInstance;
    }

    /* package */ void onNativeInitialized(Profile profile) {
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

    /* package */ void maybeSaveSessionStateOnTermination(@SessionStartupPolicy int startupPolicy) {
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

    /* package */ List<String> resolveStartupUrls(boolean incognito) {
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

        List<String> urls = ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls();
        RecordHistogram.recordCount100Histogram(
                "Android.MultiWindow.RestoreOnStartupUrlsCount", urls.size());
        return urls;
    }

    /**
     * Evaluates whether default instance ID allocation should force allocating a brand-new instance
     * ID instead of adopting an existing persisted instance.
     *
     * <p>This is a pure query method that does not modify the startup policy claim state.
     *
     * <p>Forcing a fresh instance ID (returning {@code true}) occurs when:
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
    /* package */ boolean shouldForceNewInstancePolicy(boolean isIncognito) {
        assert MultiWindowUtils.isMultiInstanceApi31Enabled();

        boolean isStartupPolicyEnabled = MultiWindowUtils.isNewStartupWindowPolicyEnabled();
        boolean isPrefSyncEnabled = MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled();
        if (!isStartupPolicyEnabled && !isPrefSyncEnabled) {
            return false;
        }

        if (mStartupPolicyClaimed || isIncognito) {
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

    /**
     * Claims the one-time session startup policy for the browser process.
     *
     * <p>This latch is claimed during pre-inflation window allocation once a valid instance ID is
     * determined for the primary launching window:
     *
     * <ul>
     *   <li>Tier 1 ({@link StartupMode#EXPLICIT_INSTANCE}): Multi-window restoration is disallowed.
     *   <li>Tier 2 ({@link StartupMode#MAPPED_TASK}): Multi-window restoration is permitted under
     *       {@link SessionStartupPolicy#RESTORE_ALL}.
     *   <li>Tier 3 ({@link StartupMode#NEW_WINDOW}): Multi-window restoration is disallowed and
     *       startup URLs are marked as evaluated to open a single NTP.
     *   <li>Tier 4 ({@link StartupMode#UNMAPPED_TASK}): Multi-window restoration is permitted under
     *       {@link SessionStartupPolicy#RESTORE_ALL}.
     * </ul>
     *
     * <p>Subsequent invocations in the same browser process are no-ops.
     *
     * @param isIncognito Whether the primary launching window is incognito.
     * @param startupMode The {@link StartupMode} specifying the launch allocation context.
     */
    /* package */ void claimStartupPolicy(boolean isIncognito, @StartupMode int startupMode) {
        assert MultiWindowUtils.isMultiInstanceApi31Enabled();

        boolean isStartupPolicyEnabled = MultiWindowUtils.isNewStartupWindowPolicyEnabled();
        boolean isPrefSyncEnabled = MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled();
        if (!isStartupPolicyEnabled && !isPrefSyncEnabled) {
            return;
        }

        if (mStartupPolicyClaimed) {
            // Any subsequent window launch in an active session must never evaluate startup URLs.
            mHasEvaluatedStartupUrls = true;
            return;
        }
        mStartupPolicyClaimed = true;
        mCanRestoreWindows =
                !isIncognito
                        && (startupMode == StartupMode.MAPPED_TASK
                                || startupMode == StartupMode.UNMAPPED_TASK);

        // When a fresh new window or incognito window is launched first in a new session, mark
        // startup URLs as evaluated so that a single NTP is opened instead of startup URLs.
        if (isIncognito || startupMode == StartupMode.NEW_WINDOW) {
            mHasEvaluatedStartupUrls = true;
        }

        if (!isIncognito) {
            int startupPref = ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue();
            @StartupPolicy int startupPolicy;
            if (startupPref == SessionStartupPref.LAST) {
                startupPolicy = StartupPolicy.LAST;
            } else if (startupPref == SessionStartupPref.NEW_TAB) {
                startupPolicy = StartupPolicy.NEW_TAB;
            } else if (startupPref == SessionStartupPref.URLS) {
                startupPolicy = StartupPolicy.URLS;
            } else {
                startupPolicy = StartupPolicy.UNSET;
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.MultiWindow.StartupPolicy", startupPolicy, StartupPolicy.NUM_ENTRIES);
        }
    }

    /* package */ void applyPolicy(ChromeTabbedActivity activity) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()
                || !MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            return;
        }

        int startupPolicy = ChromeMultiInstancePersistentStore.readSessionStartupPolicy();
        ChromeMultiInstancePersistentStore.clearSessionStartupPolicy();

        if (!mCanRestoreWindows || startupPolicy != SessionStartupPolicy.RESTORE_ALL) {
            return;
        }

        int currentInstanceId = activity.getWindowId();
        Set<Integer> allIds = ChromeMultiInstancePersistentStore.readAllInstanceIds();
        Map<Integer, AppTask> appTasksById = MultiWindowUtils.getAppTasksById(activity);
        int recoverableWindowCount = 0;
        boolean windowsRestored = false;
        for (int windowId : allIds) {
            int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
            if (windowId != currentInstanceId
                    && ChromeMultiInstancePersistentStore.readIsRecoverable(windowId)) {
                recoverableWindowCount++;
                windowsRestored |=
                        restoreWindow(
                                activity,
                                windowId,
                                appTasksById.get(taskId),
                                NewWindowAppSource.RELAUNCH);
            }
        }
        if (recoverableWindowCount > 0) {
            RecordHistogram.recordExactLinearHistogram(
                    "Android.MultiWindow.StartupRestorationWindowCount",
                    recoverableWindowCount,
                    TabWindowManager.MAX_SELECTORS_1000 + 1);
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
        resetState();
    }

    /* package */ static void setInstanceForTesting(
            @Nullable TabbedStartupWindowPolicyDelegate delegate) {
        sInstance = delegate;
        ResettersForTesting.register(() -> sInstance = null);
    }

    @NativeMethods
    /* package */ interface Natives {
        @JniType("std::vector<std::string>")
        List<String> getSessionStartupUrls(@JniType("PrefService*") PrefService prefService);

        void setSessionStartupUrlsForTesting(
                @JniType("PrefService*") PrefService prefService,
                @JniType("std::vector<std::string>") List<String> urls);
    }
}
