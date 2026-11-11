// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_registry.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
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

ActorSurface* ActorSurfaceRegistry::Get(
    ActorSurfaceHandle actor_surface_handle) const {
  return GetImpl(actor_surface_handle);
}

ActorSurface* ActorSurfaceRegistry::GetForTab(tabs::TabHandle tab) const {
  auto it = tab_to_surface_.find(tab);
  return it == tab_to_surface_.end() ? nullptr : Get(it->second);
}

ActorSurface* ActorSurfaceRegistry::GetForHeadless(
    const content::WebContents* contents) const {
  CHECK(contents);
  for (const auto& [actor_surface_handle, surface] : owned_surfaces_) {
    if (surface->IsTab()) {
      continue;
    }
    if (surface->GetWebContents() == contents) {
      return surface.get();
    }
  }
  return nullptr;
}

void ActorSurfaceRegistry::OnTabCreated(tabs::TabInterface& tab) {
  if (GetBrowserContextForTab(&tab) != headless_manager_->browser_context()) {
    return;
  }
  const tabs::TabHandle tab_handle = tab.GetHandle();
  if (GetForTab(tab_handle)) {
    return;
  }
  if (content::WebContents* contents = tab.GetContents()) {
    if (ActorSurface* headless = GetForHeadless(contents)) {
      const ActorSurfaceHandle actor_surface_handle = headless->GetHandle();
      GetImpl(actor_surface_handle)->SetTab(tab_handle);
      CHECK(!tab_to_surface_.contains(tab_handle));
      tab_to_surface_[tab_handle] = actor_surface_handle;
      return;
    }
  }
  // The only place a surface handle may reuse a tab's raw value; removed with
  // kUseTabHandleAsSurfaceHandle. Nothing may rely on the two being equal.
  ActorSurfaceHandle actor_surface_handle =
      base::FeatureList::IsEnabled(kUseTabHandleAsSurfaceHandle)
          ? ActorSurfaceHandle(tab_handle.raw_value())
          : ActorSurfaceHandle::NextHandle();
  CHECK(!owned_surfaces_.contains(actor_surface_handle));
  CHECK(!tab_to_surface_.contains(tab_handle));
  owned_surfaces_.emplace(
      actor_surface_handle,
      std::make_unique<ActorSurfaceImpl>(actor_surface_handle, tab_handle));
  tab_to_surface_[tab_handle] = actor_surface_handle;
}

void ActorSurfaceRegistry::OnTabWillBeDestroyed(tabs::TabHandle tab) {
  auto it = tab_to_surface_.find(tab);
  if (it == tab_to_surface_.end()) {
    return;
  }
  const ActorSurfaceHandle actor_surface_handle = it->second;
  tab_to_surface_.erase(it);
  owned_surfaces_.erase(actor_surface_handle);
}

ActorSurface* ActorSurfaceRegistry::CreateHeadlessWebContents(
    base::PassKey<ActorKeyedService> pass_key) {
  return CreateForHeadless(headless_manager_->Create(), pass_key);
}

ActorSurface* ActorSurfaceRegistry::CreateForHeadless(
    content::WebContents* contents,
    base::PassKey<ActorKeyedService>) {
  CHECK(contents);
  CHECK_EQ(contents->GetBrowserContext(), headless_manager_->browser_context());
  if (ActorSurface* existing = GetForHeadless(contents)) {
    return existing;
  }
  ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::NextHandle();
  CHECK(!owned_surfaces_.contains(actor_surface_handle));
  owned_surfaces_.emplace(
      actor_surface_handle,
      std::make_unique<ActorSurfaceImpl>(actor_surface_handle, contents));
  return Get(actor_surface_handle);
}

void ActorSurfaceRegistry::OnSurfacePromoted(
    ActorSurfaceHandle actor_surface_handle) {
  ActorSurfaceImpl* surface = GetImpl(actor_surface_handle);
  CHECK(surface);

  // Promotion parents the surface's existing WebContents into a tab.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(surface->GetWebContents());
  CHECK(tab);

  if (surface->IsTab()) {
    CHECK(*surface->GetTabHandle() == tab->GetHandle());
    return;
  }

  surface->SetTab(tab->GetHandle());
  CHECK(!tab_to_surface_.contains(tab->GetHandle()));
  tab_to_surface_[tab->GetHandle()] = actor_surface_handle;
}

void ActorSurfaceRegistry::OnSurfaceWillBeDemoted(
    ActorSurfaceHandle actor_surface_handle) {
  ActorSurfaceImpl* surface = GetImpl(actor_surface_handle);
  CHECK(surface);
  CHECK(surface->IsTab());
  CHECK(!GetForHeadless(surface->GetWebContents()));

  // The WebContents outlives the tab wrapper and becomes the headless backing.
  tab_to_surface_.erase(*surface->GetTabHandle());
  surface->SetHeadless();
}

void ActorSurfaceRegistry::DestroyHeadlessActorSurface(
    ActorSurfaceHandle actor_surface_handle,
    base::PassKey<ActorKeyedService>) {
  ActorSurfaceImpl* surface = GetImpl(actor_surface_handle);
  if (!surface) {
    return;
  }
  CHECK(!surface->IsTab());
  content::WebContents* contents = surface->GetWebContents();
  // Destroy the surface before its WebContents, so it never outlives it.
  owned_surfaces_.erase(actor_surface_handle);
  headless_manager_->Destroy(contents);
}

void ActorSurfaceRegistry::OnHeadlessContentsWillBeDestroyed(
    content::WebContents* contents) {
  // Headless WebContents are only destroyed through
  // DestroyHeadlessActorSurface(), which drops the surface first. A headless
  // page can't close itself.
  CHECK(!GetForHeadless(contents));
}

base::WeakPtr<ActorSurfaceRegistry> ActorSurfaceRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

ActorSurfaceImpl* ActorSurfaceRegistry::GetImpl(
    ActorSurfaceHandle actor_surface_handle) const {
  auto it = owned_surfaces_.find(actor_surface_handle);
  return it == owned_surfaces_.end() ? nullptr : it->second.get();
}

}  // namespace actor
