// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Java access point for MediaCaptureDevicesDispatcher, allowing for querying and manipulation of
 * media capture state.
 */
public class MediaCaptureDevicesDispatcherAndroid {
    public static boolean isCapturingAudio(WebContents webContents) {
        if (webContents == null) return false;
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingAudio(webContents);
    }

    public static boolean isCapturingVideo(WebContents webContents) {
        if (webContents == null) return false;
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingVideo(webContents);
    }

    public static boolean isCapturingScreen(WebContents webContents) {
        if (webContents == null) return false;
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingScreen(webContents);
    }

    public static void notifyStopped(WebContents webContents) {
        if (webContents == null) return;
        MediaCaptureDevicesDispatcherAndroidJni.get().notifyStopped(webContents);
    }

    @NativeMethods
    interface Natives {
        boolean isCapturingAudio(WebContents webContents);
        boolean isCapturingVideo(WebContents webContents);
        boolean isCapturingScreen(WebContents webContents);
        void notifyStopped(WebContents webContents);
    }
}