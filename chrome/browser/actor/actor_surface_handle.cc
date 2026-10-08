// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_handle.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_surface.h"
#include "components/actor/core/actor_features.h"

namespace actor {
namespace {

// Lookup table to back ActorSurfaceHandle::Get().
class ActorSurfaceLookup {
 public:
  static ActorSurfaceLookup& GetInstance() {
    static base::NoDestructor<ActorSurfaceLookup> lookup;
    return *lookup;
  }

  ActorSurface* Get(ActorSurfaceHandle handle) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = registered_surfaces_.find(handle);
    return it == registered_surfaces_.end() ? nullptr : it->second.get();
  }

  ActorSurfaceHandle GetNextHandle() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (base::FeatureList::IsEnabled(kUseTabHandleAsSurfaceHandle)) {
      CHECK_GT(next_headless_handle_, std::numeric_limits<int32_t>::min());
      return ActorSurfaceHandle(next_headless_handle_--);
    }
    CHECK_LT(next_handle_, std::numeric_limits<int32_t>::max());
    return ActorSurfaceHandle(next_handle_++);
  }

  void Register(ActorSurfaceHandle handle, ActorSurface* surface) {
    CHECK(!handle.is_null());
    CHECK(surface);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto [it, inserted] = registered_surfaces_.emplace(handle, surface);
    CHECK(inserted) << "Duplicate ActorSurfaceHandle: " << handle;
  }

  void Unregister(ActorSurfaceHandle handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = registered_surfaces_.find(handle);
    CHECK(it != registered_surfaces_.end());
    std::optional<tabs::TabHandle> tab = it->second->GetTabHandle();
    registered_surfaces_.erase(it);

    // Without a task runner (e.g. very late in shutdown) the entry could never
    // be cleared, so don't record it.
    if (!tab || !base::SequencedTaskRunner::HasCurrentDefault()) {
      return;
    }
    recently_destroyed_tabs_[*tab] = handle;
    // Clear the destroyed handle after a PostTask. Safe to use base::Unretained
    // since ActorSurfaceLookup is a singleton.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ActorSurfaceLookup::ForgetDestroyedTab,
                                  base::Unretained(this), *tab));
  }

  // Linear in the number of live surfaces (roughly the number of open tabs),
  // which keeps this lookup consistent with each surface's current backing
  // without a second index to maintain. Falls back to surfaces destroyed
  // earlier in the current task; see `recently_destroyed_tabs_`.
  ActorSurfaceHandle GetForTab(tabs::TabHandle tab) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (const auto& [handle, surface] : registered_surfaces_) {
      if (surface->GetTabHandle() == tab) {
        return handle;
      }
    }
    // Maybe the tab was just deleted. Find it from the recent deletions.
    auto it = recently_destroyed_tabs_.find(tab);
    return it == recently_destroyed_tabs_.end() ? ActorSurfaceHandle::Null()
                                                : it->second;
  }

  // Inverse of GetForTab(). Falls back to surfaces destroyed earlier in the
  // current task; see `recently_destroyed_tabs_`.
  tabs::TabHandle GetTabForHandle(ActorSurfaceHandle handle) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (ActorSurface* surface = Get(handle)) {
      return surface->GetTabHandle().value_or(tabs::TabHandle::Null());
    }
    // Maybe the surface's tab was just deleted. Find it from the recent
    // deletions.
    for (const auto& [tab, destroyed_handle] : recently_destroyed_tabs_) {
      if (destroyed_handle == handle) {
        return tab;
      }
    }
    return tabs::TabHandle::Null();
  }

 private:
  void ForgetDestroyedTab(tabs::TabHandle tab) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    recently_destroyed_tabs_.erase(tab);
  }

  // Map of all live surfaces across profiles, keyed by handle.
  base::flat_map<ActorSurfaceHandle, raw_ptr<ActorSurface>> registered_surfaces_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Tab-backed surfaces destroyed during the current task, keyed by their tab.
  // A tab's surface is destroyed with its TabFeatures, which happens before
  // TabStripModel notifies observers of the removal while the tab itself is
  // still alive. Keeping the handle until the current task completes lets
  // those observers still map the tab to its (now destroyed) surface handle.
  // The entry is cleaned up shortly with a PostTask, so the map never grows
  // unbounded.
  base::flat_map<tabs::TabHandle, ActorSurfaceHandle> recently_destroyed_tabs_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Monotonically increasing positive handle counter (> 0). Used for all
  // surfaces when kUseTabHandleAsSurfaceHandle is disabled.
  int32_t next_handle_ GUARDED_BY_CONTEXT(sequence_checker_) = 1;

  // Will be removed after the refactor is complete. Monotonically decreasing
  // negative handle counter (< 0) used for headless surfaces while
  // kUseTabHandleAsSurfaceHandle is enabled and tab-backed surfaces reuse
  // tabs::TabHandle::raw_value().
  int32_t next_headless_handle_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

ActorSurface* ActorSurfaceHandle::Get() const {
  if (is_null()) {
    return nullptr;
  }
  return ActorSurfaceLookup::GetInstance().Get(*this);
}

tabs::TabHandle ActorSurfaceHandle::GetTabHandle() const {
  if (is_null()) {
    return tabs::TabHandle::Null();
  }
  return ActorSurfaceLookup::GetInstance().GetTabForHandle(*this);
}

// static
ActorSurfaceHandle ActorSurfaceHandle::From(tabs::TabHandle tab) {
  if (tab == tabs::TabHandle::Null()) {
    return Null();
  }
  return ActorSurfaceLookup::GetInstance().GetForTab(tab);
}

// static
ActorSurfaceHandle ActorSurfaceHandle::NextHandle(
    base::PassKey<ActorSurfaceRegistry>) {
  return ActorSurfaceLookup::GetInstance().GetNextHandle();
}

// static
void ActorSurfaceHandle::Register(ActorSurfaceHandle handle,
                                  ActorSurface* surface) {
  ActorSurfaceLookup::GetInstance().Register(handle, surface);
}

// static
void ActorSurfaceHandle::Unregister(ActorSurfaceHandle handle) {
  ActorSurfaceLookup::GetInstance().Unregister(handle);
}

std::ostream& operator<<(std::ostream& os, ActorSurfaceHandle handle) {
  return os << "ActorSurfaceHandle(" << handle.raw_value() << ")";
}

}  // namespace actor
