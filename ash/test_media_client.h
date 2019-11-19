// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_MEDIA_CLIENT_H_
#define ASH_TEST_MEDIA_CLIENT_H_

#include "ash/public/cpp/media_client.h"
#include "base/macros.h"

namespace ash {

// Implement MediaClient mojo interface to simulate chrome behavior in tests.
// This breaks the ash/chrome dependency to allow testing ash code in isolation.
class TestMediaClient : public MediaClient {
 public:
  TestMediaClient();
  ~TestMediaClient() override;

  // MediaClient:
  void HandleMediaNextTrack() override;
  void HandleMediaPlayPause() override;
  void HandleMediaPrevTrack() override;
  void RequestCaptureState() override;
  void SuspendMediaSessions() override;

  int handle_media_next_track_count() const {
    return handle_media_next_track_count_;
  }
  int handle_media_play_pause_count() const {
    return handle_media_play_pause_count_;
  }
  int handle_media_prev_track_count() const {
    return handle_media_prev_track_count_;
  }
  bool media_sessions_suspended() const { return media_sessions_suspended_; }

 private:
  int handle_media_next_track_count_ = 0;
  int handle_media_play_pause_count_ = 0;
  int handle_media_prev_track_count_ = 0;
  bool media_sessions_suspended_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestMediaClient);
};

}  // namespace ash

#endif  // ASH_TEST_MEDIA_CLIENT_H_
