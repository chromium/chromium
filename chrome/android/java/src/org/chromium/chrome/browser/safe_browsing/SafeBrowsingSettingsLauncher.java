// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.content_public.browser.WebContents;

/**
 * Bridge between Java and native SafeBrowsing code to launch the Safe Browsing settings page.
 */
public class SafeBrowsingSettingsLauncher {
    private SafeBrowsingSettingsLauncher() {}

    @CalledByNative
    private static void showSafeBrowsingSettings(
            WebContents webContents, @SettingsAccessPoint int accessPoint) {
        // TODO
    }
}
