// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_HANDLE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_HANDLE_H_

#include <cstdint>
#include <ostream>
#include <utility>

#include "base/types/pass_key.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ActorSurface;
class ActorSurfaceImpl;
class ActorSurfaceRegistry;

// A weak, copyable, comparable, hashable reference to an ActorSurface, modeled
// on tabs::TabHandle. Safe to store and pass around (including over mojo/proto
// via raw_value()) after the surface is destroyed; Get() then returns null.
//
// Ownership and lookup model:
//  - ActorSurface instances are owned per-profile by ActorSurfaceRegistry and
//    removed when the backing object is destroyed. Hence a live surface
//    resolved via Get() never has a dangling tab or WebContents.
//  - ActorSurfaceImpl registers itself in a process-wide lookup table on
//    construction and unregisters itself on destruction. This lets
//    ActorSurfaceHandle::Get() resolve any handle directly without plumbing
//    ActorSurfaceRegistry through every caller.
//
// Usage rules (same as TabHandle):
//  - Always use handles and never pass ActorSurface* or WebContents* across
//    async hops.
//  - Get() is profile-agnostic; callers acting on behalf of a profile must
//    verify the surface belongs to it.
//  - UI thread only.
//
// ID space:
//  - During migration (when kUseTabHandleAsSurfaceHandle is enabled):
//    tab-backed surfaces reuse their TabHandle raw value (> 0), and headless
//    surfaces allocate negative values (< 0) so the two ranges never collide.
//  - After the migration is finished (when kUseTabHandleAsSurfaceHandle is
//    disabled): all surfaces allocate from a single positive counter (> 0).
//  - 0 is null. IDs are never reused within a process.
class ActorSurfaceHandle {
 public:
  static constexpr int32_t kNullValue = 0;

  constexpr ActorSurfaceHandle() = default;
  constexpr explicit ActorSurfaceHandle(int32_t raw_value)
      : raw_value_(raw_value) {}

  static constexpr ActorSurfaceHandle Null() { return ActorSurfaceHandle(); }

  constexpr int32_t raw_value() const { return raw_value_; }
  constexpr bool is_null() const { return raw_value_ == kNullValue; }

  // Returns the surface, or null if it no longer exists (or never did).
  ActorSurface* Get() const;

  // Returns the tab currently backing this surface, or TabHandle::Null() if
  // the surface no longer exists or is not backed by a tab. This is the
  // inverse of From(); callers must not assume the two share a raw value.
  tabs::TabHandle GetTabHandle() const;

  // Returns the handle of the surface currently backed by `tab`, or Null() if
  // there is none (e.g. the tab is gone, or belongs to a profile without an
  // ActorKeyedService). This is the supported way to convert a tab to a
  // surface: callers must not assume the two share a raw value.
  static ActorSurfaceHandle From(tabs::TabHandle tab);

  // Mints the next unique handle (> 0, or < 0 for headless surfaces while
  // kUseTabHandleAsSurfaceHandle is enabled).
  static ActorSurfaceHandle NextHandle(base::PassKey<ActorSurfaceRegistry>);

  friend constexpr bool operator==(ActorSurfaceHandle,
                                   ActorSurfaceHandle) = default;
  friend constexpr auto operator<=>(ActorSurfaceHandle,
                                    ActorSurfaceHandle) = default;

  template <typename H>
  friend H AbslHashValue(H h, ActorSurfaceHandle handle) {
    return H::combine(std::move(h), handle.raw_value_);
  }

 private:
  friend class ActorSurfaceImpl;

  // Registers or unregisters an ActorSurface in the lookup table.
  static void Register(ActorSurfaceHandle handle, ActorSurface* surface);
  static void Unregister(ActorSurfaceHandle handle);

  // Underlying integer value of the handle. Defaults to 0 (kNullValue).
  int32_t raw_value_ = kNullValue;
};

std::ostream& operator<<(std::ostream& os, ActorSurfaceHandle handle);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_HANDLE_H_
