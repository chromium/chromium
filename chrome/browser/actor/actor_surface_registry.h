// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_

#include <cstddef>
#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/headless_web_contents_manager.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor {

class ActorKeyedService;
class ActorSurfaceImpl;

// Owns every ActorSurface for a profile, both tab-backed and headless.
//
// Every live tab in this profile has a tab-backed surface, created eagerly by
// ActorSurfaceTabHelper and torn down when the tab is destroyed. Headless
// surfaces are created and destroyed by the client via ActorKeyedService APIs.
// A surface never outlives its backing, which is what lets
// ActorSurface::GetWebContents() always return a valid pointer. See
// chrome/browser/actor/README.md.
class ActorSurfaceRegistry : public HeadlessWebContentsManager::Observer {
 public:
  // `headless_manager` must outlive this registry.
  explicit ActorSurfaceRegistry(HeadlessWebContentsManager* headless_manager);
  ActorSurfaceRegistry(const ActorSurfaceRegistry&) = delete;
  ActorSurfaceRegistry& operator=(const ActorSurfaceRegistry&) = delete;
  ~ActorSurfaceRegistry() override;

  // Profile-scoped lookups: return null if no matching surface is owned by
  // this profile's registry. Contrast with ActorSurfaceHandle::Get(), which is
  // process-wide and profile-agnostic.
  ActorSurface* Get(ActorSurfaceHandle handle) const;
  ActorSurface* GetForTab(tabs::TabHandle tab) const;
  ActorSurface* GetForHeadless(const content::WebContents* contents) const;

  // Lifecycle notifications from ActorSurfaceTabHelper.
  void OnTabCreated(tabs::TabInterface& tab);
  void OnTabWillBeDestroyed(tabs::TabHandle tab);

  // Creates a headless WebContents owned by the headless manager and returns
  // its surface. Features use ActorKeyedService::CreateHeadlessActorSurface().
  ActorSurface* CreateHeadlessWebContents(base::PassKey<ActorKeyedService>);

  // `contents` must be owned by the headless manager.
  ActorSurface* CreateForHeadless(content::WebContents* contents,
                                  base::PassKey<ActorKeyedService>);

  // Reconciles registry state when a surface's backing changes; the surface
  // keeps its ActorSurfaceHandle. Promotion and demotion move the same
  // WebContents in and out of a tab, so the new backing is derived from the
  // surface itself.
  //
  // Both must be called while the tab exists: after the WebContents is inserted
  // into a tab, and before the tab is destroyed.
  void OnSurfacePromoted(ActorSurfaceHandle handle);
  void OnSurfaceWillBeDemoted(ActorSurfaceHandle handle);

  // Called to destroy the headless surface represented by `handle` and its
  // WebContents. The surface is dropped before the WebContents is destroyed. A
  // no-op if the surface is already gone. For tab-backed surfaces, cleanup is
  // done via OnTabWillBeDestroyed() instead.
  void DestroyHeadlessActorSurface(ActorSurfaceHandle handle,
                                   base::PassKey<ActorKeyedService>);

  // HeadlessWebContentsManager::Observer:
  void OnHeadlessContentsWillBeDestroyed(
      content::WebContents* contents) override;

  size_t size() const { return owned_surfaces_.size(); }

  base::WeakPtr<ActorSurfaceRegistry> GetWeakPtr();

 private:
  ActorSurfaceImpl* GetImpl(ActorSurfaceHandle handle) const;

  // Creates and destroys the WebContents backing headless surfaces.
  const raw_ptr<HeadlessWebContentsManager> headless_manager_;
  base::ScopedObservation<HeadlessWebContentsManager,
                          HeadlessWebContentsManager::Observer>
      headless_manager_observation_{this};

  // All surfaces owned by this profile's registry, keyed by handle.
  std::map<ActorSurfaceHandle, std::unique_ptr<ActorSurfaceImpl>>
      owned_surfaces_;

  // Surface handle for each tab-backed surface's tab.
  std::map<tabs::TabHandle, ActorSurfaceHandle> tab_to_surface_;

  base::WeakPtrFactory<ActorSurfaceRegistry> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
