// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_impl.h"

#include "base/check.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "content/public/browser/web_contents.h"

namespace actor {

ActorSurfaceImpl::ActorSurfaceImpl(ActorSurfaceHandle handle,
                                   tabs::TabHandle tab)
    : handle_(handle), tab_(tab) {
  CHECK(tab.Get());
  ActorSurfaceHandle::Register(handle_, this);
}

ActorSurfaceImpl::ActorSurfaceImpl(ActorSurfaceHandle handle,
                                   content::WebContents* headless_contents)
    : handle_(handle), headless_contents_(headless_contents) {
  CHECK(headless_contents_);
  ActorSurfaceHandle::Register(handle_, this);
}

ActorSurfaceImpl::~ActorSurfaceImpl() {
  ActorSurfaceHandle::Unregister(handle_);
}

ActorSurfaceHandle ActorSurfaceImpl::GetHandle() const {
  return handle_;
}

bool ActorSurfaceImpl::IsTab() const {
  return tab_.has_value();
}

content::WebContents* ActorSurfaceImpl::GetWebContents() const {
  if (!IsTab()) {
    return headless_contents_;
  }
  // Resolved through the tab rather than cached: a tab's WebContents can be
  // swapped, e.g. when the tab is discarded.
  tabs::TabInterface* tab = tab_->Get();
  CHECK(tab);
  return tab->GetContents();
}

std::optional<tabs::TabHandle> ActorSurfaceImpl::GetTabHandle() const {
  return tab_;
}

ActorTabData* ActorSurfaceImpl::GetActorTabData() const {
  if (IsTab()) {
    tabs::TabInterface* tab = tab_->Get();
    CHECK(tab);
    return ActorTabData::From(tab);
  }
  // TODO(b/567721071): Implement ActorTabData for headless surfaces.
  return nullptr;
}

void ActorSurfaceImpl::SetTab(tabs::TabHandle tab) {
  tab_ = tab;
  headless_contents_ = nullptr;
}

void ActorSurfaceImpl::SetHeadless() {
  headless_contents_ = GetWebContents();
  tab_.reset();
}

}  // namespace actor
