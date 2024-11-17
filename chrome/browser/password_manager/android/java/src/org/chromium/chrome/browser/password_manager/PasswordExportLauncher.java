// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.os.Bundle;

import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;

/** Launches {@link MainSettings} and starts the password export flow. */
public class PasswordExportLauncher {
    // Launch argument for the main settings. If set to to true, the passwords
    // export flow starts immediately.
    public static final String START_PASSWORDS_EXPORT = "start-passwords-export";

    public static void showMainSettingsAndStartExport(Context context) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putBoolean(START_PASSWORDS_EXPORT, true);
        context.startActivity(
                SettingsNavigationFactory.createSettingsNavigation()
                        .createSettingsIntent(context, SettingsFragment.MAIN, fragmentArgs));
    }
}
