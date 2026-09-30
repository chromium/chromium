// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_

#include <cstdint>
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

  // Null if no surface exists.
  ActorSurface* Get(ActorSurfaceId id) const;
  ActorSurface* GetForTab(tabs::TabHandle tab) const;
  ActorSurface* GetForHeadless(const content::WebContents* contents) const;

  ActorSurface* GetOrCreateForTab(tabs::TabHandle tab);

  // Creates a headless WebContents owned by the headless manager and returns
  // its surface.
  ActorSurface* CreateHeadlessWebContents();

  // `contents` must be owned by the headless manager.
  ActorSurface* CreateForHeadless(content::WebContents* contents);

  // Reconciles registry state when a surface's backing changes; the surface
  // keeps its ActorSurfaceId. Promotion and demotion move the same WebContents
  // in and out of a tab, so the new backing is derived from the surface itself.
  //
  // Both must be called while the tab exists: after the WebContents is inserted
  // into a tab, and before it is detached from one. Detaching fires
  // WillDetach(kDelete), which destroys tab-backed surfaces.
  void OnSurfacePromoted(ActorSurfaceId id);
  void OnSurfaceWillBeDemoted(ActorSurfaceId id);

  void DestroySurface(ActorSurfaceId id);

  // HeadlessWebContentsManager::Observer:
  void OnHeadlessContentsWillBeDestroyed(
      content::WebContents* contents) override;

  size_t size() const { return surfaces_.size(); }

 private:
  ActorSurfaceImpl* GetImpl(ActorSurfaceId id) const;
  void StartTrackingTab(ActorSurfaceId id, tabs::TabHandle tab);
  void StopTrackingTab(tabs::TabHandle tab);

  // Creates and destroys the WebContents backing headless surfaces.
  const raw_ptr<HeadlessWebContentsManager> headless_manager_;
  base::ScopedObservation<HeadlessWebContentsManager,
                          HeadlessWebContentsManager::Observer>
      headless_manager_observation_{this};

  // Generates a new ActorSurfaceId for each surface created by this registry.
  // Used when kGenerateIndependentIdsForActorSurface is enabled.
  ActorSurfaceId::Generator next_surface_id_;

  // Next id for a headless surface when kGenerateIndependentIdsForActorSurface
  // is disabled. Starts well above any expected TabHandle value, since
  // tab-backed surfaces then use their TabHandle value as their id.
  static constexpr int32_t kFirstHeadlessSurfaceId = 1000000;
  int32_t next_headless_surface_id_ = kFirstHeadlessSurfaceId;

  // All surfaces owned by this registry, keyed by id.
  std::map<ActorSurfaceId, std::unique_ptr<ActorSurfaceImpl>> surfaces_;
  // Surface for each tab-backed surface's tab.
  std::map<tabs::TabHandle, ActorSurfaceId> tab_to_surface_;
  // WillDetach subscriptions for tracked tabs.
  std::map<tabs::TabHandle, base::CallbackListSubscription> tab_subscriptions_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
