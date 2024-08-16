// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_UTIL_H_
#define ASH_WM_DESKS_DESKS_UTIL_H_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/compositor.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Compositor;
}  // namespace ui

namespace ash {

class Desk;
namespace desks_util {

// Note: the max number of desks depends on a runtime flag and the function
// `GetMaxNumberOfDesks` below will return that value. The value returned from
// that function will not be more than this constant.
constexpr size_t kDesksUpperLimit = 16;

ASH_EXPORT size_t GetMaxNumberOfDesks();

ASH_EXPORT std::vector<int> GetDesksContainersIds();

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

// Returns active desk's associated lacros profile ID when Desk Profiles feature
// is enabled; returns null otherwise.
ASH_EXPORT std::optional<uint64_t> GetActiveDeskLacrosProfileId();

// If `context` is a descendent window of a desk container, return that desk
// container, otherwise return nullptr. Note that this will return nullptr if
// `context` is a descendent of the float container, even if it is associated
// with a desk container.
ASH_EXPORT aura::Window* GetDeskContainerForContext(aura::Window* context);

// If `context` belong to a desk, return the desk pointer, otherwise return
// nullptr. Note that floated window is not in the desk container but still
// considered as "belonging" to the desk, as it's only visible on a particular
// desk.
ASH_EXPORT const Desk* GetDeskForContext(aura::Window* context);

// Returns true if the DesksBar widget should be created in overview mode.
ASH_EXPORT bool ShouldDesksBarBeCreated();

// Returns true if the DesksBar widget should be created in overview mode and
// the desk bar should contain "mini views" of each desk.
ASH_EXPORT bool ShouldRenderDeskBarWithMiniViews();

// Selects and returns the compositor to measure performance metrics.
ui::Compositor* GetSelectedCompositorForPerformanceMetrics();

// Check if a desk is being dragged.
ASH_EXPORT bool IsDraggingAnyDesk();

// Returns whether a |window| is visible on all workspaces.
ASH_EXPORT bool IsWindowVisibleOnAllWorkspaces(const aura::Window* window);

// Returns true for windows that are interesting from an all-desk z-order
// tracking perspective.
ASH_EXPORT bool IsZOrderTracked(aura::Window* window);

// Get the position of `window` in `windows` (as filtered by `IsZOrderTracked`)
// in reverse order. If `window` is not in the list (or isn't z-order tracked),
// then nullopt is returned.
ASH_EXPORT std::optional<size_t> GetWindowZOrder(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
    aura::Window* window);

// Move an item at |old_index| to |new_index|.
template <typename T>
ASH_EXPORT void ReorderItem(std::vector<T>& items,
                            int old_index,
                            int new_index) {
  const int items_size = static_cast<int>(items.size());

  DCHECK_GE(old_index, 0);
  DCHECK_LT(old_index, items_size);
  DCHECK_GE(new_index, 0);
  DCHECK_LT(new_index, items_size);

  if (old_index == new_index)
    return;

  auto start_iter = items.begin();
  const int step = old_index < new_index ? 1 : -1;

  for (auto iter = start_iter + old_index; iter != start_iter + new_index;
       iter += step) {
    std::iter_swap(iter, iter + step);
  }
}

}  // namespace desks_util

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_UTIL_H_
