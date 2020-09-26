// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_UTIL_H_
#define ASH_WM_DESKS_DESKS_UTIL_H_

#include <array>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/compositor/compositor.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Compositor;
}  // namespace ui

namespace ash {

namespace desks_util {

constexpr size_t kMaxNumberOfDesks = 4;

ASH_EXPORT const std::array<int, kMaxNumberOfDesks>& GetDesksContainersIds();

ASH_EXPORT std::vector<aura::Window*> GetDesksContainers(aura::Window* root);

ASH_EXPORT const char* GetDeskContainerName(int container_id);

ASH_EXPORT bool IsDeskContainer(const aura::Window* container);

ASH_EXPORT bool IsDeskContainerId(int id);

// NOTE: The below *ActiveDesk* functions work with the currently active desk.
// If they can be called during a desk-switch animation, you might be interested
// in the soon-to-be active desk when the animation ends.
// See `DesksController::GetTargetActiveDesk()`.

ASH_EXPORT int GetActiveDeskContainerId();

ASH_EXPORT bool IsActiveDeskContainer(const aura::Window* container);

ASH_EXPORT aura::Window* GetActiveDeskContainerForRoot(aura::Window* root);

ASH_EXPORT bool BelongsToActiveDesk(aura::Window* window);

// If |context| is a descendent window of a desk container, return that desk
// container, otherwise return nullptr.
ASH_EXPORT aura::Window* GetDeskContainerForContext(aura::Window* context);

// Returns true if the DesksBar widget should be created in overview mode.
ASH_EXPORT bool ShouldDesksBarBeCreated();

// Selects and returns the compositor to measure performance metrics.
ui::Compositor* GetSelectedCompositorForPerformanceMetrics();

}  // namespace desks_util

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_UTIL_H_
