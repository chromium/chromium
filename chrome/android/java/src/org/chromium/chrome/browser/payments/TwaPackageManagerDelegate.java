// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;

/** Utility class for TWA package management */
public class TwaPackageManagerDelegate {
    /**
     * Get the package name of an activity if it is a Trusted Web Activity.
     * @param activity An activity that is intended to check whether its a Trusted Web Activity and
     *         get the package name from. Not allowed to be null.
     * @return The package name of a given activity if it is a Trusted Web Activity; null otherwise.
     */
    @Nullable
    public String getTwaPackageName(Activity activity) {
        assert activity != null;
        if (!(activity instanceof CustomTabActivity)) return null;
        CustomTabActivity customTabActivity = ((CustomTabActivity) activity);
        if (!customTabActivity.isInTwaMode()) return null;
        return customTabActivity.getTwaPackage();
    }

    /**
     * Get the package name of a specified package's installer app.
     * @param packageName The package name of the specified package. Not allowed to be null.
     * @return The package name of the installer app.
     */
    @Nullable
    public String getInstallerPackage(String packageName) {
        assert packageName != null;
        return ContextUtils.getApplicationContext().getPackageManager().getInstallerPackageName(
                packageName);
    }
}
