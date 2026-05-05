// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** A browser-process class for querying SafeMode state and executing SafeModeActions. */
@NullMarked
public class SafeModeController {
    public static final String SAFE_MODE_STATE_COMPONENT =
            "org.chromium.android_webview.SafeModeState";
    public static final String URI_AUTHORITY_SUFFIX = ".SafeModeContentProvider";
    public static final String SAFE_MODE_ACTIONS_URI_PATH = "/safe-mode-actions";
    public static final String ACTIONS_COLUMN = "actions";

    private static final String TAG = "WebViewSafeMode";

    private SafeModeAction @Nullable [] mRegisteredActions;

    private SafeModeController() {}

    private static @Nullable SafeModeController sInstanceForTests;

    private static class LazyHolder {
        static final SafeModeController INSTANCE = new SafeModeController();
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        SafeModeExecutionResult.SUCCESS,
        SafeModeExecutionResult.UNKNOWN_ERROR,
        SafeModeExecutionResult.ACTION_FAILED,
        SafeModeExecutionResult.ACTION_UNKNOWN,
        SafeModeExecutionResult.COUNT
    })
    public @interface SafeModeExecutionResult {
        int SUCCESS = 0;
        int UNKNOWN_ERROR = 1;
        int ACTION_FAILED = 2;
        int ACTION_UNKNOWN = 3;
        int COUNT = 3;
    }

    // LINT.IfChange(SafeModeActionIds)
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        SafeModeActionName.DELETE_VARIATIONS_SEED,
        SafeModeActionName.FAST_VARIATIONS_SEED,
        SafeModeActionName.NOOP,
        SafeModeActionName.DISABLE_ANDROID_AUTOFILL,
        SafeModeActionName.DISABLE_ORIGIN_TRIALS,
        SafeModeActionName.DISABLE_SAFE_BROWSING,
        SafeModeActionName.RESET_COMPONENT_UPDATER,
        SafeModeActionName.DISABLE_SUPERVISION_CHECKS,
        SafeModeActionName.DISABLE_STARTUP_TASKS_LOGIC,
        SafeModeActionName.DISABLE_CRASHY_CLASS
    })
    private @interface SafeModeActionName {
        int DELETE_VARIATIONS_SEED = 0;
        int FAST_VARIATIONS_SEED = 1;
        int NOOP = 2;
        int DISABLE_ANDROID_AUTOFILL = 3;
        // int DISABLE_CHROME_AUTOCOMPLETE = 4;  // Autofill replaced Autocomplete since Android O.
        int DISABLE_ORIGIN_TRIALS = 5;
        int DISABLE_SAFE_BROWSING = 6;
        int RESET_COMPONENT_UPDATER = 7;
        int DISABLE_SUPERVISION_CHECKS = 8;
        int DISABLE_STARTUP_TASKS_LOGIC = 9;
        int DISABLE_CRASHY_CLASS = 10;
        int COUNT = 11;
    }

    // Maps the SafeModeAction ID to its histogram enum
    @VisibleForTesting
    public static final Map<String, Integer> sSafeModeActionLoggingMap = createLoggingMap();

    private static Map<String, Integer> createLoggingMap() {
        Map<String, Integer> map = new HashMap<String, Integer>();
        map.put(
                SafeModeActionIds.DELETE_VARIATIONS_SEED,
                SafeModeActionName.DELETE_VARIATIONS_SEED);
        map.put(SafeModeActionIds.FAST_VARIATIONS_SEED, SafeModeActionName.FAST_VARIATIONS_SEED);
        map.put(SafeModeActionIds.NOOP, SafeModeActionName.NOOP);
        map.put(
                SafeModeActionIds.DISABLE_ANDROID_AUTOFILL,
                SafeModeActionName.DISABLE_ANDROID_AUTOFILL);
        map.put(SafeModeActionIds.DISABLE_ORIGIN_TRIALS, SafeModeActionName.DISABLE_ORIGIN_TRIALS);
        map.put(
                SafeModeActionIds.DISABLE_AW_SAFE_BROWSING,
                SafeModeActionName.DISABLE_SAFE_BROWSING);
        map.put(
                SafeModeActionIds.RESET_COMPONENT_UPDATER,
                SafeModeActionName.RESET_COMPONENT_UPDATER);
        map.put(
                SafeModeActionIds.DISABLE_SUPERVISION_CHECKS,
                SafeModeActionName.DISABLE_SUPERVISION_CHECKS);
        map.put(
                SafeModeActionIds.DISABLE_STARTUP_TASKS_LOGIC,
                SafeModeActionName.DISABLE_STARTUP_TASKS_LOGIC);
        map.put(SafeModeActionIds.DISABLE_CRASHY_CLASS, SafeModeActionName.DISABLE_CRASHY_CLASS);
        return map;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:SafeModeActionIds)

    /**
     * Sets the singleton instance for testing. Not thread safe, must only be called from single
     * threaded tests.
     *
     * @param controller The SafeModeController object to return from getInstance(). Passing in a
     *     null value resets this.
     */
    public static void setInstanceForTests(SafeModeController controller) {
        sInstanceForTests = controller;
        ResettersForTesting.register(() -> sInstanceForTests = null);
    }

    public static SafeModeController getInstance() {
        return sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
    }

    /**
     * Registers a list of {@link SafeModeAction}s which can be executed. This must only be called
     * once (per-process) and each action in the list must have a unique ID.
     *
     * @throws IllegalStateException if actions have already been registered.
     * @throws IllegalArgumentException if there are any duplicates.
     */
    public void registerActions(SafeModeAction[] actions) {
        if (mRegisteredActions != null) {
            throw new IllegalStateException("Already registered a list of actions in this process");
        }
        if (BuildConfig.ENABLE_ASSERTS) {
            // Verify we don't register any duplicate IDs. Only check this in debug builds to avoid
            // delaying startup.
            Set<String> allIds = new HashSet<>();
            for (SafeModeAction action : actions) {
                if (!allIds.add(action.getId())) {
                    throw new IllegalArgumentException("Received duplicate ID: " + action.getId());
                }
            }
        }
        mRegisteredActions = actions;
    }

    public void unregisterActionsForTesting() {
        mRegisteredActions = null;
    }

    public void enableAllRegisteredActionsForTesting() {
        if (mRegisteredActions == null) {
            throw new IllegalStateException(
                    "enableAllRegisteredActionsForTesting called before registerActions");
        }
        for (SafeModeAction action : mRegisteredActions) {
            action.enable();
        }
    }

    /** Overload for queryActions which gets the Context from ContextUtils. */
    public Set<String> queryActions(String webViewPackageName) {
        final Context appContext = ContextUtils.getApplicationContext();
        return queryActions(appContext, webViewPackageName);
    }

    /**
     * Queries SafeModeContentProvider for the set of actions which should be applied. Returns the
     * empty set if SafeMode is disabled. This should only be called from embedded WebView contexts.
     */
    public Set<String> queryActions(Context appContext, String webViewPackageName) {
        if (mRegisteredActions == null) {
            throw new IllegalStateException("Must registerActions() before calling queryActions()");
        }
        return queryActionsInternal(appContext, webViewPackageName);
    }

    /**
     * Overload for queryActions which does not require registerActions() to be called first. This
     * overload is only for ComponentUpdater and should be removed once that code is no longer
     * needed.
     */
    public Set<String> queryActionsUnsafe(String webViewPackageName) {
        final Context appContext = ContextUtils.getApplicationContext();
        return queryActionsInternal(appContext, webViewPackageName);
    }

    private Set<String> queryActionsInternal(Context appContext, String webViewPackageName) {
        Set<String> actions = new HashSet<>();

        Uri uri =
                new Uri.Builder()
                        .scheme("content")
                        .authority(webViewPackageName + URI_AUTHORITY_SUFFIX)
                        .path(SAFE_MODE_ACTIONS_URI_PATH)
                        .build();

        try (Cursor cursor =
                appContext
                        .getContentResolver()
                        .query(
                                uri,
                                /* projection= */ null,
                                /* selection= */ null,
                                /* selectionArgs= */ null,
                                /* sortOrder= */ null)) {
            if (cursor == null || cursor.getCount() == 0) {
                Log.i(TAG, "ContentProvider doesn't support querying '" + uri + "'");
                return actions;
            }
            int actionIdColumnIndex = cursor.getColumnIndexOrThrow(ACTIONS_COLUMN);
            while (cursor.moveToNext()) {
                actions.add(cursor.getString(actionIdColumnIndex));
            }
        }

        Log.i(TAG, "Received SafeModeActions: %s", actions);
        if (mRegisteredActions == null) return actions;
        for (SafeModeAction action : mRegisteredActions) {
            if (actions.contains(action.getId())) {
                Log.i(TAG, "Enabling %s.", action.getId());
                action.enable();
            }
        }
        return actions;
    }

    /**
     * Helper method to return whether a given SafeModeAction is enabled. If it has not been
     * registered, this method defaults to false and DOES NOT fail loudly. If this method is called
     * before executeActions then it will return false. If a SafeModeAction is later executed, it
     * will return true for the same action.
     *
     * @param id the SafeModeActionId of the action to query.
     * @return if a SafeModeAction has been registered and has been enabled.
     */
    public boolean isActionEnabled(String id) {
        if (mRegisteredActions == null) {
            Log.w(TAG, "SafeModeAction enablement checked before registerActions was called");
            return false;
        }
        for (SafeModeAction action : mRegisteredActions) {
            if (id.equals(action.getId())) {
                return action.isEnabled();
            }
        }
        return false;
    }

    /**
     * Executes the given set of {@link SafeModeAction}s. Execution order is determined by the order
     * of the array registered by {@link registerActions}.
     *
     * @return {@code true} if <b>all</b> actions succeeded, {@code false} otherwise.
     * @throws IllegalStateException if this is called before {@link registerActions}.
     */
    public @SafeModeExecutionResult int executeActions(Set<String> actionsToExecute)
            throws Throwable {
        // Execute SafeModeActions in a deterministic order.
        if (mRegisteredActions == null) {
            throw new IllegalStateException(
                    "Must registerActions() before calling executeActions()");
        }

        String currentSafeModeActionName = "";
        try {
            @SafeModeExecutionResult int overallStatus = SafeModeExecutionResult.SUCCESS;
            Set<String> allIds = new HashSet<>();
            for (SafeModeAction action : mRegisteredActions) {
                allIds.add(action.getId());
                if (actionsToExecute.contains(action.getId())) {
                    currentSafeModeActionName = action.getId();
                    // Allow SafeModeActions in general to perform disk reads and writes.
                    try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                        Log.i(TAG, "Starting to execute %s", currentSafeModeActionName);
                        if (action.executeAtStartup()) {
                            Log.i(
                                    TAG,
                                    "Finished executing %s (%s)",
                                    currentSafeModeActionName,
                                    "success");
                        } else {
                            overallStatus = SafeModeExecutionResult.ACTION_FAILED;
                            Log.e(
                                    TAG,
                                    "Finished executing %s (%s)",
                                    currentSafeModeActionName,
                                    "failure");
                        }
                    }
                    logSafeModeActionName(currentSafeModeActionName);
                }
            }

            if (overallStatus != SafeModeExecutionResult.ACTION_FAILED) {
                for (String action : actionsToExecute) {
                    if (!allIds.contains(action)) {
                        overallStatus = SafeModeExecutionResult.ACTION_UNKNOWN;
                        break;
                    }
                }
            }
            logSafeModeExecutionResult(overallStatus);
            return overallStatus;
        } catch (Throwable t) {
            if (!"".equals(currentSafeModeActionName)) {
                // Logging this with the ExecutionResult will help correlate failures with a
                // specific SafeModeAction
                logSafeModeActionName(currentSafeModeActionName);
            }
            logSafeModeExecutionResult(SafeModeController.SafeModeExecutionResult.UNKNOWN_ERROR);
            throw t;
        }
    }

    /**
     *
     * @return A copy of the list of registered {@link SafeModeAction} actions.
     */
    public SafeModeAction @Nullable [] getRegisteredActions() {
        if (mRegisteredActions == null) {
            return null;
        }
        return Arrays.copyOf(mRegisteredActions, mRegisteredActions.length);
    }

    private static void logSafeModeExecutionResult(@SafeModeExecutionResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.SafeMode.ExecutionResult", result, SafeModeExecutionResult.COUNT);
    }

    private static void logSafeModeActionName(String actionName) {
        if (sSafeModeActionLoggingMap.get(actionName) != null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.WebView.SafeMode.ActionName",
                    sSafeModeActionLoggingMap.get(actionName),
                    SafeModeActionName.COUNT);
        }
    }

    /**
     * Quickly determine whether SafeMode is enabled. SafeMode is off-by-default.
     *
     * @param webViewPackageName the package name of the WebView implementation to query about
     *     SafeMode (generally this is the current WebView provider).
     */
    public boolean isSafeModeEnabled(String webViewPackageName) {
        final Context context = ContextUtils.getApplicationContext();
        return isSafeModeEnabled(context, webViewPackageName);
    }

    /**
     * Quickly determine whether SafeMode is enabled. SafeMode is off-by-default.
     *
     * @param context the WebView context. This overload is used by early startup before
     *     ContextUtils has been initialized.
     * @param webViewPackageName the package name of the WebView implementation to query about
     *     SafeMode (generally this is the current WebView provider).
     */
    public boolean isSafeModeEnabled(Context context, String webViewPackageName) {
        ComponentName safeModeComponent =
                new ComponentName(webViewPackageName, SAFE_MODE_STATE_COMPONENT);
        int enabledState =
                context.getPackageManager().getComponentEnabledSetting(safeModeComponent);
        return enabledState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
    }
}
