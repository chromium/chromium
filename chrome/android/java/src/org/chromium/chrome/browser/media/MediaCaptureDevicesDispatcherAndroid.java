// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * Java access point for MediaCaptureDevicesDispatcher, allowing for querying and manipulation of
 * media capture state.
 */
@NullMarked
public class MediaCaptureDevicesDispatcherAndroid {
    public static boolean isCapturingAudio(WebContents webContents) {
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingAudio(webContents);
    }

    public static boolean isCapturingVideo(WebContents webContents) {
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingVideo(webContents);
    }

    public static boolean isCapturingTab(WebContents webContents) {
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingTab(webContents);
    }

    public static boolean isCapturingWindow(WebContents webContents) {
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingWindow(webContents);
    }

    public static boolean isCapturingScreen(WebContents webContents) {
        return MediaCaptureDevicesDispatcherAndroidJni.get().isCapturingScreen(webContents);
    }

    public static void notifyStopped(@Nullable WebContents webContents) {
        if (webContents == null) return;
        MediaCaptureDevicesDispatcherAndroidJni.get().notifyStopped(webContents);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        boolean isCapturingAudio(WebContents webContents);

        boolean isCapturingVideo(WebContents webContents);

        boolean isCapturingTab(WebContents webContents);

        boolean isCapturingWindow(WebContents webContents);

        boolean isCapturingScreen(WebContents webContents);

        void notifyStopped(WebContents webContents);
    }
}
