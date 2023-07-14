// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import org.chromium.components.version_info.VersionInfo;

/**
 * Provides helper methods for the password migration warning.
 */
public class PasswordMigrationWarningUtil {
    /**
     * Returns the display name of the Chrome channel.
     */
    public static String getChannelString() {
        if (VersionInfo.isCanaryBuild()) {
            return "Canary";
        }
        if (VersionInfo.isDevBuild()) {
            return "Dev";
        }
        if (VersionInfo.isBetaBuild()) {
            return "Beta";
        }
        assert !VersionInfo.isStableBuild();
        return "";
    }
}
