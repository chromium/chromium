// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CAMERA_PRESENCE_NOTIFIER_H_
#define CHROME_BROWSER_ASH_CAMERA_PRESENCE_NOTIFIER_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace ash {

// Monitors Camera sources. Establishes connection to source on creation. Fires
// callbacks on state changes after Start() is called until Stop().
class CameraPresenceNotifier {
 public:
  // |callback| for notification of camera presence changes. Only one
  // client may monitor per instance.
  using CameraPresenceCallback = base::RepeatingCallback<void(bool)>;
  explicit CameraPresenceNotifier(CameraPresenceCallback callback);

  CameraPresenceNotifier(const CameraPresenceNotifier&) = delete;
  CameraPresenceNotifier& operator=(const CameraPresenceNotifier&) = delete;

  ~CameraPresenceNotifier();

  // Start polling for camera presence changes. A callback always fires after
  // Start() is called since the first result is always a change.
  void Start();

  // Stop polling for camera presence changes. |callback| will not be run after
  // this is called until Start() is called again. If Start() has not been
  // called, this is a nop.
  void Stop();

 private:
  // The system starts in kStopped then progresses to kFirstRun then kStarted.
  // Moving to kStopped restarts the process.
  enum class State { kStopped, kFirstRun, kStarted };

  void VideoSourceProviderDisconnectHandler();

  // Checks for camera device presence.
  void CheckCameraPresence();

  // Checks for camera presence after getting video source information.
  void OnGotSourceInfos(
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  // Result of the last presence check.
  bool camera_present_on_last_check_;

  // Callback for presence check results.
  CameraPresenceCallback callback_;

  // Timer for camera check cycle.
  base::RepeatingTimer camera_check_timer_;

  State state_ = State::kStopped;

  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CameraPresenceNotifier> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CAMERA_PRESENCE_NOTIFIER_H_
