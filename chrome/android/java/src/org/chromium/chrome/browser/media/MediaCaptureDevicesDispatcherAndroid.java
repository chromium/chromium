// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * Java access point for MediaCaptureDevicesDispatcher, allowing for querying and manipulation of
 * media capture state.
 */
@NullMarked
public class MediaCaptureDevicesDispatcherAndroid {
    /** Observer for changes in media capture state. */
    public interface Observer {
        /**
         * Called when the tab capture state of a {@link WebContents} changes.
         *
         * @param webContents The {@link WebContents} whose state changed.
         * @param isCapturing Whether tab capture is currently active.
         */
        default void onIsCapturingTabChanged(WebContents webContents, boolean isCapturing) {}
    }

    private static final ObserverList<Observer> sObservers = new ObserverList<>();

    /**
     * Adds an observer to be notified of changes in media capture state.
     *
     * @param observer The observer to add.
     */
    public static void addObserver(Observer observer) {
        sObservers.addObserver(observer);
    }

    /**
     * Removes a previously added observer.
     *
     * @param observer The observer to remove.
     */
    public static void removeObserver(Observer observer) {
        sObservers.removeObserver(observer);
    }

    @CalledByNative
    private static void onIsCapturingTabChanged(WebContents webContents, boolean isCapturing) {
        for (Observer observer : sObservers) {
            observer.onIsCapturingTabChanged(webContents, isCapturing);
        }
    }

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

    public static void notifyDisplayMediaStopped(@Nullable WebContents webContents) {
        if (webContents == null) return;
        MediaCaptureDevicesDispatcherAndroidJni.get().notifyDisplayMediaStopped(webContents);
    }

    public static void notifyTabCapturingStopped(@Nullable WebContents webContents) {
        if (webContents == null) return;
        MediaCaptureDevicesDispatcherAndroidJni.get().notifyTabCapturingStopped(webContents);
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

        void notifyDisplayMediaStopped(WebContents webContents);

        void notifyTabCapturingStopped(WebContents webContents);
    }
}
