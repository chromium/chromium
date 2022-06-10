// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CAMERA_PRESENCE_NOTIFIER_H_
#define CHROME_BROWSER_ASH_CAMERA_PRESENCE_NOTIFIER_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace ash {

// Camera presence status dispatcher.
// To use the dispatcher, make an observer of CameraPresenceNotifier.
//
// Usage:
// base::ScopedObservation<ash::CameraPresenceNotifier,
//                         ash::CameraPresenceNotifier::Observer>
//     camera_observer_{this};
// camera_observer_.Observe(ash::CameraPresenceNotifier::GetInstance());

class CameraPresenceNotifier {
 public:
  class Observer {
   public:
    virtual void OnCameraPresenceCheckDone(bool is_camera_present) = 0;
   protected:
    virtual ~Observer() {}
  };

  static CameraPresenceNotifier* GetInstance();

  CameraPresenceNotifier(const CameraPresenceNotifier&) = delete;
  CameraPresenceNotifier& operator=(const CameraPresenceNotifier&) = delete;

  void AddObserver(CameraPresenceNotifier::Observer* observer);
  void RemoveObserver(CameraPresenceNotifier::Observer* observer);

 private:
  friend struct base::DefaultSingletonTraits<CameraPresenceNotifier>;
  CameraPresenceNotifier();

  void VideoSourceProviderDisconnectHandler();

  ~CameraPresenceNotifier();

  // Checks asynchronously for camera device presence.
  void CheckCameraPresence();

  // Gets Video sources and checks camera presence.
  void CheckPresenceOnUIThread();

  // Checks for camera presence after getting video source information.
  void OnGotSourceInfos(
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  // Result of the last presence check.
  bool camera_present_on_last_check_;

  // Timer for camera check cycle.
  base::RepeatingTimer camera_check_timer_;

  base::ObserverList<Observer>::Unchecked observers_;

  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_remote_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
using ::ash::CameraPresenceNotifier;
}

#endif  // CHROME_BROWSER_ASH_CAMERA_PRESENCE_NOTIFIER_H_
