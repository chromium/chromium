// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_registry.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/web_contents.h"

namespace actor {
namespace {

content::BrowserContext* GetBrowserContextForTab(tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  if (tab->GetProfile()) {
    return tab->GetProfile();
  }
  return tab->GetContents() ? tab->GetContents()->GetBrowserContext() : nullptr;
}

}  // namespace

ActorSurfaceRegistry::ActorSurfaceRegistry(
    HeadlessWebContentsManager* headless_manager)
    : headless_manager_(headless_manager) {
  CHECK(headless_manager_);
  headless_manager_observation_.Observe(headless_manager_.get());
}

ActorSurfaceRegistry::~ActorSurfaceRegistry() = default;

ActorSurface* ActorSurfaceRegistry::Get(ActorSurfaceHandle handle) const {
  return GetImpl(handle);
}

ActorSurface* ActorSurfaceRegistry::GetForTab(tabs::TabHandle tab) const {
  auto it = tab_to_surface_.find(tab);
  return it == tab_to_surface_.end() ? nullptr : Get(it->second);
}

ActorSurface* ActorSurfaceRegistry::GetForHeadless(
    const content::WebContents* contents) const {
  CHECK(contents);
  for (const auto& [handle, surface] : owned_surfaces_) {
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
  // Exclude nonexistent or cross-profile tabs.
  if (GetBrowserContextForTab(tab.Get()) !=
      headless_manager_->browser_context()) {
    return nullptr;
  }
  if (ActorSurface* existing = GetForTab(tab)) {
    return existing;
  }
  ActorSurfaceHandle handle =
      base::FeatureList::IsEnabled(kUseTabHandleAsSurfaceHandle)
          ? ActorSurfaceHandle(tab.raw_value())
          : ActorSurfaceHandle::NextHandle(
                base::PassKey<ActorSurfaceRegistry>());
  CHECK(!owned_surfaces_.contains(handle));
  owned_surfaces_.emplace(handle,
                          std::make_unique<ActorSurfaceImpl>(handle, tab));
  StartTrackingTab(handle, tab);
  return Get(handle);
}

ActorSurface* ActorSurfaceRegistry::CreateHeadlessWebContents() {
  return CreateForHeadless(headless_manager_->Create());
}

ActorSurface* ActorSurfaceRegistry::CreateForHeadless(
    content::WebContents* contents) {
  CHECK(contents);
  CHECK_EQ(contents->GetBrowserContext(), headless_manager_->browser_context());
  if (ActorSurface* existing = GetForHeadless(contents)) {
    return existing;
  }
  ActorSurfaceHandle handle =
      ActorSurfaceHandle::NextHandle(base::PassKey<ActorSurfaceRegistry>());
  CHECK(!owned_surfaces_.contains(handle));
  owned_surfaces_.emplace(handle,
                          std::make_unique<ActorSurfaceImpl>(handle, contents));
  return Get(handle);
}

void ActorSurfaceRegistry::OnSurfacePromoted(ActorSurfaceHandle handle) {
  ActorSurfaceImpl* surface = GetImpl(handle);
  CHECK(surface);
  CHECK(!surface->IsTab());

  // Promotion parents the surface's existing WebContents into a tab.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(surface->GetWebContents());
  CHECK(tab);

  surface->SetTab(tab->GetHandle());
  StartTrackingTab(handle, tab->GetHandle());
}

void ActorSurfaceRegistry::OnSurfaceWillBeDemoted(ActorSurfaceHandle handle) {
  ActorSurfaceImpl* surface = GetImpl(handle);
  CHECK(surface);
  CHECK(surface->IsTab());
  CHECK(!GetForHeadless(surface->GetWebContents()));

  // The WebContents outlives the tab wrapper and becomes the headless backing.
  StopTrackingTab(*surface->GetTabHandle());
  surface->SetHeadless();
}

void ActorSurfaceRegistry::DestroySurface(ActorSurfaceHandle handle) {
  ActorSurfaceImpl* surface = GetImpl(handle);
  if (!surface) {
    return;
  }
  if (std::optional<tabs::TabHandle> tab = surface->GetTabHandle()) {
    StopTrackingTab(*tab);
    owned_surfaces_.erase(handle);
  } else {
    content::WebContents* contents = surface->GetWebContents();
    owned_surfaces_.erase(handle);
    headless_manager_->Destroy(contents);
  }
}

void ActorSurfaceRegistry::OnHeadlessContentsWillBeDestroyed(
    content::WebContents* contents) {
  // Intentionally empty: the registry drops headless surfaces in
  // DestroySurface() before asking the manager to destroy the WebContents.
}

ActorSurfaceImpl* ActorSurfaceRegistry::GetImpl(
    ActorSurfaceHandle handle) const {
  auto it = owned_surfaces_.find(handle);
  return it == owned_surfaces_.end() ? nullptr : it->second.get();
}

void ActorSurfaceRegistry::StartTrackingTab(ActorSurfaceHandle handle,
                                            tabs::TabHandle tab) {
  CHECK(tab.Get());
  CHECK(!tab_to_surface_.contains(tab));
  tab_to_surface_[tab] = handle;
  tab_subscriptions_[tab] = tab.Get()->RegisterWillDetach(base::BindRepeating(
      [](ActorSurfaceRegistry* registry, ActorSurfaceHandle handle,
         tabs::TabInterface* tab, tabs::TabInterface::DetachReason reason) {
        if (reason == tabs::TabInterface::DetachReason::kDelete) {
          registry->DestroySurface(handle);
        }
      },
      base::Unretained(this), handle));
}

void ActorSurfaceRegistry::StopTrackingTab(tabs::TabHandle tab) {
  tab_to_surface_.erase(tab);
  tab_subscriptions_.erase(tab);
}

}  // namespace actor
