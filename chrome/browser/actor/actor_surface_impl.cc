// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_impl.h"

#include <memory>

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
    : handle_(handle),
      headless_contents_(headless_contents),
      actor_tab_data_(std::make_unique<ActorTabData>()) {
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
  return actor_tab_data_.get();
}

void ActorSurfaceImpl::SetTab(tabs::TabHandle tab) {
  tab_ = tab;
  headless_contents_ = nullptr;
  // The tab's own `ActorTabData` takes over.
  actor_tab_data_.reset();
}

void ActorSurfaceImpl::SetHeadless() {
  headless_contents_ = GetWebContents();
  tab_.reset();
  actor_tab_data_ = std::make_unique<ActorTabData>();
}

}  // namespace actor
