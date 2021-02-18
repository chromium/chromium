// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_throttler/frame_throttling_controller.h"

#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

void CollectFrameSinkIds(const aura::Window* window,
                         std::vector<viz::FrameSinkId>* frame_sink_ids) {
  if (window->GetFrameSinkId().is_valid()) {
    frame_sink_ids->push_back(window->GetFrameSinkId());
    return;
  }
  for (auto* child : window->children()) {
    CollectFrameSinkIds(child, frame_sink_ids);
  }
}

void CollectBrowserFrameSinkIds(const std::vector<aura::Window*>& windows,
                                std::vector<viz::FrameSinkId>* frame_sink_ids) {
  for (auto* window : windows) {
    if (ash::AppType::BROWSER == static_cast<ash::AppType>(window->GetProperty(
                                     aura::client::kAppType))) {
      CollectFrameSinkIds(window, frame_sink_ids);
    }
  }
}

}  // namespace

FrameThrottlingController::FrameThrottlingController(
    ui::ContextFactory* context_factory)
    : context_factory_(context_factory) {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kFrameThrottleFps)) {
    int value;
    if (base::StringToInt(cl->GetSwitchValueASCII(switches::kFrameThrottleFps),
                          &value)) {
      throttled_fps_ = value;
    }
  }
}

FrameThrottlingController::~FrameThrottlingController() {
  EndThrottling();
}

void FrameThrottlingController::StartThrottling(
    const std::vector<aura::Window*>& windows) {
  if (windows_throttled_)
    EndThrottling();

  auto all_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  std::vector<aura::Window*> all_arc_windows;
  std::copy_if(all_windows.begin(), all_windows.end(),
               std::back_inserter(all_arc_windows), [](aura::Window* window) {
                 return ash::AppType::ARC_APP ==
                        static_cast<ash::AppType>(
                            window->GetProperty(aura::client::kAppType));
               });

  std::vector<aura::Window*> browser_windows;
  browser_windows.reserve(windows.size());
  std::vector<aura::Window*> arc_windows;
  arc_windows.reserve(windows.size());
  for (auto* window : windows) {
    ash::AppType type =
        static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
    switch (type) {
      case ash::AppType::BROWSER:
        browser_windows.push_back(window);
        break;
      case ash::AppType::ARC_APP:
        arc_windows.push_back(window);
        break;
      default:
        break;
    }
  }

  if (!browser_windows.empty()) {
    std::vector<viz::FrameSinkId> frame_sink_ids;
    frame_sink_ids.reserve(browser_windows.size());
    CollectBrowserFrameSinkIds(browser_windows, &frame_sink_ids);
    if (!frame_sink_ids.empty())
      StartThrottlingFrameSinks(frame_sink_ids);
  }

  std::vector<aura::Window*> all_windows_to_throttle(browser_windows);

  // Do not throttle arc if at least one arc window should not be throttled.
  if (!arc_windows.empty() && (arc_windows.size() == all_arc_windows.size())) {
    StartThrottlingArc(arc_windows);
    all_windows_to_throttle.insert(all_windows_to_throttle.end(),
                                   arc_windows.begin(), arc_windows.end());
  }

  if (!all_windows_to_throttle.empty()) {
    windows_throttled_ = true;
    for (auto& observer : observers_)
      observer.OnThrottlingStarted(all_windows_to_throttle, throttled_fps_);
  }
}

void FrameThrottlingController::StartThrottlingFrameSinks(
    const std::vector<viz::FrameSinkId>& frame_sink_ids) {
  DCHECK(!frame_sink_ids.empty());
  if (context_factory_) {
    context_factory_->GetHostFrameSinkManager()->StartThrottling(
        frame_sink_ids, base::TimeDelta::FromSeconds(1) / throttled_fps_);
  }
}

void FrameThrottlingController::StartThrottlingArc(
    const std::vector<aura::Window*>& arc_windows) {
  for (auto& arc_observer : arc_observers_) {
    arc_observer.OnThrottlingStarted(arc_windows, throttled_fps_);
  }
}

void FrameThrottlingController::EndThrottlingFrameSinks() {
  if (context_factory_)
    context_factory_->GetHostFrameSinkManager()->EndThrottling();
}

void FrameThrottlingController::EndThrottlingArc() {
  for (auto& arc_observer : arc_observers_) {
    arc_observer.OnThrottlingEnded();
  }
}

void FrameThrottlingController::EndThrottling() {
  EndThrottlingFrameSinks();
  EndThrottlingArc();

  for (auto& observer : observers_) {
    observer.OnThrottlingEnded();
  }
  windows_throttled_ = false;
}

void FrameThrottlingController::AddObserver(FrameThrottlingObserver* observer) {
  observers_.AddObserver(observer);
}

void FrameThrottlingController::RemoveObserver(
    FrameThrottlingObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FrameThrottlingController::AddArcObserver(
    FrameThrottlingObserver* observer) {
  arc_observers_.AddObserver(observer);
}

void FrameThrottlingController::RemoveArcObserver(
    FrameThrottlingObserver* observer) {
  arc_observers_.RemoveObserver(observer);
}

}  // namespace ash
