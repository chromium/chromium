// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.util.ArrayMap;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.Map;

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
    private static final Map<WebContents, WebContentsObserver> sSourceSwitchingWebContents =
            new ArrayMap<>();

    /**
     * Sets whether a tab sharing source switch is actively in progress for the capturer tab. When a
     * tab sharing session switches source to a different tab, native WebRTC stream handoff (via
     * MediaStreamManager::ChangeMediaStreamSourceFromBrowser) stops the old desktop capture device
     * before starting a new capture device for the new tab. Suppressing this transient stop
     * callback prevents Android MediaProjection FGS tokens or toolbar UI from being torn down.
     *
     * @param capturer The capturer {@link WebContents}.
     * @param isSwitching Whether source switching is in progress.
     */
    public static void setSourceSwitchingInProgress(
            @Nullable WebContents capturer, boolean isSwitching) {
        if (capturer == null) return;
        if (isSwitching) {
            if (sSourceSwitchingWebContents.containsKey(capturer)) return;
            WebContentsObserver observer =
                    new WebContentsObserver(capturer) {
                        @Override
                        public void webContentsDestroyed() {
                            setSourceSwitchingInProgress(capturer, false);
                        }
                    };
            sSourceSwitchingWebContents.put(capturer, observer);
        } else {
            WebContentsObserver observer = sSourceSwitchingWebContents.remove(capturer);
            if (observer != null) {
                observer.observe(null);
            }
            // If switching ended without a stream resuming (e.g. timeout or abort), ensure
            // observers sync with the stopped state.
            if (!isCapturingTab(capturer)) {
                for (Observer obs : sObservers) {
                    obs.onIsCapturingTabChanged(capturer, false);
                }
            }
        }
    }

    /**
     * Returns whether a tab sharing source switch is currently in progress for {@code capturer}.
     * Lets UI distinguish the transient stop/start emitted while WebRTC renegotiates the source
     * from a genuine session teardown, so the toolbar can be swapped in place rather than torn down
     * and re-created.
     *
     * @param capturer The capturer {@link WebContents}.
     * @return True if switching is in progress.
     */
    public static boolean isSourceSwitchingInProgress(@Nullable WebContents capturer) {
        if (capturer == null) return false;
        return sSourceSwitchingWebContents.containsKey(capturer);
    }

    /**
     * Adds an observer to receive notifications when tab capture starts or stops.
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

    @VisibleForTesting
    @CalledByNative
    static void onIsCapturingTabChanged(
            @JniType("content::WebContents*") WebContents webContents, boolean isCapturing) {
        if (webContents != null && sSourceSwitchingWebContents.containsKey(webContents)) {
            if (!isCapturing) {
                // Suppress the transient stop signal emitted when native stream handoff
                // stops the old desktop capture device prior to starting the new one.
                return;
            }
            // When capturing resumes on the new stream, the atomic handoff is complete.
            setSourceSwitchingInProgress(webContents, false);
        }
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
        setSourceSwitchingInProgress(webContents, false);
        MediaCaptureDevicesDispatcherAndroidJni.get().notifyStopped(webContents);
    }

    public static void notifyDisplayMediaStopped(@Nullable WebContents webContents) {
        if (webContents == null) return;
        setSourceSwitchingInProgress(webContents, false);
        MediaCaptureDevicesDispatcherAndroidJni.get().notifyDisplayMediaStopped(webContents);
    }

    public static void notifyTabCapturingStopped(@Nullable WebContents webContents) {
        if (webContents == null) return;
        setSourceSwitchingInProgress(webContents, false);
        TabSharingUiManager.getInstance().stopSharingByCapturerTab(webContents);
    }

    public static boolean shouldFilterWebContents(
            @Nullable WebContents capturer, @Nullable WebContents target) {
        if (capturer == null || target == null) return true;
        return MediaCaptureDevicesDispatcherAndroidJni.get()
                .shouldFilterWebContents(capturer, target);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        boolean isCapturingAudio(@JniType("content::WebContents*") WebContents webContents);

        boolean isCapturingVideo(@JniType("content::WebContents*") WebContents webContents);

        boolean isCapturingTab(@JniType("content::WebContents*") WebContents webContents);

        boolean isCapturingWindow(@JniType("content::WebContents*") WebContents webContents);

        boolean isCapturingScreen(@JniType("content::WebContents*") WebContents webContents);

        void notifyStopped(@JniType("content::WebContents*") WebContents webContents);

        void notifyDisplayMediaStopped(@JniType("content::WebContents*") WebContents webContents);

        boolean shouldFilterWebContents(
                @JniType("content::WebContents*") WebContents capturer,
                @JniType("content::WebContents*") WebContents target);
    }
}
