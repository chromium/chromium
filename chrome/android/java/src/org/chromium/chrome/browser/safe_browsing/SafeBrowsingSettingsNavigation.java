// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.base.WindowAndroid;

/** Bridge between Java and native SafeBrowsing code to launch the Safe Browsing settings page. */
public class SafeBrowsingSettingsNavigation {
    private SafeBrowsingSettingsNavigation() {}

    @CalledByNative
    private static void showSafeBrowsingSettings(
            WindowAndroid window, @SettingsAccessPoint int accessPoint) {
        if (window == null) return;
        Context currentContext = window.getContext().get();
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(
                currentContext,
                SafeBrowsingSettingsFragment.class,
                SafeBrowsingSettingsFragment.createArguments(accessPoint));
    }
}
