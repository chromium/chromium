// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;

/** DevToolsWindowAndroid provides an interface to control DevTools windows. */
@NullMarked
public class DevToolsWindowAndroid {
    // Make this class's ctor private to prevent it from being instantiated. This class has no
    // non-static method.
    private DevToolsWindowAndroid() {}

    public static boolean isDevToolsAllowedFor(Profile profile, WebContents webContents) {
        return ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)
                && DevToolsWindowAndroidJni.get().isDevToolsAllowedFor(profile, webContents);
    }

    public static boolean canViewSource(Profile profile, WebContents webContents) {
        // Disallow ViewSource if DevTools are disabled.
        return isDevToolsAllowedFor(profile, webContents)
                && webContents.getNavigationController().canViewSource();
    }

    /**
     * Opens a DevTools window for the given WebContents.
     *
     * @param webContents The web contents to be inspected by DevTools.
     */
    public static void openDevTools(WebContents webContents) {
        DevToolsWindowAndroidJni.get().openDevTools(webContents);
    }

    @NativeMethods
    interface Natives {
        void openDevTools(WebContents webContents);

        boolean isDevToolsAllowedFor(Profile profile, WebContents webContents);
    }
}
