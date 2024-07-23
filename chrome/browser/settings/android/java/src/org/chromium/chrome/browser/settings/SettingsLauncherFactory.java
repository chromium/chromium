// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.base.ResettersForTesting;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/** Factory for {@link SettingsLauncher}. Can be used from chrome/browser modules. */
public class SettingsLauncherFactory {
    private static SettingsLauncher sInstance = new SettingsLauncherImpl();
    private static SettingsLauncher sInstanceForTesting;

    /** Create a {@link SettingsLauncher}. */
    public static SettingsLauncher createSettingsLauncher() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return sInstance;
    }

    /** Set a test double to replace the real {@link SettingsLauncherImpl} in a test. */
    public static void setInstanceForTesting(SettingsLauncher instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
