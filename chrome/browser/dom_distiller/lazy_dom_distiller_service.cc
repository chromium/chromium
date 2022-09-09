// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"

namespace dom_distiller {

namespace {

// Identifies the user data on the profile.
const char kLazyDomDistillerServiceKey[] = "kLazyDomDistillerServiceKey";

}  // namespace

// static
LazyDomDistillerService* LazyDomDistillerService::Create(Profile* profile) {
  // We can't use make_unique because the constructor is private.
  LazyDomDistillerService* instance = new LazyDomDistillerService(profile);
  profile->SetUserData(&kLazyDomDistillerServiceKey,
                       base::WrapUnique(instance));
  return instance;
}

LazyDomDistillerService::~LazyDomDistillerService() = default;

std::unique_ptr<ViewerHandle> LazyDomDistillerService::ViewUrl(
    ViewRequestDelegate* delegate,
    std::unique_ptr<DistillerPage> distiller_page,
    const GURL& url) {
  return GetImpl()->ViewUrl(delegate, std::move(distiller_page), url);
}

std::unique_ptr<DistillerPage>
LazyDomDistillerService::CreateDefaultDistillerPage(
    const gfx::Size& render_view_size) {
  return GetImpl()->CreateDefaultDistillerPage(render_view_size);
}

std::unique_ptr<DistillerPage>
LazyDomDistillerService::CreateDefaultDistillerPageWithHandle(
    std::unique_ptr<SourcePageHandle> handle) {
  return GetImpl()->CreateDefaultDistillerPageWithHandle(std::move(handle));
}

DistilledPagePrefs* LazyDomDistillerService::GetDistilledPagePrefs() {
  return GetImpl()->GetDistilledPagePrefs();
}

DistillerUIHandle* LazyDomDistillerService::GetDistillerUIHandle() {
  return GetImpl()->GetDistillerUIHandle();
}

LazyDomDistillerService::LazyDomDistillerService(Profile* profile)
    : profile_(profile) {}

// This will create an object and schedule work the first time it's called
// and just return an existing object after that.
DomDistillerServiceInterface* LazyDomDistillerService::GetImpl() const {
  return DomDistillerServiceFactory::GetInstance()->GetForBrowserContext(
      profile_);
}

}  // namespace dom_distiller
