// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;

import org.chromium.base.version_info.VersionInfo;

/** Provides helper methods for the password migration warning. */
public class PasswordMigrationWarningUtil {
    /** Returns the display name of the Chrome channel. */
    public static String getChannelString(Context context) {
        if (VersionInfo.isCanaryBuild()) {
            return context.getString(R.string.chrome_channel_name_canary);
        }
        if (VersionInfo.isDevBuild()) {
            return context.getString(R.string.chrome_channel_name_dev);
        }
        if (VersionInfo.isBetaBuild()) {
            return context.getString(R.string.chrome_channel_name_beta);
        }
        assert !VersionInfo.isStableBuild();
        return "";
    }
}
