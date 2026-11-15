// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_impl.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

// Attaches to a headless page the WebContents helpers that actor tools depend
// on. A tab gets these from TabFeatures. Must run before the page's first
// navigation, so the helpers see every frame and form. Helpers that already
// exist are kept.
void AttachHelpers(content::WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  CHECK(profile);

  // Same autofill and password manager setup as TabFeatures::Init().
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(contents);
  if (!autofill_client_provider.uses_platform_autofill()) {
    ChromePasswordManagerClient::CreateForWebContents(contents);
  }
}

}  // namespace

ActorSurfaceImpl::ActorSurfaceImpl(ActorSurfaceHandle actor_surface_handle,
                                   tabs::TabHandle tab)
    : actor_surface_handle_(actor_surface_handle),
      tab_(tab),
      actor_tab_data_(std::make_unique<ActorTabData>()) {
  CHECK(tab.Get());
  ActorSurfaceHandle::Register(actor_surface_handle_, this);
}

ActorSurfaceImpl::ActorSurfaceImpl(ActorSurfaceHandle actor_surface_handle,
                                   content::WebContents* headless_contents)
    : actor_surface_handle_(actor_surface_handle),
      headless_contents_(headless_contents),
      actor_tab_data_(std::make_unique<ActorTabData>()) {
  CHECK(headless_contents_);
  AttachHelpers(headless_contents_);
  ActorSurfaceHandle::Register(actor_surface_handle_, this);
}

ActorSurfaceImpl::~ActorSurfaceImpl() {
  ActorSurfaceHandle::Unregister(actor_surface_handle_);
}

ActorSurfaceHandle ActorSurfaceImpl::GetHandle() const {
  return actor_surface_handle_;
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
  CHECK(actor_tab_data_);
  return actor_tab_data_.get();
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
