// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MEDIA_CONTROLLER_H_
#define ASH_PUBLIC_CPP_MEDIA_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scoped_singleton_resetter_for_test.h"
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
  using ScopedResetterForTest = ScopedSingletonResetterForTest<MediaController>;

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
  // Called when VMs' media capture notifications change. Each VM can have 0 or
  // 1 media notification. It can either be a "camera", "mic", or "camera and
  // mic" notification. Each of the argument is true if a notification of the
  // corresponding type is active.
  //
  // There is no `AccountId` in the argument because only the primary
  // account/profile can launch a VM.
  virtual void NotifyVmMediaNotificationState(bool camera,
                                              bool mic,
                                              bool camera_and_mic) = 0;

 protected:
  MediaController();
  virtual ~MediaController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MEDIA_CONTROLLER_H_
