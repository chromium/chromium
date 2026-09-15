// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import android.content.Context;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;

/** DevToolsWindowAndroid provides an interface to control DevTools windows. */
@NullMarked
public class DevToolsWindowAndroid {
    // Make this class's ctor private to prevent it from being instantiated. This class has no
    // non-static method.
    private DevToolsWindowAndroid() {}

    public static boolean isDevToolsAvailable(Context context) {
        return ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    public static boolean isDevToolsAllowedFor(
            Context context, Profile profile, WebContents webContents) {
        return isDevToolsAvailable(context)
                && DevToolsWindowAndroidJni.get().isDevToolsAllowedFor(profile, webContents);
    }

    public static boolean canViewSource(Context context, Profile profile, WebContents webContents) {
        // Disallow ViewSource if DevTools are disabled.
        return isDevToolsAllowedFor(context, profile, webContents)
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

    /**
     * Attaches the DevTools frontend web contents to the browser window.
     *
     * @param webContents DevTools frontend web contents.
     * @param nativeBrowserWindowPtr Browser window to host the DevTools frontend.
     */
    public static void attachToBrowser(WebContents webContents, long nativeBrowserWindowPtr) {
        DevToolsWindowAndroidJni.get().attachToBrowser(webContents, nativeBrowserWindowPtr);
    }

    @NativeMethods
    interface Natives {
        void openDevTools(WebContents webContents);

        boolean isDevToolsAllowedFor(Profile profile, WebContents webContents);

        void attachToBrowser(WebContents webContents, long nativeBrowserWindowPtr);
    }
}
