// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;
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
            return null;
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
