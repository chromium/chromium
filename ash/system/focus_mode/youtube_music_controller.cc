// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/youtube_music_controller.h"

#include "ash/shell.h"
#include "base/check.h"
#include "components/account_id/account_id.h"

namespace ash {

YoutubeMusicController::YoutubeMusicController() {
  SessionController* session_controller = SessionController::Get();
  CHECK(session_controller);
  session_controller->AddObserver(this);
}

YoutubeMusicController::~YoutubeMusicController() {
  // TODO(yongshun): Create the delegate.

  SessionController* session_controller = SessionController::Get();
  CHECK(session_controller);
  session_controller->RemoveObserver(this);
}

void YoutubeMusicController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // TODO(yongshun): Notify the delegate to update the client.
}

}  // namespace ash
