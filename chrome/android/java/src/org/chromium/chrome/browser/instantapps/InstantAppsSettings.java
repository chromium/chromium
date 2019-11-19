// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * A bridge class for retrieving Instant Apps-related settings.
 */
public class InstantAppsSettings {

    /**
     * Check whether the instant app at the given url should be opened by default.
     */
    public static boolean isInstantAppDefault(WebContents webContents, String url) {
        return InstantAppsSettingsJni.get().getInstantAppDefault(webContents, url);
    }

    /**
     * Remember that the instant app at the given url should be opened by default.
     */
    public static void setInstantAppDefault(WebContents webContents, String url) {
        InstantAppsSettingsJni.get().setInstantAppDefault(webContents, url);
    }

    /**
     * Check whether the banner promoting an instant app should be shown.
     */
    public static boolean shouldShowBanner(WebContents webContents, String url) {
        return InstantAppsSettingsJni.get().shouldShowBanner(webContents, url);
    }

    @NativeMethods
    interface Natives {
        boolean getInstantAppDefault(WebContents webContents, String url);
        void setInstantAppDefault(WebContents webContents, String url);
        boolean shouldShowBanner(WebContents webContents, String url);
    }
}
