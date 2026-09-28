// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_registry.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_surface_impl.h"

namespace actor {

ActorSurfaceRegistry::ActorSurfaceRegistry() = default;

ActorSurfaceRegistry::~ActorSurfaceRegistry() = default;

ActorSurface* ActorSurfaceRegistry::Get(ActorSurfaceId id) const {
  return GetImpl(id);
}

ActorSurface* ActorSurfaceRegistry::GetForTab(tabs::TabHandle tab) const {
  auto it = tab_to_surface_.find(tab);
  return it == tab_to_surface_.end() ? nullptr : Get(it->second);
}

ActorSurface* ActorSurfaceRegistry::GetForHeadless(
    const content::WebContents* contents) const {
  CHECK(contents);
  for (const auto& [id, surface] : surfaces_) {
    if (surface->IsTab()) {
      continue;
    }
    if (surface->GetWebContents() == contents) {
      return surface.get();
    }
  }
  return nullptr;
}

ActorSurface* ActorSurfaceRegistry::GetOrCreateForTab(tabs::TabHandle tab) {
  if (ActorSurface* existing = GetForTab(tab)) {
    return existing;
  }
  ActorSurfaceId id = next_surface_id_.GenerateNextId();
  surfaces_.emplace(id, std::make_unique<ActorSurfaceImpl>(id, tab));
  StartTrackingTab(id, tab);
  return Get(id);
}

ActorSurface* ActorSurfaceRegistry::CreateForHeadless(
    content::WebContents* contents) {
  if (ActorSurface* existing = GetForHeadless(contents)) {
    return existing;
  }
  ActorSurfaceId id = next_surface_id_.GenerateNextId();
  surfaces_.emplace(id, std::make_unique<ActorSurfaceImpl>(id, contents));
  return Get(id);
}

void ActorSurfaceRegistry::OnSurfacePromoted(ActorSurfaceId id) {
  ActorSurfaceImpl* surface = GetImpl(id);
  CHECK(surface);
  CHECK(!surface->IsTab());

  // Promotion parents the surface's existing WebContents into a tab.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(surface->GetWebContents());
  CHECK(tab);

  surface->SetTab(tab->GetHandle());
  StartTrackingTab(id, tab->GetHandle());
}

void ActorSurfaceRegistry::OnSurfaceWillBeDemoted(ActorSurfaceId id) {
  ActorSurfaceImpl* surface = GetImpl(id);
  CHECK(surface);
  CHECK(surface->IsTab());
  CHECK(!GetForHeadless(surface->GetWebContents()));

  // The WebContents outlives the tab wrapper and becomes the headless backing.
  StopTrackingTab(*surface->GetTabHandle());
  surface->SetHeadless();
}

void ActorSurfaceRegistry::DestroySurface(ActorSurfaceId id) {
  ActorSurfaceImpl* surface = GetImpl(id);
  if (!surface) {
    return;
  }
  if (std::optional<tabs::TabHandle> tab = surface->GetTabHandle()) {
    StopTrackingTab(*tab);
  }
  surfaces_.erase(id);
}

ActorSurfaceImpl* ActorSurfaceRegistry::GetImpl(ActorSurfaceId id) const {
  auto it = surfaces_.find(id);
  return it == surfaces_.end() ? nullptr : it->second.get();
}

void ActorSurfaceRegistry::StartTrackingTab(ActorSurfaceId id,
                                            tabs::TabHandle tab) {
  CHECK(!tab_to_surface_.contains(tab));
  tab_to_surface_[tab] = id;
  tab_subscriptions_[tab] = tab.Get()->RegisterWillDetach(base::BindRepeating(
      [](ActorSurfaceRegistry* registry, ActorSurfaceId id,
         tabs::TabInterface* tab, tabs::TabInterface::DetachReason reason) {
        if (reason == tabs::TabInterface::DetachReason::kDelete) {
          registry->DestroySurface(id);
        }
      },
      base::Unretained(this), id));
}

void ActorSurfaceRegistry::StopTrackingTab(tabs::TabHandle tab) {
  tab_to_surface_.erase(tab);
  tab_subscriptions_.erase(tab);
}

}  // namespace actor
