// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/remote/flinging_controller_bridge.h"

#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/features/media_router/jni_headers/FlingingControllerBridge_jni.h"
#include "chrome/android/features/media_router/jni_headers/MediaStatusBridge_jni.h"

namespace media_router {

// From Android MediaStatus documentation.
// https://developers.google.com/android/reference/com/google/android/gms/cast/MediaStatus.html
const int PLAYER_STATE_UNKOWN = 0;
const int PLAYER_STATE_IDLE = 1;
const int PLAYER_STATE_PLAYING = 2;
const int PLAYER_STATE_PAUSED = 3;
const int PLAYER_STATE_BUFFERING = 4;
const int IDLE_REASON_FINISHED = 1;

FlingingControllerBridge::FlingingControllerBridge(
    base::android::ScopedJavaGlobalRef<jobject> controller)
    : j_flinging_controller_bridge_(controller) {}

FlingingControllerBridge::~FlingingControllerBridge() = default;

void FlingingControllerBridge::Play() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_play(env, j_flinging_controller_bridge_);
}

void FlingingControllerBridge::Pause() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_pause(env, j_flinging_controller_bridge_);
}

void FlingingControllerBridge::SetMute(bool mute) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_setMute(env, j_flinging_controller_bridge_,
                                        mute);
}

void FlingingControllerBridge::SetVolume(float volume) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_setVolume(env, j_flinging_controller_bridge_,
                                          volume);
}

void FlingingControllerBridge::Seek(base::TimeDelta time) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_seek(env, j_flinging_controller_bridge_,
                                     time.InMilliseconds());
}

media::MediaController* FlingingControllerBridge::GetMediaController() {
  return this;
}

void FlingingControllerBridge::AddMediaStatusObserver(
    media::MediaStatusObserver* observer) {
  DCHECK(!observer_);
  observer_ = observer;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_addNativeFlingingController(
      env, j_flinging_controller_bridge_, reinterpret_cast<intptr_t>(this));
}

void FlingingControllerBridge::RemoveMediaStatusObserver(
    media::MediaStatusObserver* observer) {
  DCHECK_EQ(observer_, observer);
  observer_ = nullptr;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_FlingingControllerBridge_clearNativeFlingingController(
      env, j_flinging_controller_bridge_);
}

void FlingingControllerBridge::OnMediaStatusUpdated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_bridge,
    const base::android::JavaParamRef<jobject>& j_status) {
  if (!observer_)
    return;

  media::MediaStatus status;

  int player_state = Java_MediaStatusBridge_playerState(env, j_status);

  switch (player_state) {
    case PLAYER_STATE_UNKOWN:
      status.state = media::MediaStatus::State::UNKNOWN;
      break;
    case PLAYER_STATE_PLAYING:
      status.state = media::MediaStatus::State::PLAYING;
      break;
    case PLAYER_STATE_PAUSED:
      status.state = media::MediaStatus::State::PAUSED;
      break;
    case PLAYER_STATE_BUFFERING:
      status.state = media::MediaStatus::State::BUFFERING;
      break;
    case PLAYER_STATE_IDLE:
      status.state = media::MediaStatus::State::STOPPED;
      int idle_reason = Java_MediaStatusBridge_idleReason(env, j_status);
      status.reached_end_of_stream = (idle_reason == IDLE_REASON_FINISHED);
      break;
  }

  status.title = base::android::ConvertJavaStringToUTF8(
      env, Java_MediaStatusBridge_title(env, j_status));
  status.can_play_pause = Java_MediaStatusBridge_canPlayPause(env, j_status);
  status.can_mute = Java_MediaStatusBridge_canMute(env, j_status);
  status.can_set_volume = Java_MediaStatusBridge_canSetVolume(env, j_status);
  status.can_seek = Java_MediaStatusBridge_canSeek(env, j_status);
  status.is_muted = Java_MediaStatusBridge_isMuted(env, j_status);
  status.volume = Java_MediaStatusBridge_volume(env, j_status);
  status.duration = base::TimeDelta::FromMilliseconds(
      Java_MediaStatusBridge_duration(env, j_status));
  status.current_time = base::TimeDelta::FromMilliseconds(
      Java_MediaStatusBridge_currentTime(env, j_status));

  observer_->OnMediaStatusUpdated(status);
}

base::TimeDelta FlingingControllerBridge::GetApproximateCurrentTime() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  long time_in_ms = Java_FlingingControllerBridge_getApproximateCurrentTime(
      env, j_flinging_controller_bridge_);

  return base::TimeDelta::FromMilliseconds(time_in_ms);
}

}  // namespace media_router
