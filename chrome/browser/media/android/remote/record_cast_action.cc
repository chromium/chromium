// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/android/chrome_jni_headers/RecordCastAction_jni.h"
#include "media/base/container_names.h"

using base::android::JavaParamRef;

namespace {

// When updating these values, remember to also update
// tools/histograms/histograms.xml.
enum CastPlayBackState {
  YT_PLAYER_SUCCESS = 0,
  YT_PLAYER_FAILURE = 1,
  DEFAULT_PLAYER_SUCCESS = 2,
  DEFAULT_PLAYER_FAILURE = 3,
  CAST_PLAYBACK_STATE_COUNT = 4
};

// When updating these values, remember to also update
// tools/histograms/histograms.xml.

// This is actually a misnomer, it should be RemotePlaybackPlayerType, but it is
// more important that it matches the histogram name in histograms.xml.
// TODO(aberent) Change this once we are upstream, when can change it both here
// and in histogram.xml in the same CL.
enum RemotePlaybackDeviceType {
  CAST_GENERIC = 0,
  CAST_YOUTUBE = 1,
  NON_CAST_YOUTUBE = 2,
  REMOTE_PLAYBACK_DEVICE_TYPE_COUNT = 3
};

}  // namespace

namespace remote_media {
static void JNI_RecordCastAction_RecordRemotePlaybackDeviceSelected(
    JNIEnv*,
    jint device_type) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Sender.DeviceType",
                            static_cast<RemotePlaybackDeviceType>(device_type),
                            REMOTE_PLAYBACK_DEVICE_TYPE_COUNT);
}

static void JNI_RecordCastAction_RecordCastPlayRequested(JNIEnv*) {
  base::RecordAction(base::UserMetricsAction("Cast_Sender_CastPlayRequested"));
}

static void JNI_RecordCastAction_RecordCastDefaultPlayerResult(
    JNIEnv*,
    jboolean cast_success) {
  if (cast_success) {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult",
                              DEFAULT_PLAYER_SUCCESS,
                              CAST_PLAYBACK_STATE_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult",
                              DEFAULT_PLAYER_FAILURE,
                              CAST_PLAYBACK_STATE_COUNT);
  }
}

static void JNI_RecordCastAction_RecordCastYouTubePlayerResult(
    JNIEnv*,
    jboolean cast_success) {
  if (cast_success) {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult", YT_PLAYER_SUCCESS,
                              CAST_PLAYBACK_STATE_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult", YT_PLAYER_FAILURE,
                              CAST_PLAYBACK_STATE_COUNT);
  }
}

static void JNI_RecordCastAction_RecordCastMediaType(
    JNIEnv*,
    jint media_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Cast.Sender.CastMediaType",
      static_cast<media::container_names::MediaContainerName>(media_type),
      media::container_names::CONTAINER_MAX + 1);
}

static void JNI_RecordCastAction_RecordCastEndedTimeRemaining(
    JNIEnv*,
    jint video_total_time,
    jint time_left_in_video) {
  int percent_remaining = 100;
  if (video_total_time > 0) {
    // Get the percentage of video remaining, but bucketize into groups of 10
    // since we don't really need that granular of data.
    percent_remaining = static_cast<int>(
        10.0 * time_left_in_video / video_total_time) * 10;
  }

  UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastTimeRemainingPercentage",
      percent_remaining, 101);
}

}  // namespace remote_media
