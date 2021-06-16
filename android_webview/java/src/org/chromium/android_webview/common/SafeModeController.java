// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.build.BuildConfig;

import java.util.HashSet;
import java.util.Set;

/**
 * A browser-process class for querying SafeMode state and executing SafeModeActions.
 */
public class SafeModeController {
    public static final String SAFE_MODE_STATE_COMPONENT =
            "org.chromium.android_webview.SafeModeState";

    private static final String TAG = "WebViewSafeMode";

    private SafeModeAction[] mRegisteredActions;

    private SafeModeController() {}

    private static class LazyHolder {
        static final SafeModeController INSTANCE = new SafeModeController();
    }

    public static SafeModeController getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Registers a list of {@link SafeModeAction}s which can be executed. This must only be called
     * once (per-process) and each action in the list must have a unique ID.
     *
     * @throws IllegalStateException if actions have already been registered.
     * @throws IllegalArgumentException if there are any duplicates.
     */
    public void registerActions(@NonNull SafeModeAction[] actions) {
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

    @VisibleForTesting
    public void unregisterActionsForTesting() {
        mRegisteredActions = null;
    }

    /**
     * Executes the given set of {@link SafeModeAction}s. Execution order is determined by the order
     * of the array registered by {@link registerActions}.
     *
     * @throws IllegalStateException if this is called before {@link registerActions}.
     */
    public void executeActions(Set<String> actionsToExecute) {
        // Execute SafeModeActions in a deterministic order.
        if (mRegisteredActions == null) {
            throw new IllegalStateException(
                    "Must registerActions() before calling executeActions()");
        }
        for (SafeModeAction action : mRegisteredActions) {
            if (actionsToExecute.contains(action.getId())) {
                // Allow SafeModeActions in general to perform disk reads and writes.
                try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                    Log.i(TAG, "Starting to execute %s", action.getId());
                    action.execute();
                    Log.i(TAG, "Finished executing %s", action.getId());
                }
            }
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
        ComponentName safeModeComponent =
                new ComponentName(webViewPackageName, SAFE_MODE_STATE_COMPONENT);
        int enabledState =
                context.getPackageManager().getComponentEnabledSetting(safeModeComponent);
        return enabledState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
    }
}
