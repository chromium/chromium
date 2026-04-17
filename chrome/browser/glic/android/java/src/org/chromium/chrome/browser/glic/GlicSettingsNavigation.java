// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Bridge between Java and native GLIC code to launch GLIC settings. */
@NullMarked
public class GlicSettingsNavigation {
    private GlicSettingsNavigation() {}

    /** Opens the GLIC settings page. */
    @CalledByNative
    private static void showGlicSettings() {
        Context context = ContextUtils.getApplicationContext();

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(context, GlicSettings.class);
    }
}
