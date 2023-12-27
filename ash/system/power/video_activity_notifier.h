// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_VIDEO_ACTIVITY_NOTIFIER_H_
#define ASH_SYSTEM_POWER_VIDEO_ACTIVITY_NOTIFIER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/video_detector.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace ash {

// Notifies the power manager when a video is playing.
class ASH_EXPORT VideoActivityNotifier : public VideoDetector::Observer,
                                         public SessionObserver {
 public:
  explicit VideoActivityNotifier(VideoDetector* detector);

  VideoActivityNotifier(const VideoActivityNotifier&) = delete;
  VideoActivityNotifier& operator=(const VideoActivityNotifier&) = delete;

  ~VideoActivityNotifier() override;

  // VideoDetector::Observer implementation.
  void OnVideoStateChanged(VideoDetector::State state) override;

  // SessionObserver implementation.
  void OnLockStateChanged(bool locked) override;

  // If |notify_timer_| is running, calls MaybeNotifyPowerManager() and returns
  // true. Returns false otherwise.
  [[nodiscard]] bool TriggerTimeoutForTest();

 private:
  bool should_notify_power_manager() {
    return video_state_ != VideoDetector::State::NOT_PLAYING &&
           !screen_is_locked_;
  }

  // Starts or stops |notify_timer_| as needed.
  void UpdateTimer();

  // Notifies powerd about video activity if should_notify_power_manager() is
  // true.
  void MaybeNotifyPowerManager();

  raw_ptr<VideoDetector> detector_;  // not owned

  // Most-recently-observed video state.
  VideoDetector::State video_state_;

  // True if the screen is currently locked.
  bool screen_is_locked_;

  // Periodically calls MaybeNotifyPowerManager() while
  // should_notify_power_manager() is true.
  base::RepeatingTimer notify_timer_;

  ScopedSessionObserver scoped_session_observer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_VIDEO_ACTIVITY_NOTIFIER_H_
