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
#include "ui/aura/window_tree_host.h"

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

// Recursively walks through all descendents of |window| and collects those
// belonging to a browser and with their frame sink ids being a member of |ids|.
// |inside_browser| indicates if the |window| already belongs to a browser.
// |browser_ids| is the output containing the result frame sinks ids.
void CollectBrowserFrameSinkIdsInWindow(
    const aura::Window* window,
    bool inside_browser,
    const base::flat_set<viz::FrameSinkId>& ids,
    base::flat_set<viz::FrameSinkId>* browser_ids) {
  if (inside_browser || ash::AppType::BROWSER ==
                            static_cast<ash::AppType>(
                                window->GetProperty(aura::client::kAppType))) {
    const auto& id = window->GetFrameSinkId();
    if (id.is_valid() && ids.contains(id))
      browser_ids->insert(id);
    inside_browser = true;
  }

  for (auto* child : window->children())
    CollectBrowserFrameSinkIdsInWindow(child, inside_browser, ids, browser_ids);
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

void FrameThrottlingController::OnCompositingFrameSinksToThrottleUpdated(
    const aura::WindowTreeHost* window_tree_host,
    const base::flat_set<viz::FrameSinkId>& ids) {
  base::flat_set<viz::FrameSinkId>& browser_ids =
      host_to_ids_map_[window_tree_host];
  browser_ids.clear();
  CollectBrowserFrameSinkIdsInWindow(window_tree_host->window(), false, ids,
                                     &browser_ids);
  UpdateThrottling();
}

void FrameThrottlingController::OnWindowDestroying(aura::Window* window) {
  DCHECK(window->IsRootWindow());
  host_to_ids_map_.erase(window->GetHost());
  UpdateThrottling();
  window->RemoveObserver(this);
}

void FrameThrottlingController::OnWindowTreeHostCreated(
    aura::WindowTreeHost* host) {
  host->AddObserver(this);
  host->window()->AddObserver(this);
}

void FrameThrottlingController::UpdateThrottling() {
  std::vector<viz::FrameSinkId> ids_to_throttle;
  for (const auto& pair : host_to_ids_map_) {
    ids_to_throttle.insert(ids_to_throttle.end(), pair.second.begin(),
                           pair.second.end());
  }
  context_factory_->GetHostFrameSinkManager()->Throttle(
      ids_to_throttle, base::TimeDelta::FromHz(throttled_fps_));
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
