// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
/**
 * Provider class for TabManagementModule.
 */
public class TabManagementModuleProvider {
    public static final String SYNTHETIC_TRIAL_POSTFIX = "SyntheticTrial";

    /**
     * Returns {@link TabManagementDelegate} implementation if the module is installed. null,
     * otherwise.
     */
    public static @Nullable TabManagementDelegate getDelegate() {
        if (!TabManagementModule.isInstalled()) {
            TabManagementModule.installDeferred();
            if (UmaSessionStats.isMetricsServiceAvailable()) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                        "DownloadAttempted");
                UmaSessionStats.registerSyntheticFieldTrial(
                        ChromeFeatureList.TAB_GROUPS_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                        "DownloadAttempted");
            }
            return null;
        }
        if (UmaSessionStats.isMetricsServiceAvailable()) {
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID)) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                        "Downloaded_Control");
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        ChromeFeatureList.TAB_GROUPS_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                        "Downloaded_Control");
            }
        }
        return TabManagementModule.getImpl();
    }

    /**
     * Returns whether TabManagementModule is supported by checking if the module is installed.
     */
    public static boolean isTabManagementModuleSupported() {
        return getDelegate() != null;
    }
}
