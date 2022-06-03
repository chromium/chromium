// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MEDIA_CLIENT_H_
#define ASH_PUBLIC_CPP_MEDIA_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// This delegate allows the UI code in ash to forward UI commands.
class ASH_PUBLIC_EXPORT MediaClient {
 public:
  // Handles the Next Track Media shortcut key.
  virtual void HandleMediaNextTrack() = 0;

  // Handles the Play/Pause Toggle Media shortcut key.
  virtual void HandleMediaPlayPause() = 0;

  // Handles the Play Media shortcut key.
  virtual void HandleMediaPlay() = 0;

  // Handles the Pause Media shortcut key.
  virtual void HandleMediaPause() = 0;

  // Handles the Stop Media shortcut key.
  virtual void HandleMediaStop() = 0;

  // Handles the Previous Track Media shortcut key.
  virtual void HandleMediaPrevTrack() = 0;

  // Handles the Seek Backward Media shortcut key.
  virtual void HandleMediaSeekBackward() = 0;

  // Handles the Seek Forward Media shortcut key.
  virtual void HandleMediaSeekForward() = 0;

  // Requests that the client resends the NotifyMediaCaptureChanged() message.
  virtual void RequestCaptureState() = 0;

  // Suspends all WebContents-associated media sessions to stop managed players.
  virtual void SuspendMediaSessions() = 0;

 protected:
  virtual ~MediaClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MEDIA_CLIENT_H_
