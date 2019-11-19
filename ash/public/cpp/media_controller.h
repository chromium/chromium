// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MEDIA_CONTROLLER_H_
#define ASH_PUBLIC_CPP_MEDIA_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/containers/flat_map.h"
#include "components/account_id/account_id.h"

namespace ash {

class MediaClient;

// Describes whether media is currently being captured.
enum class MediaCaptureState {
  kNone = 0,
  kAudio = 1,
  kVideo = 2,
  kAudioVideo = 3,
};

class ASH_PUBLIC_EXPORT MediaController {
 public:
  // Helper class to reset ShutdowController instance in constructor and
  // restore it in destructor so that tests could create its own instance.
  class ScopedResetterForTest {
   public:
    ScopedResetterForTest();
    ~ScopedResetterForTest();

   private:
    MediaController* const instance_;
  };

  // Gets the singleton MediaController instance.
  static MediaController* Get();

  // Sets the client.
  virtual void SetClient(MediaClient* client) = 0;

  // Forces media shortcut key handling in MediaClient instead of in ash. This
  // defaults to false and will be reset if the client encounters an error.
  virtual void SetForceMediaClientKeyHandling(bool enabled) = 0;

  // Called when the media capture state changes on the client, or in response
  // to a RequestCaptureState() request. Returns a map from AccountId to
  // MediaCaptureState representing every user's state.
  virtual void NotifyCaptureState(
      const base::flat_map<AccountId, MediaCaptureState>& capture_states) = 0;

 protected:
  MediaController();
  virtual ~MediaController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MEDIA_CONTROLLER_H_
