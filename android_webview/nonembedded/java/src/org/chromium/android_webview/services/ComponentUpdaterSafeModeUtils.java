// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.io.File;
import java.util.Set;

/** A util class for Component Updater Safe Mode operations. */
public class ComponentUpdaterSafeModeUtils {
    private static final String TAG = "AwCUSafeMode";
    private static final String HISTOGRAM_COMPONENT_UPDATER_SAFEMODE_EXECUTED =
            "Android.WebView.ComponentUpdater.SafeModeActionExecuted";

    // Don't instantiate this class.
    private ComponentUpdaterSafeModeUtils() {}

    /**
     * Executes Component Updater Safe Mode actions, if Safe Mode is enabled.
     * @param configDir The directory containing dynamic configs.
     * @return true if any SafeMode actions were executed, false otherwise.
     */
    public static boolean executeSafeModeIfEnabled(File configDir) {
        SafeModeController controller = SafeModeController.getInstance();
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        if (!controller.isSafeModeEnabled(packageName)) {
            RecordHistogram.recordBooleanHistogram(
                    HISTOGRAM_COMPONENT_UPDATER_SAFEMODE_EXECUTED, false);
            return false;
        }
        Set<String> actions = controller.queryActions(packageName);

        if (actions.isEmpty() || !actions.contains(SafeModeActionIds.RESET_COMPONENT_UPDATER)) {
            RecordHistogram.recordBooleanHistogram(
                    HISTOGRAM_COMPONENT_UPDATER_SAFEMODE_EXECUTED, false);
            return false;
        }

        if (!FileUtils.recursivelyDeleteFile(configDir, null)) {
            Log.w(TAG, "Failed to delete " + configDir.getAbsolutePath());
        }
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_COMPONENT_UPDATER_SAFEMODE_EXECUTED, true);
        return true;
    }
}
