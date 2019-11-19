// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// How long to wait before attempting to re-establish a lost connection.
constexpr base::TimeDelta kReEstablishConnectionDelay =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace

VideoDetector::VideoDetector()
    : state_(State::NOT_PLAYING),
      video_is_playing_(false),
      is_shutting_down_(false) {
  aura::Env::GetInstance()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  EstablishConnectionToViz();
}

VideoDetector::~VideoDetector() {
  Shell::Get()->RemoveShellObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

void VideoDetector::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VideoDetector::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VideoDetector::OnWindowInitialized(aura::Window* window) {
  window_observer_manager_.Add(window);
}

void VideoDetector::OnWindowDestroying(aura::Window* window) {
  if (fullscreen_desks_containers_.count(window)) {
    window_observer_manager_.Remove(window);
    fullscreen_desks_containers_.erase(window);
    UpdateState();
  }
}

void VideoDetector::OnWindowDestroyed(aura::Window* window) {
  window_observer_manager_.Remove(window);
}

void VideoDetector::OnChromeTerminating() {
  // Stop checking video activity once the shutdown
  // process starts. crbug.com/231696.
  is_shutting_down_ = true;
}

void VideoDetector::OnFullscreenStateChanged(bool is_fullscreen,
                                             aura::Window* container) {
  const bool has_fullscreen_in_container =
      fullscreen_desks_containers_.count(container);
  if (is_fullscreen && !has_fullscreen_in_container) {
    fullscreen_desks_containers_.insert(container);
    if (!window_observer_manager_.IsObserving(container))
      window_observer_manager_.Add(container);
    UpdateState();
  } else if (!is_fullscreen && has_fullscreen_in_container) {
    fullscreen_desks_containers_.erase(container);
    window_observer_manager_.Remove(container);
    UpdateState();
  }
}

void VideoDetector::UpdateState() {
  State new_state = State::NOT_PLAYING;
  if (video_is_playing_) {
    new_state = fullscreen_desks_containers_.empty()
                    ? State::PLAYING_WINDOWED
                    : State::PLAYING_FULLSCREEN;
  }

  if (state_ != new_state) {
    state_ = new_state;
    for (auto& observer : observers_)
      observer.OnVideoStateChanged(state_);
  }
}

void VideoDetector::OnVideoActivityStarted() {
  if (is_shutting_down_)
    return;
  video_is_playing_ = true;
  UpdateState();
}

void VideoDetector::OnVideoActivityEnded() {
  video_is_playing_ = false;
  UpdateState();
}

void VideoDetector::EstablishConnectionToViz() {
  if (receiver_.is_bound())
    receiver_.reset();
  mojo::PendingRemote<viz::mojom::VideoDetectorObserver> observer =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &VideoDetector::OnConnectionError, base::Unretained(this)));
  aura::Env::GetInstance()
      ->context_factory_private()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(std::move(observer));
}

void VideoDetector::OnConnectionError() {
  if (video_is_playing_)
    OnVideoActivityEnded();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VideoDetector::EstablishConnectionToViz,
                     weak_factory_.GetWeakPtr()),
      kReEstablishConnectionDelay);
}

}  // namespace ash
