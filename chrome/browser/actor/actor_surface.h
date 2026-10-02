// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_H_

#include <optional>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor {

class ActorTabData;

// An actuation surface for the actor, backed by either a visible browser tab
// or an unparented headless WebContents.
//
// Ownership and lifecycle:
//  - Surfaces are owned per-profile by ActorSurfaceRegistry.
//  - A surface's lifetime is strictly aligned with its backing tab or headless
//    WebContents: ActorSurfaceRegistry observes tab detach/deletion and
//    HeadlessWebContentsManager destruction, destroying the ActorSurface
//    before its backing tab or WebContents goes away. A live ActorSurface
//    never holds a dangling tab or WebContents, and GetWebContents() is always
//    non-null.
//  - Each live surface is registered in a process-wide lookup table on
//    construction and unregistered on destruction, allowing
//    ActorSurfaceHandle::Get() to safely resolve a handle to a live
//    ActorSurface* (or nullptr once destroyed) without needing a registry
//    reference.
class ActorSurface {
 public:
  virtual ~ActorSurface() = default;

  // Unique ID for the surface. Stable across promotion and demotion, and safe
  // to pass around since Get() returns null if the surface is destroyed.
  virtual ActorSurfaceHandle GetHandle() const = 0;

  // True if this surface is currently backed by a visible browser tab.
  virtual bool IsTab() const = 0;

  // Never null. For tabs this is resolved dynamically, since the tab's
  // WebContents can be swapped (e.g. on discard).
  virtual content::WebContents* GetWebContents() const = 0;

  // Nullopt when headless.
  virtual std::optional<tabs::TabHandle> GetTabHandle() const = 0;

  // Per-surface actor data, e.g. the last observed page content used for
  // TOCTOU validation.
  virtual ActorTabData* GetActorTabData() const = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_H_
