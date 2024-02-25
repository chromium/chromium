// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VIDEO_DETECTOR_H_
#define ASH_WM_VIDEO_DETECTOR_H_

#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Receives notifications from viz::VideoDetector about whether it is likely
// that a video is being played on screen. If video activity is detected, this
// class will classify it as full screen or windowed.
class ASH_EXPORT VideoDetector : public aura::EnvObserver,
                                 public aura::WindowObserver,
                                 public SessionObserver,
                                 public ShellObserver,
                                 public viz::mojom::VideoDetectorObserver {
 public:
  // State of detected video activity.
  enum class State {
    // Video activity has been detected recently and there are no fullscreen
    // windows.
    PLAYING_WINDOWED,
    // Video activity has been detected recently and there is at least one
    // fullscreen window.
    PLAYING_FULLSCREEN,
    // Video activity has not been detected recently.
    NOT_PLAYING,
  };

  class Observer {
   public:
    // Invoked when the video playback state has changed.
    virtual void OnVideoStateChanged(VideoDetector::State state) = 0;

   protected:
    virtual ~Observer() {}
  };

  VideoDetector();

  VideoDetector(const VideoDetector&) = delete;
  VideoDetector& operator=(const VideoDetector&) = delete;

  ~VideoDetector() override;

  State state() const { return state_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // EnvObserver overrides.
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // SessionStateController overrides.
  void OnChromeTerminating() override;

  // ShellObserver overrides.
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  // viz::mojom::VideoDetectorObserver implementation.
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override;

 private:
  // Updates |state_| and notifies |observers_| if it changed.
  void UpdateState();

  // Connects to Viz and starts observing video activities.
  void EstablishConnectionToViz();

  // Called when connection to Viz is lost. The connection will be
  // re-established after a short delay.
  void OnConnectionError();

  // Current playback state.
  State state_;

  // True if video has been observed in the last |kVideoTimeoutMs|.
  bool video_is_playing_;

  // Currently-fullscreen desks containers windows.
  std::set<raw_ptr<aura::Window, SetExperimental>> fullscreen_desks_containers_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_manager_{this};
  ScopedSessionObserver scoped_session_observer_{this};

  bool is_shutting_down_;

  mojo::Receiver<viz::mojom::VideoDetectorObserver> receiver_{this};

  base::WeakPtrFactory<VideoDetector> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_VIDEO_DETECTOR_H_
