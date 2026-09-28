// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/actor/actor_surface.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor {

class ActorSurfaceImpl;

// Owns every ActorSurface for a profile.
//
// Tab-backed surfaces are torn down automatically when their tab is deleted.
// Headless surfaces are torn down by the headless WebContents manager, which
// owns their lifecycle and calls DestroySurface() before destroying the
// WebContents. Either way a surface never outlives its backing, which is what
// lets ActorSurface::GetWebContents() always return a valid pointer.
class ActorSurfaceRegistry {
 public:
  ActorSurfaceRegistry();
  ActorSurfaceRegistry(const ActorSurfaceRegistry&) = delete;
  ActorSurfaceRegistry& operator=(const ActorSurfaceRegistry&) = delete;
  ~ActorSurfaceRegistry();

  // Null if no surface exists.
  ActorSurface* Get(ActorSurfaceId id) const;
  ActorSurface* GetForTab(tabs::TabHandle tab) const;
  ActorSurface* GetForHeadless(const content::WebContents* contents) const;

  ActorSurface* GetOrCreateForTab(tabs::TabHandle tab);

  // `contents` is owned by the caller and must outlive the surface.
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

  size_t size() const { return surfaces_.size(); }

 private:
  ActorSurfaceImpl* GetImpl(ActorSurfaceId id) const;
  void StartTrackingTab(ActorSurfaceId id, tabs::TabHandle tab);
  void StopTrackingTab(tabs::TabHandle tab);

  ActorSurfaceId::Generator next_surface_id_;

  std::map<ActorSurfaceId, std::unique_ptr<ActorSurfaceImpl>> surfaces_;
  std::map<tabs::TabHandle, ActorSurfaceId> tab_to_surface_;
  std::map<tabs::TabHandle, base::CallbackListSubscription> tab_subscriptions_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_REGISTRY_H_
