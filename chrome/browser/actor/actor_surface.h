// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_H_

#include <optional>

#include "components/actor/core/actor_surface_id.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor {

class ActorTabData;

// An actuation surface for the actor, backed by either a visible browser tab
// or an unparented headless WebContents.
//
// Lifetime contract: surfaces are owned by ActorSurfaceRegistry, which
// destroys a surface before its backing tab or WebContents goes away. While a
// surface is alive, GetWebContents() is always non-null.
class ActorSurface {
 public:
  virtual ~ActorSurface() = default;

  // Stable across promotion and demotion.
  virtual ActorSurfaceId Id() const = 0;

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
