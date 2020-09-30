// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.media.MediaContainerName;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Record statistics on interesting cast events and actions.
 */
public class RecordCastAction {
    /**
     * UMA histogram values for the device types the user could select.
     * <p>
     * Keep in sync with enums.xml
     */
    @IntDef({DeviceType.CAST_GENERIC, DeviceType.CAST_YOUTUBE, DeviceType.NON_CAST_YOUTUBE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DeviceType {
        int CAST_GENERIC = 0;
        int CAST_YOUTUBE = 1;
        int NON_CAST_YOUTUBE = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * UMA histogram values for the fullscreen controls the user could tap.
     * <p>
     * Keep in sync with enums.xml
     */
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
     *
     * @param playerType the type of cast receiver.
     */
    public static void remotePlaybackDeviceSelected(@DeviceType int playerType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Cast.Sender.DeviceType", playerType, DeviceType.NUM_ENTRIES);
    }

    /**
     * Record that a remote playback was requested. This is intended to record all playback
     * requests, whether they were user initiated or was an auto-playback resulting from the user
     * selecting the device initially.
     */
    public static void castPlayRequested() {
        RecordUserAction.record("Cast_Sender_CastPlayRequested");
    }

    /**
     * Record the amount of time remaining on the video when the remote playback stops.
     *
     * @param videoLengthMs the total length of the video in milliseconds
     * @param timeRemainingMs the remaining time in the video in milliseconds
     */
    public static void castEndedTimeRemaining(long videoLengthMs, long timeRemainingMs) {
        int percentRemaining = 100;
        if (videoLengthMs > 0) {
            // Get the percentage of video remaining, but bucketize into groups of 10
            // since we don't really need that granular of data.
            percentRemaining = ((int) (10 * timeRemainingMs / videoLengthMs)) * 10;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Cast.Sender.CastTimeRemainingPercentage", percentRemaining, 101);
    }

    /**
     * Record the type of the media being cast.
     *
     * @param mediaType the type of the media being casted, see media/base/container_names.h for
     *            possible media types.
     */
    public static void castMediaType(@MediaContainerName int mediaType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Cast.Sender.CastMediaType", mediaType, MediaContainerName.MAX + 1);
    }

    /**
     * Record if the remotely played media element is alive when the
     * {@link ExpandedControllerActivity} is shown.
     *
     * @param isMediaElementAlive if the media element is alive.
     */
    public static void recordFullscreenControlsShown(boolean isMediaElementAlive) {
        RecordHistogram.recordBooleanHistogram(
                "Cast.Sender.MediaElementPresentWhenShowFullscreenControls", isMediaElementAlive);
    }

    /**
     * Record when an action was taken on the {@link ExpandedControllerActivity} by the user.
     * Notes if the corresponding media element has been alive at that point in time.
     *
     * @param action one of the FULLSCREEN_CONTROLS_* constants defined above.
     * @param isMediaElementAlive if the media element is alive.
     */
    public static void recordFullscreenControlsAction(int action, boolean isMediaElementAlive) {
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
        RecordHistogram.recordPercentageHistogram(
                "Cast.Sender.SessionTimeWithoutMediaElementPercentage", percentage);
    }
}
