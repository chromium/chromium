// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_IMPL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/actor_surface.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor {

// Holds either a tab handle or a headless WebContents, never both. Created,
// owned, and mutated per-profile by ActorSurfaceRegistry. Registers its
// ActorSurfaceHandle in the process-wide lookup table on construction and
// unregisters it on destruction to back convenient lookups via
// ActorSurfaceHandle::Get().
class ActorSurfaceImpl final : public ActorSurface {
 public:
  ActorSurfaceImpl(ActorSurfaceHandle handle, tabs::TabHandle tab);
  ActorSurfaceImpl(ActorSurfaceHandle handle,
                   content::WebContents* headless_contents);

  // Disallow copy/assign.
  ActorSurfaceImpl(const ActorSurfaceImpl&) = delete;
  ActorSurfaceImpl& operator=(const ActorSurfaceImpl&) = delete;

  ~ActorSurfaceImpl() override;

  // ActorSurface:
  ActorSurfaceHandle GetHandle() const override;
  bool IsTab() const override;
  content::WebContents* GetWebContents() const override;
  std::optional<tabs::TabHandle> GetTabHandle() const override;
  ActorTabData* GetActorTabData() const override;

 private:
  friend class ActorSurfaceRegistry;

  // Transitions keep the same WebContents; only the backing changes.
  void SetTab(tabs::TabHandle tab);
  void SetHeadless();

  // Unique ID for this surface.
  const ActorSurfaceHandle handle_;

  // Set when this surface is backed by a tab; mutually exclusive with
  // `headless_contents_`.
  std::optional<tabs::TabHandle> tab_;

  // Set when this surface is backed by a headless WebContents; mutually
  // exclusive with `tab_`.
  raw_ptr<content::WebContents> headless_contents_ = nullptr;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_IMPL_H_
