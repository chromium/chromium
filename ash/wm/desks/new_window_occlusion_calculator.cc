// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/new_window_occlusion_calculator.h"

#include "base/containers/circular_deque.h"
#include "base/functional/function_ref.h"
#include "components/viz/client/frame_eviction_manager.h"
#include "ui/aura/env.h"

namespace ash {

NewWindowOcclusionCalculator::NewWindowOcclusionCalculator() {
  auto* desks_controller = DesksController::Get();
  if (desks_controller) {
    desks_controller_observation_.Observe(desks_controller);
    for (const auto& desk : desks_controller->desks()) {
      desk_observations_.AddObservation(desk.get());
    }
  }
}

NewWindowOcclusionCalculator::~NewWindowOcclusionCalculator() = default;

aura::Window::OcclusionState NewWindowOcclusionCalculator::GetOcclusionState(
    aura::Window* window) const {
  aura::Window* w = window;
  while (w) {
    auto it = cached_states_.find(w);
    if (it != cached_states_.end()) {
      auto state_it = it->second.find(window);
      if (state_it != it->second.end()) {
        return state_it->second;
      }
      return aura::Window::OcclusionState::VISIBLE;
    }
    w = w->parent();
  }
  return aura::Window::OcclusionState::UNKNOWN;
}

void NewWindowOcclusionCalculator::SnapshotOcclusionStateForWindows(
    const aura::Window::Windows& containers_to_snapshot) {
  // Recalculate if the cache data is missing for any container, which is
  // empty if a container was added, or reset when a desk is removed,
  // a window is added or removed in the desk container.
  bool all_containers_have_cache = true;
  for (aura::Window* container : containers_to_snapshot) {
    if (!cached_states_.contains(container)) {
      all_containers_have_cache = false;
      break;
    }
  }
  if (all_containers_have_cache) {
    return;  // This also return when empty.
  }

  Reset();

  auto* tracker = aura::Env::GetInstance()->GetWindowOcclusionTracker();

  auto traverse_tree = [](aura::Window* root,
                          base::FunctionRef<void(aura::Window*)> callback) {
    base::circular_deque<aura::Window*> queue;
    queue.push_back(root);
    while (!queue.empty()) {
      aura::Window* w = queue.front();
      queue.pop_front();
      callback(w);
      for (aura::Window* child : w->children()) {
        queue.push_back(child);
      }
    }
  };

  // Collect all tracked windows under the snapshot containers and lock them
  // so that those who observe the occlusion state will not be notified
  // when taking snapshot.
  std::vector<std::unique_ptr<aura::WindowOcclusionTracker::ScopedLockState>>
      locks;
  for (aura::Window* container : containers_to_snapshot) {
    traverse_tree(container, [&locks, tracker](aura::Window* w) {
      if (tracker->IsTracking(w)) {
        locks.push_back(
            std::make_unique<aura::WindowOcclusionTracker::ScopedLockState>(w));
      }
    });
  }

  // 1. Pause frame eviction to prevent discarding frames during computation.
  viz::FrameEvictionManager::ScopedPause frame_eviction_pause;

  // 2. Force containers visible so they are computed as visible.
  std::vector<std::unique_ptr<aura::WindowOcclusionTracker::ScopedForceVisible>>
      forced_visibilities;
  for (aura::Window* container : containers_to_snapshot) {
    forced_visibilities.push_back(
        std::make_unique<aura::WindowOcclusionTracker::ScopedForceVisible>(
            container));
  }

  // 3. Force a synchronous occlusion computation.
  tracker->ForceComputeOcclusion();

  // 4. Extract computed occlusion states.
  for (aura::Window* container : containers_to_snapshot) {
    auto& map = cached_states_.try_emplace(container).first->second;
    traverse_tree(container, [&map, tracker](aura::Window* w) {
      if (tracker->IsTracking(w)) {
        map.try_emplace(w, tracker->GetComputedOcclusionState(w));
      }
    });
  }
}

std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
NewWindowOcclusionCalculator::Pause() {
  return std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
}

base::WeakPtr<WindowOcclusionCalculator>
NewWindowOcclusionCalculator::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void NewWindowOcclusionCalculator::OnDeskAdded(const Desk* desk,
                                               bool from_undo) {
  desk_observations_.AddObservation(const_cast<Desk*>(desk));
  Reset();
}

void NewWindowOcclusionCalculator::OnDeskRemoved(const Desk* desk) {
  desk_observations_.RemoveObservation(const_cast<Desk*>(desk));
  Reset();
}

void NewWindowOcclusionCalculator::OnContentChanged() {
  Reset();
}

void NewWindowOcclusionCalculator::OnDeskDestroyed(const Desk* desk) {
  if (desk_observations_.IsObservingSource(const_cast<Desk*>(desk))) {
    desk_observations_.RemoveObservation(const_cast<Desk*>(desk));
  }
  Reset();
}

void NewWindowOcclusionCalculator::Reset() {
  cached_states_.clear();
}

}  // namespace ash
