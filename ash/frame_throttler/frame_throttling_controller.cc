// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_throttler/frame_throttling_controller.h"

#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
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
      fps_ = value;
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

  windows_throttled_ = true;
  std::vector<viz::FrameSinkId> frame_sink_ids;
  frame_sink_ids.reserve(windows.size());
  CollectBrowserFrameSinkIds(windows, &frame_sink_ids);
  if (!frame_sink_ids.empty())
    StartThrottling(frame_sink_ids, fps_);

  for (auto& observer : observers_) {
    observer.OnThrottlingStarted(windows);
  }
}

void FrameThrottlingController::StartThrottling(
    const std::vector<viz::FrameSinkId>& frame_sink_ids,
    uint8_t fps) {
  DCHECK_GT(fps, 0);
  DCHECK(!frame_sink_ids.empty());
  if (context_factory_) {
    context_factory_->GetHostFrameSinkManager()->StartThrottling(
        frame_sink_ids, base::TimeDelta::FromSeconds(1) / fps);
  }
}

void FrameThrottlingController::EndThrottling() {
  if (context_factory_)
    context_factory_->GetHostFrameSinkManager()->EndThrottling();

  for (auto& observer : observers_) {
    observer.OnThrottlingEnded();
  }
  windows_throttled_ = false;
}

void FrameThrottlingController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FrameThrottlingController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
