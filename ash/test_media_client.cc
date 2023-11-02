// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test_media_client.h"

namespace ash {

TestMediaClient::TestMediaClient() = default;
TestMediaClient::~TestMediaClient() = default;

void TestMediaClient::HandleMediaNextTrack() {
  ++handle_media_next_track_count_;
}

void TestMediaClient::HandleMediaPlayPause() {
  ++handle_media_play_pause_count_;
}

void TestMediaClient::HandleMediaPlay() {
  ++handle_media_play_count_;
}

void TestMediaClient::HandleMediaPause() {
  ++handle_media_pause_count_;
}

void TestMediaClient::HandleMediaStop() {
  ++handle_media_stop_count_;
}

void TestMediaClient::HandleMediaPrevTrack() {
  ++handle_media_prev_track_count_;
}

void TestMediaClient::HandleMediaSeekBackward() {
  ++handle_media_seek_backward_count_;
}

void TestMediaClient::HandleMediaSeekForward() {
  ++handle_media_seek_forward_count_;
}

void TestMediaClient::RequestCaptureState() {}

void TestMediaClient::SuspendMediaSessions() {
  media_sessions_suspended_ = true;
}

}  // namespace ash
