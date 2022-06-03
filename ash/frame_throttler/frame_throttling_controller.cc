// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_throttler/frame_throttling_controller.h"

#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

namespace {

void CollectFrameSinkIds(const aura::Window* window,
                         base::flat_set<viz::FrameSinkId>* frame_sink_ids) {
  if (window->GetFrameSinkId().is_valid()) {
    frame_sink_ids->insert(window->GetFrameSinkId());
    return;
  }
  for (auto* child : window->children()) {
    CollectFrameSinkIds(child, frame_sink_ids);
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
  if (windows_manually_throttled_)
    EndThrottling();

  if (windows.empty())
    return;

  auto all_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  std::vector<aura::Window*> all_arc_windows;
  std::copy_if(all_windows.begin(), all_windows.end(),
               std::back_inserter(all_arc_windows), [](aura::Window* window) {
                 return ash::AppType::ARC_APP ==
                        static_cast<ash::AppType>(
                            window->GetProperty(aura::client::kAppType));
               });

  std::vector<aura::Window*> arc_windows;
  arc_windows.reserve(windows.size());
  for (auto* window : windows) {
    ash::AppType type =
        static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
    switch (type) {
      case ash::AppType::BROWSER:
        CollectFrameSinkIds(window, &manually_throttled_ids_);
        break;
      case ash::AppType::ARC_APP:
        arc_windows.push_back(window);
        break;
      default:
        break;
    }
  }

  // Throttle browser frame sinks.
  if (!manually_throttled_ids_.empty()) {
    UpdateThrottlingOnBrowserWindows();
    windows_manually_throttled_ = true;
  }
  // Do not throttle arc if at least one arc window should not be throttled.
  if (!arc_windows.empty() && (arc_windows.size() == all_arc_windows.size())) {
    StartThrottlingArc(arc_windows);
    windows_manually_throttled_ = true;
  }
}

void FrameThrottlingController::StartThrottlingArc(
    const std::vector<aura::Window*>& arc_windows) {
  for (auto& arc_observer : arc_observers_) {
    arc_observer.OnThrottlingStarted(arc_windows, throttled_fps_);
  }
}

void FrameThrottlingController::EndThrottlingArc() {
  for (auto& arc_observer : arc_observers_) {
    arc_observer.OnThrottlingEnded();
  }
}

void FrameThrottlingController::EndThrottling() {
  if (windows_manually_throttled_) {
    manually_throttled_ids_.clear();
    UpdateThrottlingOnBrowserWindows();
    EndThrottlingArc();

    windows_manually_throttled_ = false;
  }
}

void FrameThrottlingController::OnCompositingFrameSinksToThrottleUpdated(
    const aura::WindowTreeHost* window_tree_host,
    const base::flat_set<viz::FrameSinkId>& ids) {
  base::flat_set<viz::FrameSinkId>& browser_ids =
      host_to_ids_map_[window_tree_host];
  browser_ids.clear();
  CollectBrowserFrameSinkIdsInWindow(window_tree_host->window(), false, ids,
                                     &browser_ids);
  UpdateThrottlingOnBrowserWindows();
}

void FrameThrottlingController::OnWindowDestroying(aura::Window* window) {
  DCHECK(window->IsRootWindow());
  host_to_ids_map_.erase(window->GetHost());
  UpdateThrottlingOnBrowserWindows();
  window->RemoveObserver(this);
}

void FrameThrottlingController::OnWindowTreeHostCreated(
    aura::WindowTreeHost* host) {
  host->AddObserver(this);
  host->window()->AddObserver(this);
}

std::vector<viz::FrameSinkId>
FrameThrottlingController::GetFrameSinkIdsToThrottle() const {
  std::vector<viz::FrameSinkId> ids_to_throttle;
  // Add frame sink ids gathered from compositing.
  for (const auto& pair : host_to_ids_map_) {
    ids_to_throttle.insert(ids_to_throttle.end(), pair.second.begin(),
                           pair.second.end());
  }
  // Add frame sink ids from special ui modes.
  if (!manually_throttled_ids_.empty()) {
    ids_to_throttle.insert(ids_to_throttle.end(),
                           manually_throttled_ids_.begin(),
                           manually_throttled_ids_.end());
  }
  return ids_to_throttle;
}

void FrameThrottlingController::UpdateThrottlingOnBrowserWindows() {
  context_factory_->GetHostFrameSinkManager()->Throttle(
      GetFrameSinkIdsToThrottle(), base::Hertz(throttled_fps_));
}

void FrameThrottlingController::AddArcObserver(
    FrameThrottlingObserver* observer) {
  arc_observers_.AddObserver(observer);
}

void FrameThrottlingController::RemoveArcObserver(
    FrameThrottlingObserver* observer) {
  arc_observers_.RemoveObserver(observer);
}

bool FrameThrottlingController::HasArcObserver(
    FrameThrottlingObserver* observer) {
  return arc_observers_.HasObserver(observer);
}

}  // namespace ash
