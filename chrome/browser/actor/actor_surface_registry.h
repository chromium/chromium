// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_

#include <cstddef>
#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/headless_web_contents_manager.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor {

class ActorSurfaceImpl;

// Owns every ActorSurface for a profile.
//
// Tab-backed surfaces are torn down automatically when their tab is deleted.
// Headless surfaces are torn down by DestroySurface(), which drops the surface
// and then asks HeadlessWebContentsManager to destroy the WebContents. Either
// way a surface never outlives its backing, which is what lets
// ActorSurface::GetWebContents() always return a valid pointer.
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

  // Returns the surface for `tab`, creating one if needed. Returns null if
  // `tab` does not resolve to a live tab in this registry.
  ActorSurface* GetOrCreateForTab(tabs::TabHandle tab);

  // Creates a headless WebContents owned by the headless manager and returns
  // its surface.
  ActorSurface* CreateHeadlessWebContents();

  // `contents` must be owned by the headless manager.
  ActorSurface* CreateForHeadless(content::WebContents* contents);

  // Reconciles registry state when a surface's backing changes; the surface
  // keeps its ActorSurfaceHandle. Promotion and demotion move the same
  // WebContents in and out of a tab, so the new backing is derived from the
  // surface itself.
  //
  // Both must be called while the tab exists: after the WebContents is inserted
  // into a tab, and before it is detached from one. Detaching fires
  // WillDetach(kDelete), which destroys tab-backed surfaces.
  void OnSurfacePromoted(ActorSurfaceHandle handle);
  void OnSurfaceWillBeDemoted(ActorSurfaceHandle handle);

  void DestroySurface(ActorSurfaceHandle handle);

  // HeadlessWebContentsManager::Observer:
  void OnHeadlessContentsWillBeDestroyed(
      content::WebContents* contents) override;

  size_t size() const { return owned_surfaces_.size(); }

 private:
  ActorSurfaceImpl* GetImpl(ActorSurfaceHandle handle) const;
  void StartTrackingTab(ActorSurfaceHandle handle, tabs::TabHandle tab);
  void StopTrackingTab(tabs::TabHandle tab);

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

  // WillDetach subscriptions for tracked tabs.
  std::map<tabs::TabHandle, base::CallbackListSubscription> tab_subscriptions_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
