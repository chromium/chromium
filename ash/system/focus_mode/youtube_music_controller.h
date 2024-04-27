// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"

class AccountId;

namespace ash {

// Provides access to the Youtube Music API for the active account through
// `YoutubeMusicDelegate`.
class ASH_EXPORT YoutubeMusicController : public SessionObserver {
 public:
  YoutubeMusicController();
  YoutubeMusicController(const YoutubeMusicController&) = delete;
  YoutubeMusicController& operator=(const YoutubeMusicController&) = delete;
  ~YoutubeMusicController() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_YOUTUBE_MUSIC_CONTROLLER_H_
