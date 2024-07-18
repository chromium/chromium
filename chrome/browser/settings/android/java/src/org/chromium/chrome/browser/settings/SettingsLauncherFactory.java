// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.components.browser_ui.settings.SettingsLauncher;

/** Factory for {@link SettingsLauncher}. Can be used from chrome/browser modules. */
public class SettingsLauncherFactory {
    public static SettingsLauncher createSettingsLauncher() {
        return new SettingsLauncherImpl();
    }
}
