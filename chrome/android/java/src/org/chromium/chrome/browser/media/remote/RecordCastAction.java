// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Record statistics on interesting cast events and actions.
 */
@JNINamespace("remote_media")
public class RecordCastAction {
    // UMA histogram values for the device types the user could select.
    // Keep in sync with the enum in uma_record_action.cc
    @IntDef({DeviceType.CAST_GENERIC, DeviceType.CAST_YOUTUBE, DeviceType.NON_CAST_YOUTUBE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DeviceType {
        int CAST_GENERIC = 0;
        int CAST_YOUTUBE = 1;
        int NON_CAST_YOUTUBE = 2;
        int NUM_ENTRIES = 3;
    }

    // UMA histogram values for the fullscreen controls the user could tap.
    // Keep in sync with the MediaCommand enum in histograms.xml
    @IntDef({FullScreenControls.RESUME, FullScreenControls.PAUSE, FullScreenControls.SEEK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FullScreenControls {
        int RESUME = 0;
        int PAUSE = 1;
        int SEEK = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Record the type of cast receiver we to which we are casting.
     * @param playerType the type of cast receiver.
     */
    public static void remotePlaybackDeviceSelected(@RecordCastAction.DeviceType int playerType) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordCastActionJni.get().recordRemotePlaybackDeviceSelected(playerType);
        }
    }

    /**
     * Record that a remote playback was requested. This is intended to record all playback
     * requests, whether they were user initiated or was an auto-playback resulting from the user
     * selecting the device initially.
     */
    public static void castPlayRequested() {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordCastActionJni.get().recordCastPlayRequested();
        }
    }

    /**
     * Record the result of the cast playback request.
     *
     * @param castSucceeded true if the playback succeeded, false if there was an error
     */
    public static void castDefaultPlayerResult(boolean castSucceeded) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordCastActionJni.get().recordCastDefaultPlayerResult(castSucceeded);
        }
    }

    /**
     * Record the result of casting a YouTube video.
     *
     * @param castSucceeded true if the playback succeeded, false if there was an error
     */
    public static void castYouTubePlayerResult(boolean castSucceeded) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordCastActionJni.get().recordCastYouTubePlayerResult(castSucceeded);
        }
    }

    /**
     * Record the amount of time remaining on the video when the remote playback stops.
     *
     * @param videoLengthMs the total length of the video in milliseconds
     * @param timeRemainingMs the remaining time in the video in milliseconds
     */
    public static void castEndedTimeRemaining(long videoLengthMs, long timeRemainingMs) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordCastActionJni.get().recordCastEndedTimeRemaining(
                    (int) videoLengthMs, (int) timeRemainingMs);
        }
    }

    /**
     * Record the type of the media being cast.
     *
     * @param mediaType the type of the media being casted, see media/base/container_names.h for
     *            possible media types.
     */
    public static void castMediaType(int mediaType) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordCastActionJni.get().recordCastMediaType(mediaType);
        }
    }

    /**
     * Record if the remotely played media element is alive when the
     * {@link ExpandedControllerActivity} is shown.
     *
     * @param isMediaElementAlive if the media element is alive.
     */
    public static void recordFullscreenControlsShown(boolean isMediaElementAlive) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordHistogram.recordBooleanHistogram(
                    "Cast.Sender.MediaElementPresentWhenShowFullscreenControls",
                    isMediaElementAlive);
        }
    }

    /**
     * Record when an action was taken on the {@link ExpandedControllerActivity} by the user.
     * Notes if the corresponding media element has been alive at that point in time.
     *
     * @param action one of the FULLSCREEN_CONTROLS_* constants defined above.
     * @param isMediaElementAlive if the media element is alive.
     */
    public static void recordFullscreenControlsAction(int action, boolean isMediaElementAlive) {
        if (!LibraryLoader.getInstance().isInitialized()) return;

        if (isMediaElementAlive) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Cast.Sender.FullscreenControlsActionWithMediaElement", action,
                    FullScreenControls.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Cast.Sender.FullscreenControlsActionWithoutMediaElement", action,
                    FullScreenControls.NUM_ENTRIES);
        }
    }

    /**
     * Record the ratio of the time the media element was detached from the remote playback session
     * to the total duration of the session (as from when the element has been attached till when
     * the session stopped or disconnected), in percents.
     *
     * @param percentage The ratio in percents.
     */
    public static void recordRemoteSessionTimeWithoutMediaElementPercentage(int percentage) {
        if (LibraryLoader.getInstance().isInitialized()) {
            RecordHistogram.recordPercentageHistogram(
                    "Cast.Sender.SessionTimeWithoutMediaElementPercentage", percentage);
        }
    }

    @NativeMethods
    interface Natives {
        // Cast sending
        void recordRemotePlaybackDeviceSelected(int deviceType);

        void recordCastPlayRequested();
        void recordCastDefaultPlayerResult(boolean castSucceeded);
        void recordCastYouTubePlayerResult(boolean castSucceeded);
        void recordCastEndedTimeRemaining(int videoLengthMs, int timeRemainingMs);
        void recordCastMediaType(int mediaType);
    }
}
